/***************************************************************************
 *             __________               __   ___.
 *   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
 *   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
 *   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
 *   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \
 *                     \/            \/     \/    \/            \/
 * $Id$
 *
 * Copyright (C) 2026 Rockpod
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

#ifndef _ALBUMART_CACHE_H_
#define _ALBUMART_CACHE_H_

#include <stdbool.h>
#include <stdint.h>

/* On-disk thumbnail file format: this header, followed by width*height native
 * (fb_data) pixels in ROW-MAJOR order. Exposed so consumers (e.g. Cover Flow)
 * can read a cached thumbnail directly. */
#define ALBUMART_CACHE_MAGIC          0x5441u  /* 'AT' */
#define ALBUMART_CACHE_FORMAT_VERSION 1

struct albumart_cache_header
{
    uint16_t magic;
    uint16_t version;
    uint16_t width;
    uint16_t height;
};

/* Background, database-driven album-art thumbnail cache.
 *
 * A dedicated low-priority thread walks the tagcache once the database is
 * ready and idle, resolves each album folder's cover art, and renders it to
 * a set of square thumbnails (see apps/albumart_sizes.h) under
 * ROCKBOX_DIR/thumbcache/<sizename>/<hash>.aat. Thumbnails are keyed by a hash
 * of the album's folder path, so once a folder is done later passes skip it
 * with a cheap existence check (no art re-resolution), and keys stay valid
 * across database rebuilds. */

/* Start the background cache thread. Call once at startup, after tagcache. */
void albumart_cache_init(void);

/* True while a generation pass is running. Drives the status-bar %lc token. */
bool albumart_cache_is_busy(void);

/* Resolve the cache-file path for a given album folder and size index.
 * Returns true and fills 'out' if a cached thumbnail file exists, false
 * otherwise. 'dir' is the album's folder path (the directory containing the
 * track), with no trailing slash -- the same key generation uses. */
bool albumart_cache_lookup(const char *dir, int size_index,
                           char *out, int out_len);

/* Number of configured thumbnail sizes, and accessors for each. */
int         albumart_cache_num_sizes(void);
int         albumart_cache_size_dim(int size_index);
const char *albumart_cache_size_name(int size_index);
/* Index of the named size in the table, or -1 if not present. */
int         albumart_cache_size_index(const char *name);

#endif /* _ALBUMART_CACHE_H_ */
