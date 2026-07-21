/* was: apps/screens.c (time-setting screen) */
/***************************************************************************
 *             __________               __   ___.
 *   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
 *   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
 *   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
 *   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \
 *                     \/            \/     \/    \/            \/
 * $Id$
 *
 * Copyright (C) 2002 Björn Stenberg
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

#include <stdbool.h>
#include <stdio.h>
#include "config.h"
#include "lang.h"
#include "timefuncs.h"
#include "font.h"
#include "usb.h"
#include "input/action.h"
#include "settings/settings.h"
#include "speech/talk.h"
#include "speech/language.h" /* lang_is_rtl */
#include "draw/screen_access.h"
#include "draw/viewport.h"
#include "system/app_util.h"
#include "system/shutdown.h"
#include "time_set.h"

/* little helper function for voice output */
static void say_time(int cursorpos, const struct tm *tm)
{
    int value = 0;
    int unit = 0;

    if (!global_settings.talk_menu)
        return;

    switch(cursorpos)
    {
    case 0:
        value = tm->tm_hour;
        unit = UNIT_HOUR;
        break;
    case 1:
        value = tm->tm_min;
        unit = UNIT_MIN;
        break;
    case 2:
        value = tm->tm_sec;
        unit = UNIT_SEC;
        break;
    case 3:
        value = tm->tm_year + 1900;
        break;
    case 5:
        value = tm->tm_mday;
        break;
    }

    if (cursorpos == 4) /* month */
        talk_id(LANG_MONTH_JANUARY + tm->tm_mon, false);
    else
        talk_value(value, unit, false);
}


#define INDEX_X 0
#define INDEX_Y 1

#define SEPARATOR ":"

#define IDX_HOURS   0
#define IDX_MINUTES 1
#define IDX_SECONDS 2
#define IDX_YEAR    3
#define IDX_MONTH   4
#define IDX_DAY     5

#define OFF_HOURS   0
#define OFF_MINUTES 3
#define OFF_SECONDS 6
#define OFF_YEAR    9
#define OFF_DAY     14

bool set_time_screen(const char* title, struct tm *tm, bool set_date)
{
    struct viewport viewports[NB_SCREENS];
    bool done = false, usb = false;
    int cursorpos = 0;
    unsigned char offsets_ptr[] =
        { OFF_HOURS, OFF_MINUTES, OFF_SECONDS, OFF_YEAR, 0, OFF_DAY };

    if (lang_is_rtl())
    {
        offsets_ptr[IDX_HOURS] = OFF_SECONDS;
        offsets_ptr[IDX_SECONDS] = OFF_HOURS;
        offsets_ptr[IDX_YEAR] = OFF_DAY;
        offsets_ptr[IDX_DAY] = OFF_YEAR;
    }

    int last_item = IDX_DAY; /*time & date*/
    if (!set_date)
        last_item = IDX_SECONDS; /*time*/

    /* speak selection when screen is entered */
    say_time(cursorpos, tm);

    while (!done) {
        int button;
        unsigned int i, realyear, min, max;
        unsigned char *ptr[6];
        unsigned char buffer[20];
        int *valptr = NULL;

        static unsigned char daysinmonth[] =
            {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

        /* for easy acess in the drawing loop */
        for (i = 0; i < 6; i++)
            ptr[i] = buffer + offsets_ptr[i];
        ptr[IDX_MONTH] = str(LANG_MONTH_JANUARY + tm->tm_mon); /* month name */

        /* calculate the number of days in febuary */
        realyear = tm->tm_year + 1900;
        if((realyear % 4 == 0 && !(realyear % 100 == 0)) || realyear % 400 == 0)
            daysinmonth[1] = 29;
        else
            daysinmonth[1] = 28;

        /* fix day if month or year changed */
        if (tm->tm_mday > daysinmonth[tm->tm_mon])
            tm->tm_mday = daysinmonth[tm->tm_mon];

        /* calculate day of week */
        set_day_of_week(tm);

        /* put all the numbers we want from the tm struct into
           an easily printable buffer */
        snprintf(buffer, sizeof(buffer),
                 "%02d " "%02d " "%02d " "%04d " "%02d",
                 tm->tm_hour, tm->tm_min, tm->tm_sec,
                 tm->tm_year+1900, tm->tm_mday);

        /* convert spaces in the buffer to '\0' to make it possible to work
           directly on the buffer */
        for(i=0; i < sizeof(buffer); i++)
        {
            if(buffer[i] == ' ')
                buffer[i] = '\0';
        }

        FOR_NB_SCREENS(s)
        {
            int pos, nb_lines;
            unsigned int separator_width, weekday_width;
            unsigned int j, width, prev_line_height;
            /* 6 possible cursor possitions, 2 values stored for each: x, y */
            unsigned int cursor[6][2];
            struct viewport *vp = &viewports[s];
            struct viewport *last_vp;
            struct screen *screen = &screens[s];
            static unsigned char rtl_idx[] =
                { IDX_SECONDS, IDX_MINUTES, IDX_HOURS, IDX_DAY, IDX_MONTH, IDX_YEAR };

            viewport_set_defaults(vp, s);
            last_vp = screen->set_viewport(vp);
            nb_lines = viewport_get_nb_lines(vp);

            /* minimum lines needed is 2 + title line */
            if (nb_lines < 4)
            {
                vp->font = FONT_SYSFIXED;
                nb_lines = viewport_get_nb_lines(vp);
            }

            /* recalculate the positions and offsets */
            if (nb_lines >= 3)
                screen->getstringsize(title, NULL, &prev_line_height);
            else
                prev_line_height = 0;

            /* weekday */
            weekday_width = screen->getstringsize(str(LANG_WEEKDAY_SUNDAY + tm->tm_wday),
                                     NULL, NULL);
            separator_width = screen->getstringsize(SEPARATOR, NULL, NULL);

            for(i=0, j=0; i < 6; i++)
            {
                if(i==3) /* second row */
                {
                    j = weekday_width + separator_width;
                    prev_line_height *= 2;
                }
                width = screen->getstringsize(ptr[i], NULL, NULL);
                cursor[i][INDEX_Y] = prev_line_height;
                cursor[i][INDEX_X] = j;
                j += width + separator_width;
            }

            /* draw the screen */
            screen->set_viewport(vp);
            screen->clear_viewport();
            /* display the screen title */
            screen->puts_scroll(0, 0, title);

            /* these are not selectable, so we draw them outside the loop */
            /* name of the week day */
            screen->putsxy(0, cursor[3][INDEX_Y],
                              str(LANG_WEEKDAY_SUNDAY + tm->tm_wday));

            pos = lang_is_rtl() ? rtl_idx[cursorpos] : cursorpos;
            /* draw the selected item with drawmode set to
                DRMODE_SOLID|DRMODE_INVERSEVID, all other selectable
                items with drawmode DRMODE_SOLID */
            for(i=0; i<6; i++)
            {
                if (pos == (int)i)
                    vp->drawmode = (DRMODE_SOLID|DRMODE_INVERSEVID);

                screen->putsxy(cursor[i][INDEX_X],
                                  cursor[i][INDEX_Y], ptr[i]);

                vp->drawmode = DRMODE_SOLID;

                screen->putsxy(cursor[i/4 +1][INDEX_X] - separator_width,
                                  cursor[0][INDEX_Y], SEPARATOR);
            }

            /* print help text */
            if (nb_lines > 4)
                screen->puts(0, 4, str(LANG_TIME_SET_BUTTON));
            if (nb_lines > 5)
                screen->puts(0, 5, str(LANG_TIME_REVERT));
            screen->update_viewport();
            screen->set_viewport(last_vp);
        }

        /* set the most common numbers */
        min = 0;
        max = 59;
        /* calculate the minimum and maximum for the number under cursor */
        switch(cursorpos) {
            case 0: /* hour */
                max = 23;
                valptr = &tm->tm_hour;
                break;
            case 1: /* minute */
                valptr = &tm->tm_min;
                break;
            case 2: /* second */
                valptr = &tm->tm_sec;
                break;
            case 3: /* year */
                min = 1;
                max = 200;
                valptr = &tm->tm_year;
                break;
            case 4: /* month */
                max = 11;
                valptr = &tm->tm_mon;
                break;
            case 5: /* day */
                min = 1;
                max = daysinmonth[tm->tm_mon];
                valptr = &tm->tm_mday;
                break;
        }

        button = get_action(CONTEXT_SETTINGS_TIME, TIMEOUT_BLOCK);
        switch ( button ) {
            case ACTION_STD_PREV:
                cursorpos = clamp_value_wrap(--cursorpos, last_item, 0);
                say_time(cursorpos, tm);
                break;
            case ACTION_STD_NEXT:
                cursorpos = clamp_value_wrap(++cursorpos, last_item, 0);
                say_time(cursorpos, tm);
                break;
            case ACTION_SETTINGS_INC:
            case ACTION_SETTINGS_INCREPEAT:
                *valptr = clamp_value_wrap(++(*valptr), max, min);
                say_time(cursorpos, tm);
                break;
            case ACTION_SETTINGS_DEC:
            case ACTION_SETTINGS_DECREPEAT:
                *valptr = clamp_value_wrap(--(*valptr), max, min);
                say_time(cursorpos, tm);
                break;

            case ACTION_STD_OK:
                done = true;
                break;

            case ACTION_STD_CANCEL:
                done = true;
                tm->tm_year = -1;
                break;

            default:
                if (default_event_handler(button) == SYS_USB_CONNECTED)
                    done = usb = true;
                break;
        }
    }
    FOR_NB_SCREENS(s)
        screens[s].scroll_stop_viewport(&viewports[s]);
    return usb;
}
