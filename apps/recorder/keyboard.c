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
#include "dialog.h"

#define KBD_MARGIN 12        /* horizontal gap from the display edge to the box */
#define KBD_BTN_GAP 10       /* gap between the Cancel and OK buttons */
#define KBD_BTN_PAD_Y 5      /* vertical padding inside each button */
#define KBD_MAX_LEN 512      /* absolute cap on editable characters */
#define KBD_CHARSET_MAX 64

/* wheel acceleration: shorter gaps between wheel steps skip more characters */
#define WHEEL_FAST   (HZ/15)
#define WHEEL_VFAST  (HZ/40)

/* minimum interval between held backspace/delete repeats (slows down the
 * firmware's fast button auto-repeat so deletion isn't too quick) */
#define KBD_DEL_REPEAT (HZ/5)

struct kbd_edit {
    ucschar_t text[KBD_MAX_LEN];
    int len;
    int caret;
    int max_len;
    int scroll;          /* horizontal pixel scroll to keep the caret visible */
    long last_wheel_tick;
    long last_del_tick;  /* throttles held backspace/delete */
    bool on_buttons;     /* focus is on the Cancel/OK button row, not the input */
    bool btn_ok;         /* on the button row, OK is selected (else Cancel) */
    bool dirty;          /* text changed since the editor opened */
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

/* Box spans the display width minus a horizontal margin; height fits the
 * content (edit line + button row) and is centred within the theme content
 * area, matching the yes/no dialog. */
static void kbd_measure(struct dialog *d, struct screen *display,
                        struct viewport *box, void *data)
{
    (void)data;
    struct dialog_insets in;
    int area_y = box->y;
    int area_h = box->height;
    int sw = display->getwidth();
    int ch_h = display->getcharheight();

    dialog_get_insets(&d->style, &in);

    int bh = ch_h + 2 * KBD_BTN_PAD_Y;      /* button height */
    int gap = ch_h / 2;                     /* gap between edit line and buttons */
    int box_h = ch_h + gap + bh + in.top + in.bottom;
    if (box_h > area_h)
        box_h = area_h;

    box->x = KBD_MARGIN;
    box->width = sw - 2 * KBD_MARGIN;
    box->y = area_y + (area_h - box_h) / 2;
    box->height = box_h;
}

static void kbd_draw(struct dialog *d, struct screen *display,
                     struct viewport *content, void *data)
{
    struct kbd_edit *s = data;
    int ch_h = display->getcharheight();
    int bh = ch_h + 2 * KBD_BTN_PAD_Y;      /* button height */

    /* --- edit line in a clipped sub-viewport so long text can scroll --- */
    struct viewport tvp = *content;
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
            /* DRMODE_FG so glyphs draw over our fill without the theme
             * backdrop bleeding into each character's background */
            display->set_drawmode(DRMODE_FG);
            draw_char(display, x, 0, s->text[i]);
        }
        x += cw;
    }

    /* --- bottom Cancel / OK buttons (triggered by MENU / SELECT) --- */
    display->set_viewport(content);
    display->set_drawmode(DRMODE_SOLID);

    int by = content->height - bh;
    int bw = (content->width - KBD_BTN_GAP) / 2;
    if (bw > 0)
    {
        dialog_draw_button(display, &d->style, 0, by, bw, bh,
                           str(LANG_KBD_CANCEL),
                           s->on_buttons && !s->btn_ok);
        dialog_draw_button(display, &d->style, bw + KBD_BTN_GAP, by, bw, bh,
                           str(LANG_KBD_OK),
                           s->on_buttons && s->btn_ok);
    }

    display->set_drawmode(DRMODE_SOLID);
}

/* Confirm discarding edits (only if the text changed); returns the disposition
 * dialog_run() should act on. */
static int kbd_confirm_cancel(struct kbd_edit *s)
{
    if (s->dirty)
    {
        static const unsigned char *lines[1];
        lines[0] = (const unsigned char *)ID2P(LANG_KBD_DISCARD);
        struct text_message message = { (const char **)lines, 1 };
        if (gui_syncyesno_run(&message, NULL, NULL) != YESNO_YES)
        {
            s->on_buttons = false;   /* keep editing, back to the input */
            return DIALOG_CONTINUE;
        }
    }
    return DIALOG_CANCEL;
}

static int kbd_on_action(struct dialog *d, int action, void *data)
{
    (void)d;
    struct kbd_edit *s = data;

    switch (action)
    {
        case ACTION_KBD_DOWN:      /* wheel forward */
            if (s->on_buttons)
                s->btn_ok = !s->btn_ok;         /* toggle OK/Cancel */
            else
            {
                wheel_step(s, +1);              /* advance character */
                s->dirty = true;
            }
            break;
        case ACTION_KBD_UP:        /* wheel back */
            if (s->on_buttons)
                s->btn_ok = !s->btn_ok;
            else
            {
                wheel_step(s, -1);              /* reverse character */
                s->dirty = true;
            }
            break;
        case ACTION_KBD_LEFT:      /* tap left */
            if (s->on_buttons)
                s->btn_ok = false;              /* Cancel (left button) */
            else
                caret_left(s);
            break;
        case ACTION_KBD_RIGHT:     /* tap right */
            if (s->on_buttons)
                s->btn_ok = true;               /* OK (right button) */
            else
            {
                int before = s->len;
                caret_right(s);                 /* may append a space */
                if (s->len != before)
                    s->dirty = true;
            }
            break;
        case ACTION_KBD_BACKSPACE: /* hold left -> backspace (throttled) */
            if (!s->on_buttons &&
                current_tick - s->last_del_tick >= KBD_DEL_REPEAT)
            {
                do_backspace(s);
                s->dirty = true;
                s->last_del_tick = current_tick;
            }
            break;
        case ACTION_KBD_DELETE:    /* hold right -> delete under caret (throttled) */
            if (!s->on_buttons &&
                current_tick - s->last_del_tick >= KBD_DEL_REPEAT)
            {
                do_delete(s);
                s->dirty = true;
                s->last_del_tick = current_tick;
            }
            break;
        case ACTION_KBD_DONE:      /* PLAY: move focus down to the buttons */
            if (!s->on_buttons)
            {
                s->on_buttons = true;
                s->btn_ok = true;               /* start on OK */
            }
            break;
        case ACTION_KBD_SELECT:    /* accept, or confirm the chosen button */
            if (s->on_buttons && !s->btn_ok)
                return kbd_confirm_cancel(s);   /* Cancel button */
            return DIALOG_ACCEPT;               /* OK / input shortcut */
        case ACTION_KBD_ABORT:     /* MENU: up to the input, or cancel */
            if (s->on_buttons)
            {
                s->on_buttons = false;          /* back up to the input */
                break;
            }
            return kbd_confirm_cancel(s);
        default:
            if (default_event_handler(action) == SYS_USB_CONNECTED)
                return DIALOG_ABORT;
            break;
    }
    return DIALOG_CONTINUE;
}

/* Click-wheel single-line text editor. Returns 0 on accept (buffer updated),
 * -1 on cancel or USB (buffer left unchanged). */
int dialog_input(char* text, int buflen)
{
    static const struct dialog_callbacks cb = {
        .measure   = kbd_measure,
        .draw      = kbd_draw,
        .on_action = kbd_on_action,
    };
    struct dialog d;

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
    st.last_del_tick = 0;
    st.on_buttons = false;   /* focus starts on the input line */
    st.btn_ok = true;
    st.dirty = false;
    ensure_nonempty(&st);

    dialog_init(&d, CONTEXT_KEYBOARD, NULL, NULL, &cb, &st);
    if (dialog_run(&d, TIMEOUT_BLOCK) != DIALOG_ACCEPT)
        return -1;           /* cancelled or USB */

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
    return 0;
}

/* Plugin-ABI wrapper: the loadable-layout `kbd` argument is ignored. */
int kbd_input(char* text, int buflen, ucschar_t *kbd)
{
    (void)kbd;
    return dialog_input(text, buflen);
}
