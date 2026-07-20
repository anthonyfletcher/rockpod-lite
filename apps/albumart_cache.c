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
#include "bitmaps/no_album_cover.h" /* compiled-in placeholder for aa_ensure_fallback */
#include "bitmaps/rockpodnoartistcover.h" /* artist placeholder (silhouette) */
#include "jpeg_load.h"
#include "usb.h"
#include "events.h"
#include "appevents.h"
#include "audio.h"
#include "cpu.h"

/* Define LOGF_ENABLE to enable logf output in this file */
/*#define LOGF_ENABLE*/
#include "logf.h"

#define THUMBCACHE_DIR ROCKBOX_DIR "/thumbcache"
#define AA_VERSION_FILE THUMBCACHE_DIR "/format.txt"

/* On-disk thumbnail format (struct albumart_cache_header + row-major native
 * pixels) is declared in albumart_cache.h so consumers can read it. A
 * magic/version lets a future format change be detected per file rather than
 * needing a global cache wipe. */

/* Directory-dedup "seen" set: open-addressed table of directory-path hashes.
 * Sized generously; if a library exceeds this, extra directories simply get
 * re-resolved (still idempotent -- generation skips existing thumbnails). Holds
 * both album folders and their parent (artist) folders now, so it carries more
 * entries per pass than album-only did. */
#define AA_SEEN_SLOTS 16384  /* power of two */

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
static char aa_artist_dir[MAX_PATH];
static char aa_probe[MAX_PATH];
static char aa_check_path[MAX_PATH];
static char aa_out_path[MAX_PATH];

/* Queue event id: the current track's embedded art was offered for caching. */
#define AA_EVENT_OFFER 1

/* Filled by the track-change hook (playback thread), consumed by the aa thread.
 * Rockbox is cooperatively scheduled, so the two never run at once and no lock is
 * needed; a newer offer simply supersedes one not yet processed. */
static struct
{
    char          path[MAX_PATH];
    off_t         pos;
    unsigned long size;
    int           flags;
} aa_offer;

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

/* The shared placeholder thumbnail for a size. The "_" prefix cannot collide
 * with an %08x hash filename, so it lives alongside the real thumbnails. */
static void aa_fallback_path(char *out, int out_len, int size_index)
{
    snprintf(out, out_len, THUMBCACHE_DIR "/%s/_fallback.aat",
             albumart_sizes[size_index].name);
}

/* As aa_fallback_path, but the artist placeholder (a silhouette) -- a separate
 * file so artist rows never show the album "?" art. */
static void aa_artist_fallback_path(char *out, int out_len, int size_index)
{
    snprintf(out, out_len, THUMBCACHE_DIR "/%s/_artist_fallback.aat",
             albumart_sizes[size_index].name);
}

bool albumart_cache_artist_fallback(int size_index, char *out, int out_len)
{
    if (size_index < 0 || size_index >= ALBUMART_CACHE_NUM_SIZES)
        return false;
    aa_artist_fallback_path(out, out_len, size_index);
    return file_exists(out);
}

bool albumart_cache_lookup(const char *dir, int size_index,
                           char *out, int out_len, bool *is_fallback)
{
    if (is_fallback)
        *is_fallback = false;
    if (!dir || size_index < 0 || size_index >= ALBUMART_CACHE_NUM_SIZES)
        return false;

    aa_cache_path(out, out_len, size_index, aa_hash(dir));
    if (file_exists(out))
        return true;

    /* No real art for this folder -- hand back the placeholder so callers don't
     * each have to draw their own "missing art" state. Absent until the cache
     * has generated it (early boot), in which case this returns false and the
     * caller falls back to whatever it did before. */
    aa_fallback_path(out, out_len, size_index);
    if (file_exists(out))
    {
        if (is_fallback)
            *is_fallback = true;
        return true;
    }
    return false;
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

/* Delete every cached thumbnail of every size (the directories themselves stay).
 * Used only when the on-disk format changes. */
static void aa_purge_thumbs(void)
{
    int i;
    char dirpath[MAX_PATH];
    char filepath[MAX_PATH];

    for (i = 0; i < ALBUMART_CACHE_NUM_SIZES; i++)
    {
        DIR *d;
        struct dirent *e;

        snprintf(dirpath, sizeof(dirpath), THUMBCACHE_DIR "/%s",
                 albumart_sizes[i].name);
        d = opendir(dirpath);
        if (!d)
            continue;

        while ((e = readdir(d)))
        {
            if (e->d_name[0] == '.')
                continue;
            snprintf(filepath, sizeof(filepath), "%s/%s", dirpath, e->d_name);
            remove(filepath);
            yield();
        }
        closedir(d);
    }
}

/* The generator decides "already cached?" with a bare file_exists(), and the
 * reader rejects any file whose header version doesn't match. So on a format
 * bump the stale files would be skipped forever *and* refused at render time -
 * every cover would silently go blank. Stamp the format version alongside the
 * cache and purge the thumbnails whenever it moves. */
static void aa_check_format_version(void)
{
    char buf[16];
    int fd, n, ver = -1;

    fd = open(AA_VERSION_FILE, O_RDONLY);
    if (fd >= 0)
    {
        n = read(fd, buf, sizeof(buf) - 1);
        if (n > 0)
        {
            buf[n] = '\0';
            ver = atoi(buf);
        }
        close(fd);
    }

    if (ver == ALBUMART_CACHE_FORMAT_VERSION)
        return;

    logf("albumart cache: format %d -> %d, purging", ver,
         ALBUMART_CACHE_FORMAT_VERSION);
    aa_purge_thumbs();

    fd = open(AA_VERSION_FILE, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (fd >= 0)
    {
        n = snprintf(buf, sizeof(buf), "%d\n", ALBUMART_CACHE_FORMAT_VERSION);
        write(fd, buf, n);
        close(fd);
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

/* Where an album-art image comes from: a file on disk (folder art), or a JPEG
 * blob embedded in an audio file (emb_pos >= 0, reusing metadata already parsed
 * by playback -- see albumart_cache_offer_current()). */
struct aa_src
{
    const char   *path;      /* file to read: the folder image, or the track */
    off_t         emb_pos;   /* embedded JPEG offset, or -1 for a whole file */
    unsigned long emb_size;  /* embedded JPEG blob length (embedded only) */
    int           emb_flags; /* embedded aa type/flags for clip_jpeg_fd */
};

/* Read the source image into `bm` with `fmt`, requesting a `w` x `h` target (the
 * readers treat that as the resize target, or overwrite it with the source's own
 * dimensions when `fmt` carries no FORMAT_RESIZE). Returns the reader's result. */
static int aa_read_source(const struct aa_src *src, struct bitmap *bm, int fmt,
                          int w, int h, void *workbuf, size_t workbuf_sz)
{
    memset(bm, 0, sizeof(*bm));
    bm->data = workbuf;
    bm->width = w;
    bm->height = h;
    bm->format = FORMAT_NATIVE;

    if (src->emb_pos >= 0)
    {
        /* Embedded art -- always JPEG in Rockbox. lseek + clip_jpeg_fd is how
         * the WPS decodes it, and it handles the ID3-unsync flag in emb_flags. */
        int fd = open(src->path, O_RDONLY);
        int rc;
        if (fd < 0)
            return -1;
        lseek(fd, src->emb_pos, SEEK_SET);
        rc = clip_jpeg_fd(fd, src->emb_flags, src->emb_size, bm,
                          (int)workbuf_sz, fmt, NULL);
        close(fd);
        return rc;
    }

    size_t namelen = strlen(src->path);
    if (namelen >= 4 && strcmp(src->path + namelen - 4, ".bmp") != 0)
    {
        return read_jpeg_file(src->path, bm, (int)workbuf_sz, fmt, NULL);
    }
    return read_bmp_file(src->path, bm, (int)workbuf_sz, fmt, NULL);
}

/* Target size for an AA_FIT_COVER decode: scale so the SHORTER side lands on
 * exactly `dim`, leaving the longer side >= dim to be cropped away. False if the
 * source is too elongated to stage in the work buffer (caller falls back to
 * CONTAIN) or its dimensions are nonsense. */
static bool aa_cover_dim(int sw, int sh, int dim, int *tw, int *th)
{
    if (sw <= 0 || sh <= 0)
        return false;

    if (sw <= sh)
    {
        *tw = dim;
        *th = (sh * dim + sw / 2) / sw;     /* rounded; >= dim */
    }
    else
    {
        *th = dim;
        *tw = (sw * dim + sh / 2) / sh;
    }

    /* rounding must never leave the short side under dim -- the crop below
     * assumes both sides are at least dim */
    if (*tw < dim)
        *tw = dim;
    if (*th < dim)
        *th = dim;

    return *tw <= dim * ALBUMART_CACHE_COVER_MAX_ASPECT &&
           *th <= dim * ALBUMART_CACHE_COVER_MAX_ASPECT;
}

/* Centre-crop a tw x th image down to dim x dim, in place. Each output row sits
 * at or before its source row (dim <= tw), so copying front-to-back is safe. */
static void aa_crop_center(void *buf, int tw, int th, int dim)
{
    fb_data *px = buf;
    int x0 = (tw - dim) / 2;
    int y0 = (th - dim) / 2;
    int y;

    for (y = 0; y < dim; y++)
        memmove(px + (size_t)y * dim,
                px + (size_t)(y + y0) * tw + x0,
                (size_t)dim * FB_DATA_SZ);
}

/* Write a native (row-major fb_data) bitmap to a .aat file: the shared header
 * followed by the pixels. Removes the file on a short write. */
static bool aa_write_aat(const char *out_path, const struct bitmap *bm)
{
    struct albumart_cache_header hdr;
    size_t bytes;
    bool ok;
    int fd = open(out_path, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (fd < 0)
        return false;

    hdr.magic = ALBUMART_CACHE_MAGIC;
    hdr.version = ALBUMART_CACHE_FORMAT_VERSION;
    hdr.width = bm->width;
    hdr.height = bm->height;
    bytes = (size_t)bm->width * bm->height * FB_DATA_SZ;

    ok = (write(fd, &hdr, sizeof(hdr)) == (ssize_t)sizeof(hdr)) &&
         (write(fd, bm->data, bytes) == (ssize_t)bytes);
    close(fd);

    if (!ok)
        remove(out_path);
    return ok;
}

/* Decode/scale the source art into a thumbnail and write it.
 *
 * CONTAIN: fitted (aspect preserved) inside dim x dim, so the cached image is
 * only square when the source is.
 * COVER:   decoded to the cover size (shorter side == dim) and centre-cropped to
 * exactly dim x dim. A source wider than ALBUMART_CACHE_COVER_MAX_ASPECT falls
 * back to CONTAIN rather than overrun the work buffer.
 *
 * Returns true on success. */
static bool aa_generate_one(const struct aa_src *src, int size_index,
                            const char *out_path, void *workbuf,
                            size_t workbuf_sz)
{
    int dim = albumart_sizes[size_index].dim;
    bool cover = albumart_sizes[size_index].fit == AA_FIT_COVER;
    struct bitmap bm;
    int fmt = FORMAT_NATIVE | FORMAT_RESIZE | FORMAT_DITHER;
    int tw = dim, th = dim;
    int ret;

    if (cover)
    {
        /* Header-only probe: with no FORMAT_RESIZE the readers report the
         * source's own dimensions, and FORMAT_RETURN_SIZE stops them decoding
         * any pixels. */
        ret = aa_read_source(src, &bm, FORMAT_NATIVE | FORMAT_RETURN_SIZE,
                             dim, dim, workbuf, workbuf_sz);
        if (ret <= 0 || !aa_cover_dim(bm.width, bm.height, dim, &tw, &th))
        {
            cover = false;      /* unreadable header, or too elongated to crop */
            tw = th = dim;
        }
    }

    /* KEEP_ASPECT is what makes a decode CONTAIN: it shrinks the requested box
     * to the source's aspect. COVER instead asks for the exact cover size it
     * computed above, so the decode must NOT keep aspect. */
    if (!cover)
        fmt |= FORMAT_KEEP_ASPECT;

    ret = aa_read_source(src, &bm, fmt, tw, th, workbuf, workbuf_sz);

    if (ret <= 0 || bm.width <= 0 || bm.height <= 0)
        return false;

    /* the decode should have landed on exactly tw x th, but never crop from an
     * image smaller than the crop window -- write what we got instead */
    if (cover && bm.width >= dim && bm.height >= dim)
    {
        aa_crop_center(bm.data, bm.width, bm.height, dim);
        bm.width = dim;
        bm.height = dim;
    }

    return aa_write_aat(out_path, &bm);
}

/* Area-average downscale of a native (fb_data) image. Used only for the
 * compiled-in placeholder, so it needs no upscale or aspect handling: the source
 * is square and always larger than the thumbnail sizes. */
static void aa_scale_native(const fb_data *src, int sw, int sh,
                            fb_data *dst, int dw, int dh)
{
    for (int dy = 0; dy < dh; dy++)
    {
        int sy0 = dy * sh / dh, sy1 = (dy + 1) * sh / dh;
        if (sy1 <= sy0)
            sy1 = sy0 + 1;
        for (int dx = 0; dx < dw; dx++)
        {
            int sx0 = dx * sw / dw, sx1 = (dx + 1) * sw / dw;
            unsigned r = 0, g = 0, b = 0, n = 0;
            if (sx1 <= sx0)
                sx1 = sx0 + 1;
            for (int sy = sy0; sy < sy1; sy++)
                for (int sx = sx0; sx < sx1; sx++)
                {
                    fb_data p = src[sy * sw + sx];
                    r += RGB_UNPACK_RED(p);
                    g += RGB_UNPACK_GREEN(p);
                    b += RGB_UNPACK_BLUE(p);
                    n++;
                }
            dst[dy * dw + dx] = LCD_RGBPACK(r / n, g / n, b / n);
        }
    }
}

/* Downscale one compiled-in (square) placeholder bitmap into every size's
 * placeholder .aat, once. `pathfn` picks the per-size destination file. Cheap in
 * steady state (a file_exists per size); regenerated after a format bump because
 * the purge removes it. */
static void aa_render_placeholder(const fb_data *src, int sw, int sh,
                                  void (*pathfn)(char *, int, int), void *workbuf)
{
    int s;
    char path[MAX_PATH];

    for (s = 0; s < ALBUMART_CACHE_NUM_SIZES; s++)
    {
        int dim = albumart_sizes[s].dim;
        struct bitmap bm;

        pathfn(path, sizeof(path), s);
        if (file_exists(path))
            continue;

        aa_scale_native(src, sw, sh, (fb_data *)workbuf, dim, dim);
        bm.data = workbuf;
        bm.width = dim;
        bm.height = dim;
        bm.format = FORMAT_NATIVE;
        aa_write_aat(path, &bm);
        yield();
    }
}

/* Render the compiled-in placeholders (apps/bitmaps/native/) into each size's
 * placeholder .aat: the album "?" (no_album_cover) and the artist silhouette
 * (rockpodnoartistcover). Both sources are square, so COVER == a plain
 * downscale. */
static void aa_ensure_fallback(void *workbuf, size_t workbuf_sz)
{
    (void)workbuf_sz;
    aa_render_placeholder((const fb_data *)no_album_cover,
                          BMPWIDTH_no_album_cover, BMPHEIGHT_no_album_cover,
                          aa_fallback_path, workbuf);
    aa_render_placeholder((const fb_data *)rockpodnoartistcover,
                          BMPWIDTH_rockpodnoartistcover,
                          BMPHEIGHT_rockpodnoartistcover,
                          aa_artist_fallback_path, workbuf);
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

/* Resolve one folder's cover art and render any of its thumbnail sizes that
 * don't exist yet. `probe_path` is a track filename under the folder (real for
 * an album folder, synthetic "<dir>/_" for an artist folder) that
 * search_albumart_files() strips down to the folder to locate cover.bmp /
 * folder.jpg; `dh` is that folder's hash (the cache key). Sets *aborted if a
 * USB/shutdown/DB-busy stop was hit mid-decode. A cheap no-op once every size
 * already exists (dircache-served file_exists checks, no art re-resolution). */
static void aa_cache_dir(const char *probe_path, unsigned int dh,
                         void *workbuf, size_t worksz, bool *aborted)
{
    int s;
    bool all_exist = true;

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
        return;

    /* Only now (a thumbnail is missing) resolve this folder's cover art.
     * album/albumartist left NULL: only folder-based art is searched
     * (cover.bmp, folder.jpg, ../cover.bmp). */
    memset(&aa_id3, 0, sizeof(aa_id3));
    strlcpy(aa_id3.path, probe_path, sizeof(aa_id3.path));
    if (!search_albumart_files(&aa_id3, "", aa_artpath, sizeof(aa_artpath)))
        return;

    struct aa_src src = { aa_artpath, -1, 0, 0 };  /* folder image on disk */
    for (s = 0; s < ALBUMART_CACHE_NUM_SIZES; s++)
    {
        aa_cache_path(aa_check_path, sizeof(aa_check_path), s, dh);
        if (file_exists(aa_check_path))
            continue;
        /* Re-check right before a (potentially slow) decode so a USB
         * connection is acknowledged with minimal delay. */
        if (aa_check_abort() || tagcache_is_busy())
        {
            *aborted = true;
            return;
        }
        aa_cache_path(aa_out_path, sizeof(aa_out_path), s, dh);
        aa_generate_one(&src, s, aa_out_path, workbuf, worksz);
        yield();
    }
}

/* One full generation pass: walk every track filename, dedup by directory,
 * resolve folder art, and render any missing thumbnails -- both the track's own
 * (album) folder and its parent (artist) folder, for libraries laid out as
 * <artist>/<album>/<track>. Returns true if the pass ran to completion, false if
 * it was aborted (USB/DB busy/no memory) and should be retried later. */
static bool aa_run_pass(void)
{
    struct tagcache_search tcs;
    size_t worksz;
    int wh, sh;
    void *workbuf;
    unsigned int *seen;
    bool aborted = false;
    int since_yield = 0;

    /* An AA_FIT_COVER decode stages a non-square image (shorter side == dim,
     * longer side up to dim * COVER_MAX_ASPECT) before cropping it square, so
     * the buffer is sized for that worst case. Elongation on the other axis has
     * the same pixel count and a narrower row, so this covers both. */
    worksz = BM_SCALED_SIZE(ALBUMART_CACHE_MAX_DIM *
                                ALBUMART_CACHE_COVER_MAX_ASPECT,
                            ALBUMART_CACHE_MAX_DIM, FORMAT_NATIVE, 0);
    worksz += JPEG_DECODE_OVERHEAD;

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
    aa_check_format_version();
    aa_ensure_fallback(workbuf, worksz);

    if (!tagcache_search(&tcs, tag_filename))
    {
        aborted = true;
        goto out;
    }

    /* A pass is running -> lights the status-bar %lc ("Caching") token. */
    cache_busy = true;
    /* Speed up the (CPU-bound) image decoding, like Cover Flow's own
     * generator does. Passes are rare (only after the database settles or
     * changes), so the extra clock is a brief one-off. */
    cpu_boost(true);

    while (tagcache_get_next(&tcs, aa_tcs_buf, sizeof(aa_tcs_buf)))
    {
        unsigned int dh, ah;

        /* Abort promptly on USB/shutdown (the thread loop then acknowledges)
         * or yield the disk to an incoming database commit, so a long pass
         * never blocks either. */
        if (aa_check_abort() || tagcache_is_busy())
        {
            aborted = true;
            break;
        }

        /* The track's own folder (the album), keyed by its path hash. Skip the
         * per-folder work entirely once this folder has been visited this pass
         * (aa_seen records it), so later tracks of the same album are cheap. */
        aa_dirname(tcs.result, aa_dir, sizeof(aa_dir));
        dh = aa_hash(aa_dir);
        if (!aa_seen(seen, dh))
            aa_cache_dir(tcs.result, dh, workbuf, worksz, &aborted);
        if (aborted)
            break;

        /* The parent folder (the artist), cached from <artist>/folder.jpg etc.
         * for <artist>/<album>/<track> layouts. Deduped independently of the
         * album so an artist whose first album has no cover still gets resolved.
         * Skipped when there is no distinct parent (flat/rooted layouts). */
        aa_dirname(aa_dir, aa_artist_dir, sizeof(aa_artist_dir));
        if (aa_artist_dir[0] && strcmp(aa_artist_dir, aa_dir) != 0)
        {
            ah = aa_hash(aa_artist_dir);
            if (!aa_seen(seen, ah))
            {
                snprintf(aa_probe, sizeof(aa_probe), "%s/_", aa_artist_dir);
                aa_cache_dir(aa_probe, ah, workbuf, worksz, &aborted);
            }
        }
        if (aborted)
            break;

        if (++since_yield >= 16)
        {
            since_yield = 0;
            yield();
        }
    }
    tagcache_search_finish(&tcs);
    cpu_boost(false); /* balances the boost above (skipped on the goto-out path) */

out:
    core_unpin(wh);
    core_unpin(sh);
    core_free(sh);
    core_free(wh);
    cache_busy = false;
    return !aborted;
}

/* Cache the offered track's embedded art into any of its folder's thumbnails
 * that don't exist yet. Fill-only: never overwrites art already cached (folder
 * images are the preferred on-disk source) -- this just gives a coverless folder
 * the art playback already had parsed for the WPS, at no decode cost to it. */
static void aa_handle_offer(void)
{
    char path[MAX_PATH];
    char dir[MAX_PATH];
    unsigned int dh;
    size_t worksz;
    int s, wh;
    void *workbuf;
    struct aa_src src;
    bool need = false;

    if (aa_offer.path[0] == '\0' || !tagcache_is_usable())
        return;

    /* Copy the path out before any yield so a newer offer can't move it. */
    strlcpy(path, aa_offer.path, sizeof(path));
    src.path = path;
    src.emb_pos = aa_offer.pos;
    src.emb_size = aa_offer.size;
    src.emb_flags = aa_offer.flags;

    aa_dirname(path, dir, sizeof(dir));
    dh = aa_hash(dir);

    for (s = 0; s < ALBUMART_CACHE_NUM_SIZES; s++)
    {
        aa_cache_path(aa_check_path, sizeof(aa_check_path), s, dh);
        if (!file_exists(aa_check_path))
        {
            need = true;
            break;
        }
    }
    if (!need)
        return;   /* already cached (folder art wins) */

    worksz = BM_SCALED_SIZE(ALBUMART_CACHE_MAX_DIM *
                                ALBUMART_CACHE_COVER_MAX_ASPECT,
                            ALBUMART_CACHE_MAX_DIM, FORMAT_NATIVE, 0);
    worksz += JPEG_DECODE_OVERHEAD;
    wh = core_alloc(worksz);
    if (wh <= 0)
        return;
    workbuf = core_get_data_pinned(wh);

    aa_ensure_dirs();
    for (s = 0; s < ALBUMART_CACHE_NUM_SIZES; s++)
    {
        aa_cache_path(aa_check_path, sizeof(aa_check_path), s, dh);
        if (file_exists(aa_check_path))
            continue;
        aa_cache_path(aa_out_path, sizeof(aa_out_path), s, dh);
        aa_generate_one(&src, s, aa_out_path, workbuf, worksz);
        yield();
    }

    core_unpin(wh);
    core_free(wh);
}

/* Playback thread: a track became current. If it carries embedded JPEG art, hand
 * its location to the aa thread (reusing the metadata playback already parsed) --
 * no decoding here. */
static void aa_track_change_cb(unsigned short id, void *event_data)
{
    (void)id; (void)event_data;
    struct mp3entry *id3 = audio_current_track();
    if (!id3 || !id3->has_embedded_albumart ||
        (id3->albumart.type & AA_CLEAR_FLAGS_MASK) != AA_TYPE_JPG)
        return;

    strlcpy(aa_offer.path, id3->path, sizeof(aa_offer.path));
    aa_offer.pos = id3->albumart.pos;
    aa_offer.size = id3->albumart.size;
    aa_offer.flags = id3->albumart.type;
    queue_post(&aa_queue, AA_EVENT_OFFER, 0);
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

            case AA_EVENT_OFFER:
                aa_handle_offer();
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

    /* Opportunistically cache the embedded art of tracks as they play, for
     * folders that have no on-disk cover (fill-only -- see aa_handle_offer). */
    add_event(PLAYBACK_EVENT_TRACK_CHANGE, aa_track_change_cb);
}

