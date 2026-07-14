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

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include "config.h"

#ifdef HAVE_ALBUMART

#include "system.h"
#include "kernel.h"
#include "thread.h"
#include "core_alloc.h"
#include "string-extra.h"
#include "file.h"
#include "dir.h"
#include "pathfuncs.h"
#include "rbpaths.h"
#include "metadata.h"
#include "albumart.h"
#include "albumart_cache.h"
#include "albumart_sizes.h"
#include "tagcache.h"
#include "lcd.h"
#include "bmp.h"
#ifdef HAVE_JPEG
#include "jpeg_load.h"
#endif
#include "usb.h"
#ifdef HAVE_ADJUSTABLE_CPU_FREQ
#include "cpu.h"
#endif

/* Define LOGF_ENABLE to enable logf output in this file */
/*#define LOGF_ENABLE*/
#include "logf.h"

#define THUMBCACHE_DIR ROCKBOX_DIR "/thumbcache"

/* On-disk thumbnail format (struct albumart_cache_header + row-major native
 * pixels) is declared in albumart_cache.h so consumers can read it. A
 * magic/version lets a future format change be detected per file rather than
 * needing a global cache wipe. */

/* Directory-dedup "seen" set: open-addressed table of directory-path hashes.
 * Sized generously; if a library exceeds this, extra directories simply get
 * re-resolved (still idempotent -- generation skips existing thumbnails). */
#define AA_SEEN_SLOTS 8192  /* power of two */

/* Generous stack: the JPEG decoder called from a pass has deep frames. */
#define AA_STACK_SIZE (DEFAULT_STACK_SIZE + 0x2000)
static long aa_stack[AA_STACK_SIZE / sizeof(long)];
static const char aa_thread_name[] = "aacache";
static unsigned int aa_thread_id;
static struct event_queue aa_queue;

static volatile bool cache_busy;

/* Scratch id3 used only to feed search_albumart_files(); kept out of the
 * thread stack because struct mp3entry is large. */
static struct mp3entry aa_id3;

/* Scratch buffers used only by the aacache thread (aa_run_pass /
 * aa_generate_one). Kept off the thread stack -- together with the JPEG
 * decoder's own deep frames they otherwise overflow it. Single-threaded and
 * non-reentrant, so module-level statics are safe. */
static char aa_tcs_buf[TAGCACHE_BUFSZ];
static char aa_artpath[MAX_PATH];
static char aa_dir[MAX_PATH];
static char aa_check_path[MAX_PATH];
static char aa_out_path[MAX_PATH];

bool albumart_cache_is_busy(void)
{
    return cache_busy;
}

int albumart_cache_num_sizes(void)
{
    return ALBUMART_CACHE_NUM_SIZES;
}

int albumart_cache_size_dim(int size_index)
{
    if (size_index < 0 || size_index >= ALBUMART_CACHE_NUM_SIZES)
        return 0;
    return albumart_sizes[size_index].dim;
}

const char *albumart_cache_size_name(int size_index)
{
    if (size_index < 0 || size_index >= ALBUMART_CACHE_NUM_SIZES)
        return NULL;
    return albumart_sizes[size_index].name;
}

int albumart_cache_size_index(const char *name)
{
    int i;
    if (!name)
        return -1;
    for (i = 0; i < ALBUMART_CACHE_NUM_SIZES; i++)
        if (!strcmp(albumart_sizes[i].name, name))
            return i;
    return -1;
}

/* Modified FNV hash (same as PictureFlow's, good avalanche/distribution). */
static unsigned int aa_hash(const char *str)
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

static void aa_cache_path(char *out, int out_len, int size_index,
                          unsigned int arthash)
{
    snprintf(out, out_len, THUMBCACHE_DIR "/%s/%08x.aat",
             albumart_sizes[size_index].name, arthash);
}

bool albumart_cache_lookup(const char *dir, int size_index,
                           char *out, int out_len)
{
    if (!dir || size_index < 0 || size_index >= ALBUMART_CACHE_NUM_SIZES)
        return false;
    aa_cache_path(out, out_len, size_index, aa_hash(dir));
    return file_exists(out);
}

static void aa_ensure_dirs(void)
{
    int i;
    char p[MAX_PATH];
    mkdir(THUMBCACHE_DIR);
    for (i = 0; i < ALBUMART_CACHE_NUM_SIZES; i++)
    {
        snprintf(p, sizeof(p), THUMBCACHE_DIR "/%s", albumart_sizes[i].name);
        mkdir(p);
    }
}

/* Extract the directory portion (without trailing slash) of a full path. */
static void aa_dirname(const char *path, char *dir, int dir_len)
{
    const char *sep = strrchr(path, '/');
    int len = sep ? (int)(sep - path) : 0;
    if (len >= dir_len)
        len = dir_len - 1;
    memcpy(dir, path, len);
    dir[len] = 0;
}

/* Returns true if 'h' was already present; otherwise records it and returns
 * false. h==0 is remapped so 0 can mark an empty slot. */
static bool aa_seen(unsigned int *seen, unsigned int h)
{
    unsigned int i, idx;
    if (h == 0)
        h = 1;
    idx = h & (AA_SEEN_SLOTS - 1);
    for (i = 0; i < AA_SEEN_SLOTS; i++)
    {
        unsigned int slot = (idx + i) & (AA_SEEN_SLOTS - 1);
        if (seen[slot] == 0)
        {
            seen[slot] = h;
            return false;
        }
        if (seen[slot] == h)
            return true;
    }
    return true; /* table full -> treat as present */
}

/* Decode/scale the source art into a square thumbnail and write it. The
 * source is fitted (aspect preserved) inside dim x dim; square sources fill
 * the square exactly. Returns true on success. */
static bool aa_generate_one(const char *art_path, int size_index,
                            unsigned int key, void *workbuf, size_t workbuf_sz)
{
    int dim = albumart_sizes[size_index].dim;
    struct bitmap bm;
    int fmt = FORMAT_NATIVE | FORMAT_RESIZE | FORMAT_KEEP_ASPECT | FORMAT_DITHER;
    int ret;
    size_t namelen = strlen(art_path);
    int fd;
    struct albumart_cache_header hdr;
    bool ok;
    size_t bytes;

    memset(&bm, 0, sizeof(bm));
    bm.data = workbuf;
    bm.width = dim;
    bm.height = dim;
#if LCD_DEPTH > 1
    bm.format = FORMAT_NATIVE;
#endif

    if (namelen >= 4 && strcmp(art_path + namelen - 4, ".bmp") != 0)
    {
#ifdef HAVE_JPEG
        ret = read_jpeg_file(art_path, &bm, (int)workbuf_sz, fmt, NULL);
#else
        return false;
#endif
    }
    else
    {
        ret = read_bmp_file(art_path, &bm, (int)workbuf_sz, fmt, NULL);
    }

    if (ret <= 0 || bm.width <= 0 || bm.height <= 0)
        return false;

    aa_cache_path(aa_out_path, sizeof(aa_out_path), size_index, key);
    fd = open(aa_out_path, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (fd < 0)
        return false;

    hdr.magic = ALBUMART_CACHE_MAGIC;
    hdr.version = ALBUMART_CACHE_FORMAT_VERSION;
    hdr.width = bm.width;
    hdr.height = bm.height;
    bytes = (size_t)bm.width * bm.height * FB_DATA_SZ;

    ok = (write(fd, &hdr, sizeof(hdr)) == (ssize_t)sizeof(hdr)) &&
         (write(fd, bm.data, bytes) == (ssize_t)bytes);
    close(fd);

    if (!ok)
        remove(aa_out_path);
    return ok;
}

/* True if the pass should stop right now: a USB connection or shutdown is
 * pending. Uses queue_peek so the event stays queued for the thread loop to
 * actually acknowledge -- we just need to stop touching the disk promptly. */
static bool aa_check_abort(void)
{
    struct queue_event ev;
    if (!queue_peek(&aa_queue, &ev))
        return false;
    switch (ev.id)
    {
        case SYS_USB_CONNECTED:
        case SYS_POWEROFF:
        case SYS_REBOOT:
            return true;
    }
    return false;
}

/* One full generation pass: walk every track filename, dedup by directory,
 * resolve folder art, and render any missing thumbnails. Returns true if the
 * pass ran to completion, false if it was aborted (USB/DB busy/no memory) and
 * should be retried later. */
static bool aa_run_pass(void)
{
    struct tagcache_search tcs;
    size_t worksz;
    int wh, sh;
    void *workbuf;
    unsigned int *seen;
    bool aborted = false;
    int since_yield = 0;

    worksz = BM_SCALED_SIZE(ALBUMART_CACHE_MAX_DIM, ALBUMART_CACHE_MAX_DIM,
                            FORMAT_NATIVE, 0);
#ifdef HAVE_JPEG
    worksz += JPEG_DECODE_OVERHEAD;
#endif

    wh = core_alloc(worksz);
    if (wh <= 0)
        return false; /* not enough free memory right now; retry later */

    sh = core_alloc(AA_SEEN_SLOTS * sizeof(unsigned int));
    if (sh <= 0)
    {
        core_free(wh);
        return false;
    }

    workbuf = core_get_data_pinned(wh);
    seen = core_get_data_pinned(sh);
    memset(seen, 0, AA_SEEN_SLOTS * sizeof(unsigned int));

    aa_ensure_dirs();

    if (!tagcache_search(&tcs, tag_filename))
    {
        aborted = true;
        goto out;
    }

    /* A pass is running -> lights the status-bar %lc ("Caching") token. */
    cache_busy = true;
#ifdef HAVE_ADJUSTABLE_CPU_FREQ
    /* Speed up the (CPU-bound) image decoding, like Cover Flow's own
     * generator does. Passes are rare (only after the database settles or
     * changes), so the extra clock is a brief one-off. */
    cpu_boost(true);
#endif

    while (tagcache_get_next(&tcs, aa_tcs_buf, sizeof(aa_tcs_buf)))
    {
        int s;
        unsigned int dh;
        bool all_exist;

        /* Abort promptly on USB/shutdown (the thread loop then acknowledges)
         * or yield the disk to an incoming database commit, so a long pass
         * never blocks either. */
        if (aa_check_abort() || tagcache_is_busy())
        {
            aborted = true;
            break;
        }

        aa_dirname(tcs.result, aa_dir, sizeof(aa_dir));
        dh = aa_hash(aa_dir);            /* thumbnails are keyed by folder */
        if (aa_seen(seen, dh))
            goto next;

        /* Skip folders whose thumbnails already exist WITHOUT resolving the
         * source art. Art resolution (search_albumart_files) is ~10 file
         * probes; doing it for every folder on every pass is what made the
         * engine churn the disk endlessly. These file_exists checks are
         * served from dircache (RAM), so a steady-state pass is cheap. */
        all_exist = true;
        for (s = 0; s < ALBUMART_CACHE_NUM_SIZES; s++)
        {
            aa_cache_path(aa_check_path, sizeof(aa_check_path), s, dh);
            if (!file_exists(aa_check_path))
            {
                all_exist = false;
                break;
            }
        }
        if (all_exist)
            goto next;

        /* Only now (a thumbnail is missing) resolve this folder's cover art.
         * album/albumartist left NULL: only folder-based art is searched
         * (cover.bmp, folder.jpg, ../cover.bmp). */
        memset(&aa_id3, 0, sizeof(aa_id3));
        strlcpy(aa_id3.path, tcs.result, sizeof(aa_id3.path));
        if (!search_albumart_files(&aa_id3, "", aa_artpath, sizeof(aa_artpath)))
            goto next;

        for (s = 0; s < ALBUMART_CACHE_NUM_SIZES; s++)
        {
            aa_cache_path(aa_check_path, sizeof(aa_check_path), s, dh);
            if (file_exists(aa_check_path))
                continue;
            /* Re-check right before a (potentially slow) decode so a USB
             * connection is acknowledged with minimal delay. */
            if (aa_check_abort() || tagcache_is_busy())
            {
                aborted = true;
                break;
            }
            aa_generate_one(aa_artpath, s, dh, workbuf, worksz);
            yield();
        }
        if (aborted)
            break;

next:
        if (++since_yield >= 16)
        {
            since_yield = 0;
            yield();
        }
    }
    tagcache_search_finish(&tcs);
#ifdef HAVE_ADJUSTABLE_CPU_FREQ
    cpu_boost(false); /* balances the boost above (skipped on the goto-out path) */
#endif

out:
    core_unpin(wh);
    core_unpin(sh);
    core_free(sh);
    core_free(wh);
    cache_busy = false;
    return !aborted;
}

static void aa_thread(void)
{
    struct queue_event ev;
    int done_total = -1;   /* entry count we've completed a full pass for */
    int prev_total = -1;   /* entry count seen last tick (stability check) */

    while (1)
    {
        queue_wait_w_tmo(&aa_queue, &ev, HZ * 5);

        switch (ev.id)
        {
            case SYS_USB_CONNECTED:
                usb_acknowledge(SYS_USB_CONNECTED_ACK, ev.data);
                usb_wait_for_disconnect(&aa_queue);
                break;

            case SYS_TIMEOUT:
            {
                int total;
                /* Wait until the database is fully usable and NOT building.
                 * Reset the stability tracker while it's busy so we always
                 * re-confirm the count settled before scanning -- this keeps
                 * generation (and the %lc "Caching" indicator) off during the
                 * whole build instead of churning through partial states. */
                if (!tagcache_is_usable() || tagcache_is_busy())
                {
                    prev_total = -1;
                    break;
                }
                total = tagcache_get_stat()->total_entries;
                /* Require the count to be stable across two consecutive checks
                 * before doing anything, so a still-growing/rebuilding database
                 * never triggers a pass mid-flight. */
                if (total != prev_total)
                {
                    prev_total = total;
                    break;
                }
                if (total == done_total)
                    break; /* already fully cached for this database */
                if (aa_run_pass())
                    done_total = total; /* settled + done -> go idle */
                break;
            }

            default:
                break;
        }
    }
}

void albumart_cache_init(void)
{
    cache_busy = false;
    queue_init(&aa_queue, true);
    aa_thread_id = create_thread(aa_thread, aa_stack, sizeof(aa_stack), 0,
                                 aa_thread_name IF_PRIO(, PRIORITY_BACKGROUND)
                                 IF_COP(, CPU));
    (void)aa_thread_id;
}

#endif /* HAVE_ALBUMART */
