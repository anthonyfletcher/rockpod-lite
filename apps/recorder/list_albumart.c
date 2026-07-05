/***************************************************************************
 *             __________               __   ___.
 *   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
 *   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
 *   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
 *   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \
 *                     \/            \/     \/    \/            \/
 * $Id$
 *
 * Copyright (C) 2026 by the Rockpod-lite contributors
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

/* Per-row album art cache for browsing screens (e.g. the Database Albums
 * list). Deliberately independent from the playback engine's album art
 * (playback.c/buffering.c): that path shares the audio ring buffer and is
 * scoped to the currently playing/buffered tracks, which has no notion of
 * "album N rows away from what's playing" and shouldn't be made to compete
 * with live audio buffering for space. This module instead decodes
 * directly into its own small, bounded core_alloc pool using the same
 * lower-level decoders (read_bmp_file/read_jpeg_file) that buffering.c
 * uses internally. */

#include <string.h>
#include "config.h"

#ifdef HAVE_ALBUMART

#include "string-extra.h"
#include "system.h"
#include "kernel.h"
#include "file.h"
#include "core_alloc.h"
#include "metadata.h"
#include "albumart.h"
#include "bmp.h"
#ifdef HAVE_JPEG
#include "jpeg_load.h"
#endif
#include "list_albumart.h"

/* Sized for the rows visible on screen plus a small scroll lookahead;
 * this is a browse-time cache, not a whole-library index. */
#define AA_CACHE_SIZE 12

struct aa_cache_entry
{
    int row_key;
    bool valid;    /* true once this slot has been resolved (found or not) */
    bool has_art;  /* false = confirmed no art available for this row */
    int handle;    /* core_alloc handle backing bmp.data; -1 if unused */
    struct bitmap bmp;
    long last_used_tick;
};

static struct aa_cache_entry aa_cache[AA_CACHE_SIZE];

static struct aa_cache_entry *find_entry(int row_key)
{
    int i;
    for (i = 0; i < AA_CACHE_SIZE; i++)
    {
        if (aa_cache[i].valid && aa_cache[i].row_key == row_key)
            return &aa_cache[i];
    }
    return NULL;
}

static struct aa_cache_entry *claim_slot(void)
{
    int i, oldest = 0;
    struct aa_cache_entry *e;
    bool was_valid;

    for (i = 0; i < AA_CACHE_SIZE; i++)
    {
        if (!aa_cache[i].valid)
        {
            oldest = i;
            break;
        }
        if (aa_cache[i].last_used_tick < aa_cache[oldest].last_used_tick)
            oldest = i;
    }

    e = &aa_cache[oldest];
    /* A never-used slot's fields are zero-initialized statics, not -1
     * sentinels, so only trust/free 'handle' for a slot that was
     * actually populated before (valid == true). */
    was_valid = e->valid;
    if (was_valid && e->handle >= 0)
        core_free(e->handle);
    memset(e, 0, sizeof(*e));
    e->handle = -1;
    return e;
}

static bool decode_art(const char *path, struct aa_cache_entry *e,
                       const struct dim *requested_size)
{
    const int format = FORMAT_NATIVE | FORMAT_DITHER |
                        FORMAT_RESIZE | FORMAT_KEEP_ASPECT;
    size_t maxsize = BM_SCALED_SIZE(requested_size->width, requested_size->height,
                                     FORMAT_NATIVE, false);
    size_t len = strlen(path);
    int handle, rc;

    handle = core_alloc(maxsize);
    if (handle < 0)
        return false;

    e->bmp.width = requested_size->width;
    e->bmp.height = requested_size->height;
    e->bmp.data = core_get_data(handle);
#if LCD_DEPTH > 1 || (defined(HAVE_REMOTE_LCD) && LCD_REMOTE_DEPTH > 1)
    e->bmp.maskdata = NULL;
#endif

#ifdef HAVE_JPEG
    if (len < 4 || strcmp(path + len - 4, ".bmp") != 0)
        rc = read_jpeg_file(path, &e->bmp, (int)maxsize, format, NULL);
    else
#endif
        rc = read_bmp_file(path, &e->bmp, (int)maxsize, format, NULL);

    if (rc <= 0)
    {
        core_free(handle);
        return false;
    }

    e->handle = handle;
    return true;
}

const struct bitmap *list_albumart_get_bitmap(const char *path,
                                               const char *album,
                                               const char *albumartist,
                                               int row_key,
                                               const struct dim *requested_size)
{
    struct aa_cache_entry *e = find_entry(row_key);
    struct mp3entry id3;
    char art_path[MAX_PATH];
    bool found;

    if (e)
    {
        e->last_used_tick = current_tick;
        if (!e->has_art)
            return NULL;
        e->bmp.data = core_get_data(e->handle);
        return &e->bmp;
    }

    memset(&id3, 0, sizeof(id3));
    strlcpy(id3.path, path, sizeof(id3.path));
    id3.album = (album && album[0]) ? (char *)album : NULL;
    id3.albumartist = (albumartist && albumartist[0]) ? (char *)albumartist : NULL;

    found = find_albumart(&id3, art_path, sizeof(art_path), requested_size);

    e = claim_slot();
    e->row_key = row_key;
    e->valid = true;
    e->last_used_tick = current_tick;

    if (!found || !decode_art(art_path, e, requested_size))
    {
        e->has_art = false;
        return NULL;
    }

    e->has_art = true;
    return &e->bmp;
}

void list_albumart_clear_cache(void)
{
    int i;
    for (i = 0; i < AA_CACHE_SIZE; i++)
    {
        /* Only ever-populated slots (valid) have a trustworthy handle;
         * a never-used slot's zero-initialized 'handle' is not a real
         * core_alloc handle and must not be freed. */
        if (aa_cache[i].valid && aa_cache[i].handle >= 0)
            core_free(aa_cache[i].handle);
    }
    memset(aa_cache, 0, sizeof(aa_cache));
    for (i = 0; i < AA_CACHE_SIZE; i++)
        aa_cache[i].handle = -1;
}

#endif /* HAVE_ALBUMART */
