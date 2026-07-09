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
#ifndef __ROOT_MENU_H__
#define __ROOT_MENU_H__

#include "config.h"
#include "gcc_extensions.h"

void root_menu(void) NORETURN_ATTR;
struct menu_table {
    char *string;
    const struct menu_item_ex *item;
};

struct menu_table *root_menu_get_options(int *nb_options);

#ifdef HAVE_TAGCACHE
/* Number of reserved root-menu shortcut slots for tagnavi.config's root
 * ("main") menu's direct tag-browse rows -- see the comment at
 * GO_TO_TAGNAVI_FIRST below. */
#define TAGNAVI_MAIN_MENU_SLOTS 20
#endif

enum {
    /* from old menu api, but still required*/
    MENU_ATTACHED_USB = -10,
    MENU_SELECTED_EXIT = -9,

    GO_TO_ROOTITEM_CONTEXT = -5,
    GO_TO_PREVIOUS_MUSIC = -4,
    GO_TO_PREVIOUS_BROWSER = -3,
    GO_TO_PREVIOUS = -2,
    GO_TO_ROOT = -1,
    GO_TO_FILEBROWSER = 0,
#ifdef HAVE_TAGCACHE
    GO_TO_DBBROWSER,
#endif
    GO_TO_WPS,
    GO_TO_MAINMENU,
    GO_TO_RECENTBMARKS,
    GO_TO_PLUGIN,
    /* Do Not add any items above here unless you want it to be able to 
       be the "start screen" after a boot up. The setting in settings_list.c
       will need editing if this is the case. */
    GO_TO_BROWSEPLUGINS,
    GO_TO_TIMESCREEN,
    GO_TO_PLAYLISTS_SCREEN,
    GO_TO_PLAYLIST_VIEWER,
    GO_TO_SYSTEM_SCREEN,
    GO_TO_SHORTCUTMENU,
#ifdef HAVE_TAGCACHE
    GO_TO_PICTUREFLOW,
    /* Reserved block of main-menu shortcuts, one per direct tag-browse row of
     * tagnavi.config's root ("main") menu (Album/Artist/Genre/etc), looked up
     * by tagtree_get_main_menu_tag_row() rather than hardcoded per-tag cases
     * so they survive tagnavi.config edits/reordering. TAGNAVI_MAIN_MENU_SLOTS
     * covers today's 8 such rows with headroom for a customized
     * tagnavi_user.config; tagtree_get_main_menu_tag_row_count() is used to
     * hide slots beyond the real row count from the Customize Main Menu
     * screen and the default-enabled root menu, not just cap this block. */
    GO_TO_TAGNAVI_FIRST,
    GO_TO_TAGNAVI_LAST = GO_TO_TAGNAVI_FIRST + TAGNAVI_MAIN_MENU_SLOTS - 1,
    /* Returned by album_covers.c's SELECT handler: enter the database
     * browser directly at the tapped album's track list (see
     * tagtree_enter_album_tracks_on_next_load()). browser() special-cases
     * this so that backing all the way out of that browse session lands on
     * Album covers instead of the generic root menu. */
    GO_TO_ALBUM_COVERS_TRACKS,
#endif
};
#ifndef PLUGIN
extern struct menu_item_ex root_menu_;

void root_menu_load_from_cfg(void* setting, char *value);
char* root_menu_write_to_cfg(void* setting, char*buf, int buf_len);
void root_menu_set_default(void* setting, void* defaultval);
bool root_menu_is_changed(void* setting, void* defaultval);
#endif


#endif /* __ROOT_MENU_H__ */
