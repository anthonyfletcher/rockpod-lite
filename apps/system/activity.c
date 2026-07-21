/* was: apps/misc.c (activity stack) */
/***************************************************************************
 *             __________               __   ___.
 *   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
 *   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
 *   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
 *   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \
 *                     \/            \/     \/    \/            \/
 * $Id$
 *
 * Copyright (C) 2002 by Daniel Stenberg
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
#include "config.h"
#include "system.h"
#include "debug.h"
#include "draw/viewport.h"
#include "skin/skin_engine.h"  /* skin_update, CUSTOM_STATUSBAR */
#include "skin/statusbar_skinned.h" /* sb_skin_force_next_update */
#include "widgets/list.h"            /* skinlist_set_cfg */
#include "activity.h"

#define MAX_ACTIVITY_DEPTH 12
static enum current_activity
        current_activity[MAX_ACTIVITY_DEPTH] = {ACTIVITY_UNKNOWN};
static int current_activity_top = 0;

static void push_current_activity_refresh(enum current_activity screen, bool refresh)
{
    current_activity[current_activity_top++] = screen;
    FOR_NB_SCREENS(i)
    {
        skinlist_set_cfg(i, NULL);
        if (refresh)
        {
            skin_render_inhibit_flush(true);
            skin_update(CUSTOM_STATUSBAR, i, SKIN_REFRESH_ALL);
            skin_render_inhibit_flush(false);
        }
    }
    if (refresh)
        sb_skin_force_next_update();
}

static void pop_current_activity_refresh(bool refresh)
{
    current_activity_top--;
    FOR_NB_SCREENS(i)
    {
        skinlist_set_cfg(i, NULL);
        if (refresh)
        {
            skin_render_inhibit_flush(true);
            skin_update(CUSTOM_STATUSBAR, i, SKIN_REFRESH_ALL);
            skin_render_inhibit_flush(false);
        }
    }
    if (refresh)
        sb_skin_force_next_update();
}

void push_current_activity(enum current_activity screen)
{
    push_current_activity_refresh(screen, true);
}

void push_activity_without_refresh(enum current_activity screen)
{
    push_current_activity_refresh(screen, false);
}

void pop_current_activity(void)
{
    pop_current_activity_refresh(true);
#if 0
    current_activity_top--;
    FOR_NB_SCREENS(i)
    {
        skinlist_set_cfg(i, NULL);
        if (ACTIVITY_REFRESH_NOW == refresh)
            skin_update(CUSTOM_STATUSBAR, i, SKIN_REFRESH_ALL);
    }
#endif
}

void pop_current_activity_without_refresh(void)
{
    pop_current_activity_refresh(false);
}

enum current_activity get_current_activity(void)
{
    return current_activity[current_activity_top?current_activity_top-1:0];
}

/* Generic "working" busy indicator, surfaced to skins as %lw. */
static bool ui_working_flag = false;

bool ui_working(void)
{
    return ui_working_flag;
}

void ui_set_working(bool working)
{
    ui_working_flag = working;
}

