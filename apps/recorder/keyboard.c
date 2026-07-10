/***************************************************************************
 *             __________               __   ___.
 *   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
 *   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
 *   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
 *   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \
 *                     \/            \/     \/    \/            \/
 * $Id$
 *
 * Copyright (C) 2002 by Björn Stenberg
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

/* Click-wheel oriented single-line text editor.
 *
 * This replaces the old on-screen virtual keyboard with a one-line editor
 * driven entirely by the click wheel: the wheel cycles the character under
 * the caret, Left/Right move the caret (holding them backspaces/deletes),
 * SELECT accepts and MENU cancels. See the iPod click-wheel text input
 * specification. This is an iPod-only build, so the old keyboard's loadable
 * layouts, morse input and touchscreen grid are intentionally gone. */

#include "config.h"
#include <stdio.h>
#include <string.h>
#include "kernel.h"
#include "system.h"
#include "font.h"
#include "screens.h"
#include "settings.h"
#include "misc.h"
#include "rbunicode.h"
#include "action.h"
#include "lang.h"
#include "keyboard.h"
#include "viewport.h"
#include "splash.h"
#include "yesno.h"
#include "skin_engine/skin_engine.h" /* skin_render_inhibit_flush */

#define KBD_MARGIN 12        /* horizontal gap from the display edge to the box */
#define KBD_PAD    10        /* inner padding inside the box */
#define KBD_MAX_LEN 512      /* absolute cap on editable characters */
#define KBD_CHARSET_MAX 64

/* wheel acceleration: shorter gaps between wheel steps skip more characters */
#define WHEEL_FAST   (HZ/15)
#define WHEEL_VFAST  (HZ/40)

struct kbd_edit {
    ucschar_t text[KBD_MAX_LEN];
    int len;
    int caret;
    int max_len;
    int scroll;          /* horizontal pixel scroll to keep the caret visible */
    long last_wheel_tick;
};

/* Not reentrant (there is only ever one text-input screen at a time); kept
 * out of the stack so plugin callers with small stacks are safe. */
static struct kbd_edit st;

/* The default (fallback) character cycle, used if the localized charset
 * string is missing or empty. Space first, then A-Z, 0-9 and a little
 * punctuation -- no lowercase (see the spec). */
static const ucschar_t default_charset[] = {
    ' ','A','B','C','D','E','F','G','H','I','J','K','L','M','N','O','P',
    'Q','R','S','T','U','V','W','X','Y','Z','0','1','2','3','4','5','6',
    '7','8','9','-','_','.','\'','&'
};

static ucschar_t charset[KBD_CHARSET_MAX];
static int charset_len;

/* '*kbd' (loadable layouts) no longer applies to the click-wheel editor;
 * kept so callers in filetree.c / settings.c still link and succeed. */
int load_kbd(unsigned char* filename)
{
    (void)filename;
    return 0;
}

static int charset_index(ucschar_t ch)
{
    for (int i = 0; i < charset_len; i++)
        if (charset[i] == ch)
            return i;
    return -1;
}

static void build_charset(void)
{
    /* Derive the wheel's character cycle from a localized string so
     * translations can supply their own; fall back to the built-in set. */
    const unsigned char *p = (const unsigned char *)str(LANG_KBD_CLICKWHEEL_CHARSET);
    charset_len = 0;
    while (p && *p && charset_len < KBD_CHARSET_MAX)
    {
        ucschar_t ch;
        p = utf8decode(p, &ch);
        if (ch == 0)
            break;
        charset[charset_len++] = ch;
    }
    if (charset_len == 0)
    {
        int n = (int)(sizeof(default_charset) / sizeof(default_charset[0]));
        for (charset_len = 0; charset_len < n; charset_len++)
            charset[charset_len] = default_charset[charset_len];
    }
    /* space must be in the cycle: it is the empty-string / blank character */
    if (charset_index(' ') < 0 && charset_len < KBD_CHARSET_MAX)
        charset[charset_len++] = ' ';
}

static void wheel_step(struct kbd_edit *s, int dir)
{
    /* dir: +1 advance, -1 reverse. Acceleration: fast spins skip characters. */
    long now = current_tick;
    long dt = now - s->last_wheel_tick;
    s->last_wheel_tick = now;

    int mag = 1;
    if (dt < WHEEL_VFAST)
        mag = 4;
    else if (dt < WHEEL_FAST)
        mag = 2;

    int idx = charset_index(s->text[s->caret]);
    if (idx < 0)
        idx = 0;
    idx = (idx + dir * mag) % charset_len;
    if (idx < 0)
        idx += charset_len;
    s->text[s->caret] = charset[idx];
}

static void ensure_nonempty(struct kbd_edit *s)
{
    if (s->len <= 0)
    {
        s->text[0] = ' ';
        s->len = 1;
    }
    if (s->caret >= s->len)
        s->caret = s->len - 1;
    if (s->caret < 0)
        s->caret = 0;
}

static void caret_left(struct kbd_edit *s)
{
    if (s->caret > 0)
        s->caret--;
}

static void caret_right(struct kbd_edit *s)
{
    if (s->caret < s->len - 1)
        s->caret++;
    else if (s->len < s->max_len)
    {
        s->text[s->len++] = ' ';   /* grow the line with a fresh space */
        s->caret++;
    }
    /* else: at maximum length, Right no longer appends */
}

static void do_backspace(struct kbd_edit *s)
{
    /* delete the character before the caret; everything shifts left */
    if (s->caret <= 0)
        return;
    for (int i = s->caret - 1; i < s->len - 1; i++)
        s->text[i] = s->text[i + 1];
    s->len--;
    s->caret--;
    ensure_nonempty(s);
}

static void do_delete(struct kbd_edit *s)
{
    /* delete the character under the caret; everything shifts left */
    if (s->len <= 0)
        return;
    for (int i = s->caret; i < s->len - 1; i++)
        s->text[i] = s->text[i + 1];
    s->len--;
    ensure_nonempty(s);
}

static int uc_width(struct screen *display, ucschar_t ch)
{
    unsigned char tmp[8];
    unsigned char *e = utf8encode(ch, tmp);
    *e = '\0';
    int w;
    display->getstringsize((char *)tmp, &w, NULL);
    return w;
}

static void draw_char(struct screen *display, int x, int y, ucschar_t ch)
{
    unsigned char tmp[8];
    unsigned char *e = utf8encode(ch, tmp);
    *e = '\0';
    display->putsxy(x, y, (char *)tmp);
}

/* Draw a static (non-selectable) labelled button outline. */
static void draw_hint_button(struct screen *display, int x, int y,
                             int w, int h, const char *label)
{
    int lw, lh;
    display->getstringsize(label, &lw, &lh);
    display->set_drawmode(DRMODE_SOLID);
    display->drawrect(x, y, w, h);
    display->set_drawmode(DRMODE_FG);
    display->putsxy(x + (w - lw) / 2, y + (h - lh) / 2, label);
}

static void kbd_draw(struct screen *display, struct viewport *vp,
                     struct kbd_edit *s)
{
    int sw = display->getwidth();
    int sh = display->getheight();

    /* Box spans the display width minus a horizontal margin; height fits the
     * content (edit line + button row) and is centred vertically, matching the
     * yes/no dialog. Inherits the theme's colours/font from vp. */
    struct viewport box = *vp;
    box.x = KBD_MARGIN;
    box.width = sw - 2 * KBD_MARGIN;
    box.y = 0;
    box.height = sh;
    struct viewport *last_vp = display->set_viewport(&box);
    int ch_h = display->getcharheight();

    int bh = ch_h + 2 * 5;                  /* button height */
    int gap = ch_h / 2;                     /* gap between edit line and buttons */
    int box_h = ch_h + gap + bh + 2 * KBD_PAD;
    if (box_h > sh - 2 * KBD_MARGIN)
        box_h = sh - 2 * KBD_MARGIN;
    box.y = (sh - box_h) / 2;
    box.height = box_h;
    display->set_viewport(&box);

    /* clear and frame the dialog like a window */
    display->set_drawmode(DRMODE_SOLID | DRMODE_INVERSEVID);
    display->fillrect(0, 0, box.width, box.height);
    display->set_drawmode(DRMODE_SOLID);
    display->drawrect(0, 0, box.width, box.height);

    /* --- edit line in a clipped sub-viewport so long text can scroll --- */
    struct viewport tvp = box;
    tvp.x += KBD_PAD;
    tvp.width -= 2 * KBD_PAD;
    tvp.y += KBD_PAD;
    tvp.height = ch_h;
    display->set_viewport(&tvp);

    /* caret x (absolute from text start) and keep it visible */
    int caret_x = 0;
    for (int i = 0; i < s->caret; i++)
        caret_x += uc_width(display, s->text[i]);
    int caret_w = uc_width(display, s->text[s->caret]);
    if (caret_x < s->scroll)
        s->scroll = caret_x;
    if (caret_x + caret_w > s->scroll + tvp.width)
        s->scroll = caret_x + caret_w - tvp.width;
    if (s->scroll < 0)
        s->scroll = 0;

    int x = -s->scroll;
    for (int i = 0; i < s->len; i++)
    {
        int cw = uc_width(display, s->text[i]);
        if (i == s->caret)
        {
            /* caret: inverse-video block */
            display->set_drawmode(DRMODE_FG);
            display->fillrect(x, 0, cw, tvp.height);
            display->set_drawmode(DRMODE_SOLID | DRMODE_INVERSEVID);
            draw_char(display, x, 0, s->text[i]);
        }
        else
        {
            display->set_drawmode(DRMODE_FG);
            draw_char(display, x, 0, s->text[i]);
        }
        x += cw;
    }

    /* --- bottom Cancel / OK buttons (triggered by MENU / SELECT) --- */
    display->set_viewport(&box);
    display->set_drawmode(DRMODE_SOLID);

    int by = box.height - bh - KBD_PAD;
    int bw = (box.width - 3 * KBD_PAD) / 2;
    if (bw > 0)
    {
        draw_hint_button(display, KBD_PAD, by, bw, bh, str(LANG_KBD_CANCEL));
        draw_hint_button(display, KBD_PAD * 2 + bw, by, bw, bh,
                         str(LANG_KBD_OK));
    }

    display->set_drawmode(DRMODE_SOLID);
    display->update_viewport();
    display->set_viewport(last_vp);
}

int kbd_input(char* text, int buflen, ucschar_t *kbd)
{
    (void)kbd;   /* loadable layouts are not used by the click-wheel editor */
    struct screen *display = &screens[SCREEN_MAIN];
    struct viewport vp;
    bool dirty = false;
    int ret = 0;

    build_charset();

    st.max_len = buflen - 1;
    if (st.max_len > KBD_MAX_LEN - 1)
        st.max_len = KBD_MAX_LEN - 1;
    if (st.max_len < 1)
        st.max_len = 1;

    /* decode the incoming UTF-8 into codepoints */
    st.len = 0;
    const unsigned char *p = (const unsigned char *)text;
    while (*p && st.len < st.max_len)
    {
        ucschar_t ch;
        p = utf8decode(p, &ch);
        if (ch == 0)
            break;
        st.text[st.len++] = ch;
    }
    /* Append one editable space so there is always a caret position: for
     * existing text this puts the caret at the end (the trailing space is
     * trimmed on accept), and for empty input it is the single blank char
     * the wheel immediately edits. */
    if (st.len < st.max_len)
        st.text[st.len++] = ' ';
    st.caret = st.len - 1;
    st.scroll = 0;
    st.last_wheel_tick = current_tick;
    ensure_nonempty(&st);

    action_wait_for_release();
    viewportmanager_theme_enable(SCREEN_MAIN, true, &vp);

    while (true)
    {
        kbd_draw(display, &vp, &st);

        /* Inhibit the SBS's own flush during input so the theme's status-bar
         * skin doesn't overdraw the editor between keystrokes (flicker) --
         * same pattern as the list and album-covers screens. */
        skin_render_inhibit_flush(true);
        int action = get_action(CONTEXT_KEYBOARD, TIMEOUT_BLOCK);
        skin_render_inhibit_flush(false);
        switch (action)
        {
            case ACTION_KBD_DOWN:      /* wheel forward -> advance character */
                wheel_step(&st, +1);
                dirty = true;
                break;
            case ACTION_KBD_UP:        /* wheel back -> reverse character */
                wheel_step(&st, -1);
                dirty = true;
                break;
            case ACTION_KBD_LEFT:      /* tap left -> caret left */
                caret_left(&st);
                break;
            case ACTION_KBD_RIGHT:     /* tap right -> caret right (may append) */
            {
                int before = st.len;
                caret_right(&st);
                if (st.len != before)
                    dirty = true;
                break;
            }
            case ACTION_KBD_BACKSPACE: /* hold left -> backspace */
                do_backspace(&st);
                dirty = true;
                break;
            case ACTION_KBD_DELETE:    /* hold right -> delete under caret */
                do_delete(&st);
                dirty = true;
                break;
            case ACTION_KBD_SELECT:    /* accept */
            case ACTION_KBD_DONE:
                goto accept;
            case ACTION_KBD_ABORT:     /* cancel (prompt if changed) */
                if (dirty)
                {
                    static const unsigned char *lines[1];
                    lines[0] = (const unsigned char *)ID2P(LANG_KBD_DISCARD);
                    struct text_message message = { (const char **)lines, 1 };
                    if (gui_syncyesno_run(&message, NULL, NULL) != YESNO_YES)
                        break;   /* keep editing */
                }
                ret = -1;
                goto done;
            default:
                if (default_event_handler(action) == SYS_USB_CONNECTED)
                {
                    ret = -1;
                    goto done;
                }
                break;
        }
    }

accept:
    /* trim leading and trailing spaces, then re-encode into the caller buffer */
    while (st.len > 0 && st.text[st.len - 1] == ' ')
        st.len--;
    int trim_start = 0;
    while (trim_start < st.len && st.text[trim_start] == ' ')
        trim_start++;
    {
        unsigned char *out = (unsigned char *)text;
        unsigned char *end = out + buflen - 1;
        for (int i = trim_start; i < st.len; i++)
        {
            unsigned char tmp[8];
            unsigned char *e = utf8encode(st.text[i], tmp);
            int n = e - tmp;
            if (out + n > end)
                break;   /* no room left in the caller's buffer */
            memcpy(out, tmp, n);
            out += n;
        }
        *out = '\0';
    }
    ret = 0;

done:
    viewportmanager_theme_undo(SCREEN_MAIN, false);
    return ret;
}
