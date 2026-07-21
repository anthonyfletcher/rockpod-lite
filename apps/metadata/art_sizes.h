/***************************************************************************
 * RockPod-Lite
 *
 * Original code from RockBox
 * was: apps/albumart_sizes.h
 * Copyright (C) 2026 Rockpod
 * GNU General Public License (version 2+)
 *
 * The album-art sizes this build caches, derived from the skins in use.
 ****************************************************************************/

#ifndef _ART_SIZES_H_
#define _ART_SIZES_H_

/* How a source image is fitted into the square NxN thumbnail.
 *
 * AA_FIT_CONTAIN : scale to fit inside NxN preserving aspect (letterbox). The
 *                  cached image is therefore NOT square for non-square sources;
 *                  its real dimensions are in the cache header.
 * AA_FIT_COVER   : scale so the shorter side == N, then centre-crop the longer
 *                  side to N. Always yields exactly NxN, cropping non-square art.
 *
 * COVER needs a non-square intermediate (N x N*aspect) before the crop, so it is
 * only applied while the source's aspect ratio is within
 * ART_CACHE_COVER_MAX_ASPECT; a more elongated source falls back to CONTAIN
 * rather than blow up the decode work buffer. Album art beyond 2:1 is vanishingly
 * rare, so raise the cap only if you actually hit it (it costs work-buffer bytes).
 */
enum albumart_fit
{
    AA_FIT_CONTAIN = 0,
    AA_FIT_COVER,
};

struct art_size
{
    const char *name;       /* cache sub-folder name / stable identifier   */
    short       dim;        /* target square edge in pixels (NxN)          */
    enum albumart_fit fit;  /* how to fit the source into the square       */
};

/* The set of thumbnail resolutions the cache generates. Edit this table to
 * add/remove/resize cached thumbnails. Keep ART_CACHE_MAX_DIM in sync
 * with the largest 'dim' below -- it sizes the decode work buffer. */
static const struct art_size art_sizes[] =
{
    { "coverflow", 128, AA_FIT_COVER   },
    { "list",       48, AA_FIT_COVER   },   /* database album rows (tree.c) */
    /* Add more sizes here once something actually consumes them.
     *
     * NOTE: each extra size costs one more source decode per album. Do NOT
     * "optimise" that by generating the largest size first and downscaling the
     * smaller ones from it: on downscale the image pipeline selects the
     * area-average scalers (scale_h_area/scale_v_area, apps/recorder/resize.c),
     * i.e. a true box filter, so scaling straight from the source is already
     * both correct and alias-free. Chaining would resample an already-resampled
     * grid and compound the error -- it would trade quality for decode time, not
     * gain detail. Chain only if generation time actually becomes a problem, and
     * do it knowing that's the trade. */
};

#define ART_CACHE_NUM_SIZES \
    ((int)(sizeof(art_sizes) / sizeof(art_sizes[0])))

/* Must be >= the largest 'dim' in art_sizes[] above. */
#define ART_CACHE_MAX_DIM 128

/* Widest source aspect ratio AA_FIT_COVER will crop; beyond this the size falls
 * back to CONTAIN. Sizes the decode work buffer (see aa_run_pass()), so raising
 * it costs memory during cache generation. */
#define ART_CACHE_COVER_MAX_ASPECT 2

#endif /* _ART_SIZES_H_ */
