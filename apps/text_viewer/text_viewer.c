/***************************************************************************
 *             __________               __   ___.
 *   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
 *   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
 *   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
 *   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \
 *                     \/            \/     \/    \/            \/
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This software is distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY
 * KIND, either express or implied.
 *
 ****************************************************************************/

/* Core text viewer.
 *
 * The ts_* engine (txt_source.h) is a one-way stream: ts_read() goes forward
 * and ts_rewind() goes back to the start, with nothing in between. Rather
 * than extract whole documents up front -- which would scale with the file
 * and fight the audio buffer for RAM -- this keeps a fixed sliding window of
 * recently extracted text. Reading is overwhelmingly linear and paging back
 * almost always means "the page I just read", so the window absorbs
 * essentially all backward navigation at a flat cost. Only a jump behind the
 * window falls back to a rewind and re-extract.
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>          /* atoi */
#include <string.h>
#include "config.h"
#include "file.h"            /* off_t, open/close/rename/remove, fdprintf */
#include "rbpaths.h"         /* ROCKBOX_DIR */
#include "action.h"
#include "screen_access.h"   /* screens[] */
#include "kernel.h"          /* HZ, SYS_USB_CONNECTED */
#include "core_alloc.h"
#include "settings.h"
#include "lang.h"
#include "splash.h"
#include "viewport.h"
#include "font.h"
#include "misc.h"            /* push_current_activity, read_line */
#include "root_menu.h"       /* GO_TO_* */
#include "txt_source.h"
#include "ts_io_core.h"
#include "text_viewer.h"

/* Extracted text kept resident, and how much of it sits behind the current
 * page. The margin is the backward reach: at a typical few KiB per page it
 * covers dozens of pages, which is more than enough for the way people
 * actually read. What is left over is forward headroom, refilled a page at a
 * time, so the window compacts only every ~64 KiB of reading. */
#define TV_WINDOW_SIZE  (256 * 1024)
#define TV_BACK_MARGIN  (192 * 1024)

/* Upper bound on what one page can consume. Used to decide how far ahead the
 * window must reach before laying a page out. */
#define TV_PAGE_MAX     (8 * 1024)

/* Page starts visited going forward, so paging back is a pop rather than a
 * re-layout from the start of the document (the only other known page
 * boundary). At a few KiB per page this covers documents far larger than
 * anything these targets will open; past it, paging back simply stops. */
#define TV_MAX_PAGES    8192

/* Most recently read documents and where the reader got to, newest first. */
#define TV_RESUME_FILE  ROCKBOX_DIR "/textviewer.dat"
#define TV_RESUME_MAX   16

struct tv_state
{
    struct ts_core_file file;
    ts_source *src;

    int arena_handle;
    int window_handle;
    int stack_handle;

    /* window holds extracted bytes [win_start, win_start + win_len) */
    unsigned char *win;
    off_t  win_start;
    size_t win_len;
    bool   eof;              /* extraction reached the end of the document */

    off_t  file_size;        /* of the document on disk, for resume staleness */

    off_t  pos;              /* first byte of the page on screen */
    off_t  next;             /* first byte of the page after it */

    off_t *stack;            /* page starts behind `pos`, oldest first */
    int    sp;

    int font;
    int line_height;
    struct viewport vp;
};

static struct tv_state tv;

/* ---- window ---------------------------------------------------------- */

/* Drops text that has fallen behind the backward reach, sliding what is left
 * to the front. Only called when the window is out of forward headroom. */
static void tv_compact(void)
{
    off_t keep_from = tv.pos - TV_BACK_MARGIN;
    size_t drop;

    if (keep_from <= tv.win_start)
        return;                              /* nothing droppable */

    drop = (size_t)(keep_from - tv.win_start);
    if (drop > tv.win_len)
        drop = tv.win_len;

    memmove(tv.win, tv.win + drop, tv.win_len - drop);
    tv.win_start += drop;
    tv.win_len   -= drop;
}

/* Pulls more text from the engine onto the end of the window. Returns false
 * at end of document or on error. */
static bool tv_extract_more(void)
{
    size_t room;
    long got;

    if (tv.eof)
        return false;

    if (TV_WINDOW_SIZE - tv.win_len < TV_PAGE_MAX)
        tv_compact();

    room = TV_WINDOW_SIZE - tv.win_len;
    if (room < 8)                            /* ts_read() needs at least 8 */
        return false;

    got = ts_read(tv.src, (char *)tv.win + tv.win_len, room);
    if (got <= 0)
    {
        tv.eof = true;                       /* clean EOF and errors alike:
                                              * stop reading either way */
        return false;
    }

    tv.win_len += (size_t)got;
    return true;
}

/* Makes sure [from, from + len) is resident, extracting forward as needed.
 * Returns false if `from` has already fallen out the back of the window --
 * the caller must then rewind and re-extract. */
static bool tv_ensure(off_t from, size_t len)
{
    if (from < tv.win_start)
        return false;

    while (tv.win_start + (off_t)tv.win_len < from + (off_t)len)
    {
        if (!tv_extract_more())
            break;                           /* end of document: all there is */
    }
    return true;
}

/* ---- layout ----------------------------------------------------------- */

/* Width of the `n` bytes at `s`.
 *
 * font_getstringnsize()'s "maxbytes" is really a codepoint count -- it
 * decrements once per utf8decode() and leans on unsigned underflow to stop
 * (firmware/font.c) -- and it halts at a NUL either way, so it cannot bound a
 * byte range. The window is ours and single-threaded, so a temporary NUL is
 * both simpler and cheaper than counting codepoints first. The window is
 * allocated with a spare byte so s[n] is always in bounds. */
static int tv_width(unsigned char *s, size_t n)
{
    unsigned char save = s[n];
    int w;

    s[n] = '\0';
    font_getstringsize(s, &w, NULL, tv.font);
    s[n] = save;
    return w;
}

/* Bytes of `s` that fit in `width`, breaking at the last space. `*draw` gets
 * the length to render, excluding the break character; the return value is
 * what the line consumes, including it. Rockbox bitmap fonts have no kerning
 * -- font_getstringnsize() just sums per-glyph widths -- so accumulating word
 * widths is exact and avoids re-measuring the line prefix per word. */
static size_t tv_line_break(unsigned char *s, size_t avail, int width,
                            size_t *draw)
{
    size_t i = 0;
    int used = 0;

    if (avail == 0)
    {
        *draw = 0;
        return 0;
    }

    if (s[0] == '\n')                        /* explicit blank line */
    {
        *draw = 0;
        return 1;
    }

    while (i < avail)
    {
        size_t word_end = i;
        int word_w;

        while (word_end < avail && s[word_end] != ' ' && s[word_end] != '\n')
            word_end++;

        word_w = tv_width(s + i, word_end - i);

        if (used + word_w > width)
        {
            if (i == 0)
            {
                /* A single word too wide for the line: break inside it at the
                 * last codepoint that fits, never mid-sequence. */
                size_t j = 0;
                int w = 0;

                while (j < word_end)
                {
                    size_t k = j + 1;
                    int cw;

                    while (k < word_end && (s[k] & 0xc0) == 0x80)
                        k++;                 /* to the next codepoint start */

                    cw = tv_width(s + j, k - j);
                    if (w + cw > width && j > 0)
                        break;
                    w += cw;
                    j = k;
                }
                *draw = j;
                return j;
            }
            break;                           /* line ends before this word */
        }

        used += word_w;
        i = word_end;

        if (i >= avail)
            break;

        if (s[i] == '\n')                    /* hard break ends the line */
        {
            *draw = i;
            return i + 1;
        }

        /* s[i] is a space: it only stays if a word follows it on this line */
        used += tv_width(s + i, 1);
        i++;
    }

    /* Trim the trailing space from what is drawn, but still consume it. */
    *draw = (i > 0 && s[i - 1] == ' ') ? i - 1 : i;
    return i;
}

/* Lays out one page from `start`, drawing it when `render` is set. Returns the
 * offset of the next page. */
static off_t tv_page(off_t start, bool render)
{
    off_t off = start;
    int y = 0;

    while (y + tv.line_height <= tv.vp.height)
    {
        unsigned char *s;
        size_t avail, draw, used;

        if (!tv_ensure(off, TV_PAGE_MAX))
            break;

        avail = (size_t)(tv.win_start + (off_t)tv.win_len - off);
        if (avail == 0)
            break;                           /* end of document */

        s = tv.win + (off - tv.win_start);
        used = tv_line_break(s, avail, tv.vp.width, &draw);
        if (used == 0)
            break;

        if (render && draw > 0)
        {
            unsigned char save = s[draw];

            s[draw] = '\0';
            screens[SCREEN_MAIN].putsxy(0, y, s);
            s[draw] = save;
        }

        off += used;
        y += tv.line_height;
    }

    return off;
}

static void tv_draw(void)
{
    struct screen *display = &screens[SCREEN_MAIN];
    struct viewport *last;

    last = display->set_viewport(&tv.vp);
    display->clear_viewport();
    tv.next = tv_page(tv.pos, true);
    display->update_viewport();
    display->set_viewport(last);
}

/* ---- navigation ------------------------------------------------------- */

/* True once the page on screen is the last one: nothing left to extract, and
 * the page after it starts at or past the final byte. */
static bool tv_at_end(void)
{
    return tv.eof && tv.next >= tv.win_start + (off_t)tv.win_len;
}

/* Brings `target` into the window and makes it the current page. The engine
 * only rewinds to the start, so reaching a page that has fallen out the back
 * of the window means re-extracting from byte zero. */
static bool tv_reach(off_t target)
{
    if (target < tv.win_start)
    {
        splash(0, ID2P(LANG_WAIT));
        if (ts_rewind(tv.src) != TS_OK)
            return false;
        tv.win_start = 0;
        tv.win_len = 0;
        tv.eof = false;
    }

    /* Set first: compaction keeps its margin behind tv.pos, so this is what
     * stops the text we are heading for being discarded on the way. */
    tv.pos = target;
    tv_ensure(target, TV_PAGE_MAX);
    return true;
}

static void tv_next_page(void)
{
    if (tv_at_end())
        return;

    if (tv.sp < TV_MAX_PAGES)
        tv.stack[tv.sp++] = tv.pos;
    tv.pos = tv.next;
    tv_draw();
}

static void tv_prev_page(void)
{
    if (tv.sp == 0)
        return;                              /* first page */

    if (!tv_reach(tv.stack[tv.sp - 1]))
        return;
    tv.sp--;
    tv_draw();
}

/* Pages forward from the start without drawing until `target` falls on the
 * current page. Doubles as the stack rebuild: resuming leaves the history
 * exactly as it would have been had the reader paged there by hand. */
static void tv_resume_to(off_t target)
{
    splash(0, ID2P(LANG_WAIT));

    while (1)
    {
        off_t next = tv_page(tv.pos, false);

        if (next <= tv.pos || next > target)
            break;                           /* end of document, or target is
                                              * on the page we are on */
        if (tv.sp < TV_MAX_PAGES)
            tv.stack[tv.sp++] = tv.pos;
        tv.pos = next;
    }
}

/* ---- resume ----------------------------------------------------------- */

/* Lines are "<offset> <size> <path>", newest first. The path trails so it may
 * hold spaces. `size` is the document's length when the offset was recorded:
 * offsets index the extracted text, so a document that has changed on disk
 * invalidates them, and the length is the cheap half of noticing. (It cannot
 * catch an edit that preserves the length, nor a layout change -- page
 * boundaries also move with the font and spacing.) */
static off_t tv_resume_load(const char *file, off_t size)
{
    int fd = open(TV_RESUME_FILE, O_RDONLY);
    char line[MAX_PATH + 32];
    off_t off = 0;

    if (fd < 0)
        return 0;

    while (read_line(fd, line, sizeof line) > 0)
    {
        char *end_off = strchr(line, ' ');
        char *end_size;

        if (!end_off)
            continue;
        end_size = strchr(end_off + 1, ' ');
        if (!end_size)
            continue;

        if (strcmp(end_size + 1, file))
            continue;

        *end_off = *end_size = '\0';
        if ((off_t)atoi(end_off + 1) == size)
            off = (off_t)atoi(line);
        break;                               /* first hit is the newest */
    }

    close(fd);
    return off;
}

/* Rewrites the list with `file` at the front, streaming the old entries past
 * a single line buffer rather than holding them all on the stack. */
static void tv_resume_save(const char *file, off_t off, off_t size)
{
    char line[MAX_PATH + 32];
    int old, new_fd, kept = 1;

    new_fd = open(TV_RESUME_FILE ".tmp", O_CREAT|O_WRONLY|O_TRUNC, 0666);
    if (new_fd < 0)
        return;

    fdprintf(new_fd, "%ld %ld %s\n", (long)off, (long)size, file);

    old = open(TV_RESUME_FILE, O_RDONLY);
    if (old >= 0)
    {
        while (kept < TV_RESUME_MAX && read_line(old, line, sizeof line) > 0)
        {
            char *end_off = strchr(line, ' ');
            char *end_size;

            if (!end_off)
                continue;
            end_size = strchr(end_off + 1, ' ');
            if (!end_size)
                continue;
            if (!strcmp(end_size + 1, file))
                continue;                    /* superseded by the new entry */

            fdprintf(new_fd, "%s\n", line);
            kept++;
        }
        close(old);
    }
    close(new_fd);

    remove(TV_RESUME_FILE);
    rename(TV_RESUME_FILE ".tmp", TV_RESUME_FILE);
}

/* ---- lifecycle -------------------------------------------------------- */

static bool tv_open(const char *file)
{
    ts_config cfg = ts_config_default();
    ts_io io;
    void *arena;
    int rc;

    tv.arena_handle = tv.window_handle = tv.stack_handle = 0;
    tv.src = NULL;

    /* Every stage of the engine holds pointers into the arena and to its
     * neighbours, so it must not move underneath an open source. */
    tv.arena_handle = core_alloc(TS_ARENA_RECOMMENDED);
    if (tv.arena_handle <= 0)
        return false;
    core_pin(tv.arena_handle);
    arena = core_get_data(tv.arena_handle);

    /* One spare byte: tv_width() and tv_draw() NUL-terminate in place, and
     * that write lands one past the end when the window is full. */
    tv.window_handle = core_alloc(TV_WINDOW_SIZE + 1);
    if (tv.window_handle <= 0)
        return false;
    core_pin(tv.window_handle);
    tv.win = core_get_data(tv.window_handle);

    tv.stack_handle = core_alloc(TV_MAX_PAGES * sizeof(off_t));
    if (tv.stack_handle <= 0)
        return false;
    core_pin(tv.stack_handle);
    tv.stack = core_get_data(tv.stack_handle);
    tv.sp = 0;

    if (ts_io_core(&io, &tv.file, file) != TS_OK)
        return false;

    /* Through the vtable rather than filesize(): coreio_size() restores the
     * descriptor's position and keeps its cached copy honest. */
    tv.file_size = io.size(io.ctx);

    rc = ts_open(&tv.src, &io, file, arena, TS_ARENA_RECOMMENDED, &cfg);
    if (rc != TS_OK)
    {
        /* ts_open() takes the fd on success only, so close it ourselves. */
        io.close(io.ctx);
        splashf(HZ * 2, "%s", ts_strerror(rc));
        return false;
    }

    tv.win_start = 0;
    tv.win_len = 0;
    tv.eof = false;
    tv.pos = 0;
    tv.next = 0;
    return true;
}

static void tv_close(void)
{
    if (tv.src)
        ts_close(tv.src);
    tv.src = NULL;

    if (tv.stack_handle > 0)
    {
        core_unpin(tv.stack_handle);
        tv.stack_handle = core_free(tv.stack_handle);
    }
    if (tv.window_handle > 0)
    {
        core_unpin(tv.window_handle);
        tv.window_handle = core_free(tv.window_handle);
    }
    if (tv.arena_handle > 0)
    {
        core_unpin(tv.arena_handle);
        tv.arena_handle = core_free(tv.arena_handle);
    }
    tv.win = NULL;
    tv.stack = NULL;
}

static void tv_setup_screen(void)
{
    /* Fills tv.vp with the themed text area (below the status bar) and its
     * colours -- see viewport_set_defaults(). */
    viewportmanager_theme_enable(SCREEN_MAIN, true, &tv.vp);

    tv.font = FONT_UI;
    tv.vp.font = tv.font;
    tv.line_height = font_get(tv.font)->height;
}

int text_viewer(const char *file)
{
    int ret = GO_TO_PREVIOUS;
    off_t resume;

    push_current_activity(ACTIVITY_TEXTVIEWER);
    tv_setup_screen();

    if (!tv_open(file))
    {
        tv_close();
        viewportmanager_theme_undo(SCREEN_MAIN, false);
        pop_current_activity();
        return GO_TO_PREVIOUS;
    }

    resume = tv_resume_load(file, tv.file_size);
    if (resume > 0)
        tv_resume_to(resume);

    tv_draw();

    while (1)
    {
        int action = get_action(CONTEXT_STD, TIMEOUT_BLOCK);

        switch (action)
        {
            case ACTION_STD_NEXT:
            case ACTION_STD_NEXTREPEAT:
                tv_next_page();
                break;

            case ACTION_STD_PREV:
            case ACTION_STD_PREVREPEAT:
                tv_prev_page();
                break;

            case ACTION_STD_CANCEL:
                goto done;

            default:
                if (default_event_handler(action) == SYS_USB_CONNECTED)
                {
                    ret = GO_TO_ROOT;
                    goto done;
                }
                break;
        }
    }

  done:
    tv_resume_save(file, tv.pos, tv.file_size);
    tv_close();
    viewportmanager_theme_undo(SCREEN_MAIN, false);
    pop_current_activity();
    return ret;
}
