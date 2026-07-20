/***************************************************************************
 *             __________               __   ___.
 *   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
 *   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
 *   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
 *   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \
 *                     \/            \/     \/    \/            \/
 * $Id$
 *
 * Copyright (C) 2005 by Miika Pekkarinen
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
#ifndef _TAGTREE_H
#define _TAGTREE_H

#include "config.h"
#include "tagcache.h"
#include "tree.h"

#define TAGNAVI_VERSION    "#! rockbox/tagbrowser/2.0"
#define TAGMENU_MAX_ITEMS  64
#define TAGMENU_MAX_MENUS  32
#define TAGMENU_MAX_FMTS   32

int tagtree_export(void);
int tagtree_import(void);
void tagtree_init(void) INIT_ATTR;
int tagtree_enter(struct tree_context* c, bool is_visible);
void tagtree_exit(struct tree_context* c, bool is_visible);
int tagtree_load(struct tree_context* c);
char* tagtree_get_entry_name(struct tree_context *c, int id,
                                    char* buf, size_t bufsize);
bool tagtree_current_playlist_insert(int position, bool queue);
int tagtree_add_to_playlist(const char* playlist, bool new_playlist);
char *tagtree_get_title(struct tree_context* c);
int tagtree_get_attr(struct tree_context* c);
bool tagtree_is_album_list(struct tree_context* c);
bool tagtree_is_artist_list(struct tree_context* c);
bool tagtree_get_album_dir(struct tree_context* c, int item,
                           char *buf, int buflen);
bool tagtree_get_artist_dir(struct tree_context* c, int item,
                            char *buf, int buflen);
int tagtree_get_icon(struct tree_context* c);
/* Arms a one-shot jump: the next time tagtree_load() sees a fresh root load
 * (dirlevel 0, TABLE_ROOT), it enters the root menu's row whose first tag
 * matches 'tag' (e.g. tag_album), looked up by tag identity rather than
 * position so it's robust to tagnavi.config reordering. Used by
 * root_menu.c's tagnavi-derived main-menu shortcuts. */
void tagtree_enter_by_tag_on_next_load(int tag);
/* Arms a direct jump: root -> straight into that specific album's own track
 * list, identified by its tagcache seek (not name/position), skipping the
 * intermediate "Album" grouping listing entirely. Used by
 * apps/gui/album_covers.c so selecting a cover lands directly on that
 * album's tracks in the core database browser, with a single BACK press
 * exiting straight back out (no intermediate level to unwind through). */
void tagtree_enter_album_tracks_on_next_load(long album_seek,
                                             const char *album_title);
/* As above, but jumps straight to a specific album-artist's album listing
 * (identified by seek), for Artist portraits (apps/gui/album_covers.c). A single
 * BACK returns to the carousel; selecting an album descends into its tracks. */
void tagtree_enter_artist_albums_on_next_load(long albumartist_seek,
                                              const char *artist_title);
/* Number of direct tag-browse ("->") rows in the root ("main") menu -- rows
 * that load a nested sub-menu ("==>") or trigger an action (e.g. "~>"
 * shuffle) don't count. Used by root_menu.c to know how many of its reserved
 * GO_TO_TAGNAVI_FIRST..LAST slots are backed by a real row. */
int tagtree_get_main_menu_tag_row_count(void);
/* Returns the tag and raw (P2STR-resolvable) display name of the Nth (0-based)
 * such row. Returns false if index is out of range. */
bool tagtree_get_main_menu_tag_row(int index, int *out_tag,
                                   const unsigned char **out_name);
int tagtree_get_filename(struct tree_context* c, char *buf, int buflen);
int tagtree_get_custom_action(struct tree_context* c);
bool tagtree_get_subentry_filename(char *buf, size_t bufsize);
bool tagtree_subentries_do_action(bool (*action_cb)(const char *file_name));

#endif
