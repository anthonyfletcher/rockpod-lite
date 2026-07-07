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

#include <stdint.h>
#include <stdio.h>
#include "string-extra.h"
#include "system.h"
#include "kernel.h"
#include "file.h"
#include "dir.h"
#include "rbpaths.h"
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

/* On-disk cache of decoded thumbnails, so a repeat visit (even in a later
 * session) can skip both find_albumart()'s filesystem probing and the
 * JPEG/BMP decode. Format/naming mirrors apps/plugins/pictureflow's "pfraw"
 * scheme (raw native-format pixels behind a tiny width/height header),
 * adapted to core calls instead of the plugin API, plus a size suffix since
 * unlike PictureFlow's single fixed slide size, different themes can request
 * %La at different dimensions. */
#define AA_CACHE_DIR ROCKBOX_DIR "/database_art_cache"

struct pfraw_header
{
    int32_t width;
    int32_t height;
};

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

/* FNV-1a-ish hash, matches apps/plugins/pictureflow/pictureflow.c's mfnv()
 * so the same style of filename derivation is used for both caches. */
static unsigned int fnv_hash(const char *str)
{
    const unsigned int p = 16777619;
    unsigned int hash = 0x811C9DC5;

    if (!str)
        return 0;

    while (*str)
        hash = (hash ^ (unsigned char)*str++) * p;
    hash += hash << 13;
    hash ^= hash >> 7;
    hash += hash << 3;
    hash ^= hash >> 17;
    hash += hash << 5;
    return hash;
}

/* Builds the pfraw cache path for (album, albumartist, size). Returns false
 * for an untagged album (no album string) rather than caching it -- both
 * hashing to 0 would collide every untagged album onto the same file. */
static bool get_pfraw_path(char *buf, size_t buflen, const char *album,
                           const char *albumartist, const struct dim *size)
{
    if (!album || !album[0])
        return false;

    snprintf(buf, buflen, AA_CACHE_DIR "/%x%x_%dx%d.pfraw",
              fnv_hash(album), fnv_hash(albumartist),
              size->width, size->height);
    return true;
}

static bool save_pfraw(const char *path, const struct bitmap *bm)
{
    struct pfraw_header hdr;
    int fd;

    if (!dir_exists(AA_CACHE_DIR))
        mkdir(AA_CACHE_DIR);

    hdr.width = bm->width;
    hdr.height = bm->height;

    fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (fd < 0)
        return false;

    write(fd, &hdr, sizeof(hdr));
    write(fd, bm->data, sizeof(fb_data) * bm->width * bm->height);
    close(fd);
    return true;
}

/* Reads a previously-saved pfraw file straight into a fresh core_alloc
 * buffer -- no JPEG/BMP decode needed on a cache hit. Rejects a file whose
 * stored dimensions don't match what's being requested now (a different
 * theme/viewport size reusing the same album+artist hash). */
static bool read_pfraw(const char *path, struct aa_cache_entry *e,
                       const struct dim *requested_size)
{
    struct pfraw_header hdr;
    int fd, handle;
    size_t size;

    fd = open(path, O_RDONLY);
    if (fd < 0)
        return false;

    if (read(fd, &hdr, sizeof(hdr)) != (ssize_t)sizeof(hdr) ||
        hdr.width != requested_size->width ||
        hdr.height != requested_size->height)
    {
        close(fd);
        return false;
    }

    size = sizeof(fb_data) * hdr.width * hdr.height;
    handle = core_alloc(size);
    if (handle < 0)
    {
        close(fd);
        return false;
    }

    e->bmp.width = hdr.width;
    e->bmp.height = hdr.height;
    e->bmp.data = core_get_data(handle);
#if LCD_DEPTH > 1 || (defined(HAVE_REMOTE_LCD) && LCD_REMOTE_DEPTH > 1)
    e->bmp.maskdata = NULL;
#endif

    if (read(fd, e->bmp.data, size) != (ssize_t)size)
    {
        close(fd);
        core_free(handle);
        return false;
    }
    close(fd);

    e->handle = handle;
    return true;
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

bool list_albumart_is_cached(int row_key, const struct bitmap **out)
{
    struct aa_cache_entry *e = find_entry(row_key);
    if (!e)
        return false;

    e->last_used_tick = current_tick;
    if (!e->has_art)
    {
        *out = NULL;
        return true;
    }
    e->bmp.data = core_get_data(e->handle);
    *out = &e->bmp;
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
    char pfraw_path[MAX_PATH];
    bool have_pfraw_path;
    bool found;

    if (e)
    {
        e->last_used_tick = current_tick;
        if (!e->has_art)
            return NULL;
        e->bmp.data = core_get_data(e->handle);
        return &e->bmp;
    }

    /* Callers are expected to have already tried list_albumart_is_cached()
     * before paying the tagcache cost of resolving path/album/artist -- if
     * we get here it should be a genuine miss, not a redraw of a row we've
     * already seen. */
    e = claim_slot();
    e->row_key = row_key;
    e->valid = true;
    e->last_used_tick = current_tick;

    /* Disk-cache fast path: skip both find_albumart()'s filesystem probing
     * and the image decode entirely if we've already resolved this album
     * before (even in an earlier session). */
    have_pfraw_path = get_pfraw_path(pfraw_path, sizeof(pfraw_path),
                                     album, albumartist, requested_size);
    if (have_pfraw_path && read_pfraw(pfraw_path, e, requested_size))
    {
        e->has_art = true;
        return &e->bmp;
    }

    memset(&id3, 0, sizeof(id3));
    strlcpy(id3.path, path, sizeof(id3.path));
    id3.album = (album && album[0]) ? (char *)album : NULL;
    id3.albumartist = (albumartist && albumartist[0]) ? (char *)albumartist : NULL;

    found = find_albumart(&id3, art_path, sizeof(art_path), requested_size);

    if (!found || !decode_art(art_path, e, requested_size))
    {
        e->has_art = false;
        return NULL;
    }

    if (have_pfraw_path)
        save_pfraw(pfraw_path, &e->bmp);

    e->has_art = true;
    return &e->bmp;
}

void list_albumart_precache_one(const char *path, const char *album,
                                 const char *albumartist,
                                 const struct dim *size)
{
    struct mp3entry id3;
    char art_path[MAX_PATH];
    char pfraw_path[MAX_PATH];
    struct aa_cache_entry tmp;

    if (!get_pfraw_path(pfraw_path, sizeof(pfraw_path), album, albumartist, size))
        return; /* untagged album: nothing to cache */
    if (file_exists(pfraw_path))
        return; /* already cached from a previous build/browse */

    /* 'path' is the representative track's own audio file path, not an
     * image -- find_albumart() resolves it to an actual cover file or
     * embedded-art extraction marker first, same as list_albumart_get_bitmap()
     * does for the browse-time (cache miss) path. Calling decode_art()
     * directly on the audio file path (as this used to) always fails, since
     * it isn't a JPEG/BMP -- which is why the disk cache never got any
     * files written despite the build appearing to run to completion. */
    memset(&id3, 0, sizeof(id3));
    strlcpy(id3.path, path, sizeof(id3.path));
    id3.album = (album && album[0]) ? (char *)album : NULL;
    id3.albumartist = (albumartist && albumartist[0]) ? (char *)albumartist : NULL;

    if (!find_albumart(&id3, art_path, sizeof(art_path), size))
        return;

    memset(&tmp, 0, sizeof(tmp));
    tmp.handle = -1;
    if (decode_art(art_path, &tmp, size))
    {
        save_pfraw(pfraw_path, &tmp.bmp);
        if (tmp.handle >= 0)
            core_free(tmp.handle);
    }
}

#define AA_CACHE_COMPLETE_MARKER AA_CACHE_DIR "/complete"

bool list_albumart_cache_is_complete(void)
{
    return file_exists(AA_CACHE_COMPLETE_MARKER);
}

void list_albumart_cache_mark_complete(void)
{
    int fd;
    if (!dir_exists(AA_CACHE_DIR))
        mkdir(AA_CACHE_DIR);
    fd = open(AA_CACHE_COMPLETE_MARKER, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (fd >= 0)
        close(fd);
}

void list_albumart_cache_mark_incomplete(void)
{
    remove(AA_CACHE_COMPLETE_MARKER);
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

void list_albumart_clear_disk_cache(void)
{
    DIR *dir;
    struct dirent *entry;
    char path[MAX_PATH];

    list_albumart_clear_cache();
    list_albumart_cache_mark_incomplete();

    dir = opendir(AA_CACHE_DIR);
    if (!dir)
        return;

    while ((entry = readdir(dir)) != NULL)
    {
        if (!strcmp(entry->d_name, ".") || !strcmp(entry->d_name, ".."))
            continue;

        snprintf(path, sizeof(path), AA_CACHE_DIR "/%s", entry->d_name);
        remove(path);
    }
    closedir(dir);
}

#endif /* HAVE_ALBUMART */
