/* was: apps/menus/exported_menus.h */
/***************************************************************************
 *             __________               __   ___.
 *   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
 *   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
 *   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
 *   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \
 *                     \/            \/     \/    \/            \/
 * $Id$
 *
 * Copyright (C) 2006 Jonathan Gordon
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
#ifndef _EXPORTED_MENUS_H
#define _EXPORTED_MENUS_H

#include "widgets/menu.h"
/* not needed for plugins */

extern const struct menu_item_ex 
        display_menu,               /* display_menu.c   */
        playback_settings,          /* playback_menu.c  */
        sound_settings,             /* sound_menu.c     */
        settings_menu_item,         /* settings_menu.c  */
        bookmark_settings_menu,
        playlist_settings,          /* playlist_menu.c  */
        viewer_settings_menu,       /* playlist_menu.c  */
        equalizer_menu,             /* eq_menu.c        */
        theme_menu                  /* theme_menu.c     */
        , album_covers_menu         /* album_covers_menu.c */
        , text_viewer_menu;         /* text_viewer_menu.c */

struct browse_folder_info {
    const char* dir;
    int show_options;
};
int browse_folder(void *param); /* in theme_menu.c as it is mostly used there */
int main_menu_config(void); /* in main_menu_config.c */

#endif /*_EXPORTED_MENUS_H */
