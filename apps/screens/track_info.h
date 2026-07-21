/* was: apps/screens.h (browse_id3) */
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

#ifndef _TRACK_INFO_H_
#define _TRACK_INFO_H_

#include <stdbool.h>
#include "timefuncs.h"
#include "metadata.h"
#include "playlist/playlist.h"

/* Track Info screen. The view_text callback renders one field full-screen;
 * callers pass widgets/text_box.c's view_text(). */
bool browse_id3_ex(struct mp3entry *id3, struct playlist_info *playlist,
                   int playlist_display_index, int playlist_amount,
                   struct tm *modified, int track_ct,
                   int (*view_text)(const char *title, const char *text));

bool browse_id3(struct mp3entry *id3, int playlist_display_index,
                int playlist_amount, struct tm *modified, int track_ct,
                int (*view_text)(const char *title, const char *text));

#endif /* _TRACK_INFO_H_ */
