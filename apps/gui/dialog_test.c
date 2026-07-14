/***************************************************************************
 *             __________               __   ___.
 *   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
 *   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
 *   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
 *   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \
 *                     \/            \/     \/    \/            \/
 * $Id$
 *
 * Copyright (C) 2026 by the Rockbox project
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

/* Interactive walkthrough of the dialog widgets - see dialog_test.h. */

#include "config.h"
#include "system.h"
#include "kernel.h"
#include "lcd.h"
#include "screen_access.h"
#include "splash.h"
#include "yesno.h"
#include "keyboard.h"
#include "dialog.h"
#include "dialog_test.h"

#ifdef HAVE_LCD_COLOR
/* A synthetic icon (a filled disc) for the styled dialogs below, so the icon's
 * extent and the text column inset beside it are both obvious. */
#define TEST_ICON_SIZE 20
static fb_data test_icon_data[TEST_ICON_SIZE * TEST_ICON_SIZE];
static struct bitmap test_icon = {
    .width       = TEST_ICON_SIZE,
    .height      = TEST_ICON_SIZE,
    .format      = FORMAT_NATIVE,
    .maskdata    = NULL,
    .alpha_offset = 0,
    .data        = (unsigned char *)test_icon_data,
};

static void build_test_icon(void)
{
    const int r = TEST_ICON_SIZE / 2;
    for (int y = 0; y < TEST_ICON_SIZE; y++)
    {
        for (int x = 0; x < TEST_ICON_SIZE; x++)
        {
            int dx = x - r, dy = y - r;
            bool inside = (dx * dx + dy * dy) <= r * r;
            test_icon_data[y * TEST_ICON_SIZE + x] =
                inside ? LCD_RGBPACK(255, 80, 40) : TRANSPARENT_COLOR;
        }
    }
}
#endif /* HAVE_LCD_COLOR */

static const char *res_name(enum yesno_res r)
{
    switch (r)
    {
        case YESNO_YES: return "Result: YES";
        case YESNO_NO:  return "Result: NO";
        case YESNO_TMO: return "Result: TIMEOUT";
        case YESNO_USB: return "Result: USB";
    }
    return "Result: ?";
}

/* Draw full-screen centre guide lines, so the following popup can be checked
 * for centring - its box should straddle the crosshair symmetrically. Only the
 * main screen is instrumented (the crosshair). */
static void draw_crosshair(void)
{
    struct screen *s = &screens[SCREEN_MAIN];
    int w = s->getwidth();
    int h = s->getheight();

    s->clear_display();
    s->set_drawmode(DRMODE_SOLID);
    s->vline(w / 2, 0, h - 1);
    s->hline(0, w - 1, h / 2);
    s->update();
}

int dialog_test_run(void)
{
    /* 1. centring check: crosshair + short popup */
    draw_crosshair();
    splashf(3 * HZ, "1/9 centring: box should straddle the crosshair");

    /* 2. multi-line popup: word wrap + centring of a taller box */
    splashf(3 * HZ, "2/9 popup: a longer message to check word wrap and "
                    "centring across several lines on the display");

    /* 3. progress meter 0..100% */
    splash_progress_set_delay(0);
    for (int i = 0; i <= 100; i += 4)
    {
        splash_progress(i, 100, "3/9 progress: %d%%", i);
        sleep(HZ / 20);
    }

    /* 4. plain blocking yes/no (long line to show centred word wrap) */
    {
        const char *lines[] = {
            "4/9 yes/no: this line is long enough to wrap across a couple "
            "of centred lines.",
            "Is it centred?",
        };
        struct text_message m = { lines, 2 };
        splash(HZ, res_name(gui_syncyesno_run(&m, NULL, NULL)));
    }

    /* 5. yes/no with a status-bar title */
    {
        const char *lines[] = { "5/9 title test." };
        struct text_message m = { lines, 1 };
        splash(HZ, res_name(
            gui_syncyesno_run_w_title("Dialog test", &m, NULL, NULL)));
    }

    /* 6. yes/no with a 5s timeout (default YES) + result messages */
    {
        const char *lines[]    = { "6/9 timeout 5s.", "Default is YES." };
        const char *yeslines[] = { "You chose YES" };
        const char *nolines[]  = { "You chose NO" };
        struct text_message m  = { lines, 2 };
        struct text_message ym = { yeslines, 1 };
        struct text_message nm = { nolines, 1 };
        splash(2 * HZ, res_name(gui_syncyesno_run_w_tmo(
            5 * HZ, YESNO_YES, "Timeout test", &m, &ym, &nm)));
    }

    /* 7. text input: edit a seeded string with the click wheel */
    {
        char buf[64] = "edit me";
        splash(HZ, "7/9 text input: wheel edits, PLAY->buttons, SELECT=OK");
        if (dialog_input(buf, sizeof buf) == 0)
            splashf(2 * HZ, "You entered: %s", buf);
        else
            splash(HZ, "Input cancelled");
    }

    /* 8+9. theming: rounded corners, an icon and custom colours, applied to
     * every dialog at once via the default style. */
    {
        struct dialog_style style;
        dialog_style_default(&style);
        style.box_border_radius    = 10;
        style.box_border_width     = 2;
        style.box_margin           = 12;
        style.button_border_radius = 6;
#ifdef HAVE_LCD_COLOR
        build_test_icon();
        style.icon                          = &test_icon;
        style.box_bg                        = LCD_RGBPACK(30, 30, 40);
        style.box_fg                        = LCD_RGBPACK(240, 240, 240);
        style.box_border_color              = LCD_RGBPACK(255, 80, 40);
        style.button_border_color           = LCD_RGBPACK(120, 120, 140);
        style.button_bg_selected            = LCD_RGBPACK(255, 80, 40);
        style.button_fg_selected            = LCD_RGBPACK(255, 255, 255);
        style.button_border_color_selected  = LCD_RGBPACK(255, 80, 40);
#endif
        dialog_set_default_style(&style);

        /* the popup is framed by the same style: rounded, icon, recoloured */
        splash(3 * HZ, "8/9 styled: rounded box, icon at the left, and the "
                       "text column inset beside it");

        const char *lines[] = { "8/9 styled yes/no.", "Rounded buttons?" };
        struct text_message m = { lines, 2 };
        splash(HZ, res_name(gui_syncyesno_run(&m, NULL, NULL)));

        char buf[64] = "styled input";
        splash(HZ, "9/9 styled text input");
        if (dialog_input(buf, sizeof buf) == 0)
            splashf(2 * HZ, "You entered: %s", buf);
        else
            splash(HZ, "Input cancelled");

        dialog_set_default_style(NULL);   /* back to the theme's plain look */
    }

    splash(HZ, "Dialog tests done");
    return 0;
}
