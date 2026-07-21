/* was: apps/misc.h (activity stack) */
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

#ifndef _ACTIVITY_H_
#define _ACTIVITY_H_

#include <stdbool.h>

enum current_activity {
    ACTIVITY_UNKNOWN = 0,
    ACTIVITY_MAINMENU,
    ACTIVITY_WPS,
    ACTIVITY_RECORDING,
    ACTIVITY_FM,
    ACTIVITY_PLAYLISTVIEWER,
    ACTIVITY_SETTINGS,
    ACTIVITY_FILEBROWSER,
    ACTIVITY_DATABASEBROWSER,
    ACTIVITY_PLUGINBROWSER,
    ACTIVITY_QUICKSCREEN,
    ACTIVITY_PITCHSCREEN,
    ACTIVITY_OPTIONSELECT,
    ACTIVITY_PLAYLISTBROWSER,
    ACTIVITY_PLUGIN,
    ACTIVITY_CONTEXTMENU,
    ACTIVITY_SYSTEMSCREEN,
    ACTIVITY_TIMEDATESCREEN,
    ACTIVITY_BOOKMARKSLIST,
    ACTIVITY_SHORTCUTSMENU,
    ACTIVITY_ID3SCREEN,
    ACTIVITY_USBSCREEN,
    ACTIVITY_ALBUMCOVERS,
    ACTIVITY_TEXTVIEWER,
    ACTIVITY_IMAGEVIEWER
};

/* custom string representation of activity */
#define MAKE_ACT_STR(act) ((char[3]){'>', 'A'+ (act), 0x0})

void push_current_activity(enum current_activity screen);
void push_activity_without_refresh(enum current_activity screen);
void pop_current_activity(void);
void pop_current_activity_without_refresh(void);
enum current_activity get_current_activity(void);

/* Generic "working" indicator for the themed status bar (%lw / "Working..").
 * Bracket a long-running foreground or background operation with
 * ui_set_working(true)/(false) to show the skin's generic busy notification.
 * Distinct from the database/thumbnail-cache "building" indicator (%lb), which
 * is driven automatically. No built-in caller yet -- reserved for future use. */
bool ui_working(void);
void ui_set_working(bool working);


#endif /* _ACTIVITY_H_ */
