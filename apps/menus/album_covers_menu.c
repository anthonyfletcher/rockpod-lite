/***************************************************************************
 *             __________               __   ___.
 *   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
 *   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
 *   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
 *   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \
 *                     \/            \/     \/    \/            \/
 * $Id$
 *
 * Copyright (C) 2007 Jonathan Gordon
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

#include <stdio.h>
#include <stdbool.h>
#include "config.h"
#include "lang.h"
#include "action.h"
#include "settings.h"
#include "menu.h"
#include "yesno.h"
#include "splash.h"
#include "icons.h"
#include "gui/album_covers.h"

MENUITEM_SETTING(album_covers_center_margin, &global_settings.album_covers_center_margin, NULL);
MENUITEM_SETTING(album_covers_slide_tuck, &global_settings.album_covers_slide_tuck, NULL);
MENUITEM_SETTING(album_covers_zoom, &global_settings.album_covers_zoom, NULL);
MENUITEM_SETTING(album_covers_parallel_slides, &global_settings.album_covers_parallel_slides, NULL);
MENUITEM_SETTING(album_covers_scroll_speed, &global_settings.album_covers_scroll_speed, NULL);
MENUITEM_SETTING(album_covers_transition_speed, &global_settings.album_covers_transition_speed, NULL);
MENUITEM_SETTING(album_covers_show_album_name, &global_settings.album_covers_show_album_name, NULL);
MENUITEM_SETTING(album_covers_sort_albums_by, &global_settings.album_covers_sort_albums_by, NULL);
MENUITEM_SETTING(album_covers_year_sort_order, &global_settings.album_covers_year_sort_order, NULL);
MENUITEM_SETTING(album_covers_show_year, &global_settings.album_covers_show_year, NULL);

static int album_covers_menu_rebuild(void)
{
    if (yesno_pop_confirm(ID2P(LANG_REBUILD_CACHE)))
        album_covers_rebuild_cache();
    return 0;
}
MENUITEM_FUNCTION(album_covers_rebuild_item, 0, ID2P(LANG_REBUILD_CACHE),
                  album_covers_menu_rebuild, NULL, Icon_NOICON);

static int album_covers_menu_update(void)
{
    if (yesno_pop_confirm(ID2P(LANG_UPDATE_CACHE)))
        album_covers_update_cache();
    return 0;
}
MENUITEM_FUNCTION(album_covers_update_item, 0, ID2P(LANG_UPDATE_CACHE),
                  album_covers_menu_update, NULL, Icon_NOICON);

MAKE_MENU(album_covers_menu, ID2P(LANG_ALBUM_COVER_SETTINGS), NULL, Icon_Audio,
            &album_covers_show_album_name,
            &album_covers_show_year,
            &album_covers_year_sort_order,
            &album_covers_sort_albums_by,
            &album_covers_center_margin,
            &album_covers_slide_tuck,
            &album_covers_zoom,
            &album_covers_parallel_slides,
            &album_covers_scroll_speed,
            &album_covers_transition_speed,
            &album_covers_rebuild_item,
            &album_covers_update_item);
