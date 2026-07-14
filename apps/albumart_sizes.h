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

#ifndef _ALBUMART_SIZES_H_
#define _ALBUMART_SIZES_H_

/* How a source image is fitted into the square NxN thumbnail.
 *
 * AA_FIT_CONTAIN : scale to fit inside NxN preserving aspect (letterbox).
 *                  Square sources come out exactly NxN.
 * AA_FIT_COVER   : scale so the shorter side == N, then centre-crop the
 *                  longer side to N (fills the square, crops non-square art).
 *
 * NOTE: AA_FIT_COVER is not implemented yet -- the generator currently treats
 * every size as CONTAIN. The enum value exists so the table and consumers can
 * already express the intent; COVER is a focused follow-up.
 */
enum albumart_fit
{
    AA_FIT_CONTAIN = 0,
    AA_FIT_COVER,
};

struct albumart_size
{
    const char *name;       /* cache sub-folder name / stable identifier   */
    short       dim;        /* target square edge in pixels (NxN)          */
    enum albumart_fit fit;  /* how to fit the source into the square       */
};

/* The set of thumbnail resolutions the cache generates. Edit this table to
 * add/remove/resize cached thumbnails. Keep ALBUMART_CACHE_MAX_DIM in sync
 * with the largest 'dim' below -- it sizes the decode work buffer. */
static const struct albumart_size albumart_sizes[] =
{
    { "coverflow", 128, AA_FIT_COVER   },
    /* Add more sizes here once something actually consumes them (e.g. a list
     * thumbnail: { "list", 32, AA_FIT_COVER }). NOTE: each extra size means an
     * extra source-image decode per album during generation, so only enable a
     * size when there's a consumer -- ideally generate the largest size first
     * and downscale the smaller ones from it rather than re-decoding. */
};

#define ALBUMART_CACHE_NUM_SIZES \
    ((int)(sizeof(albumart_sizes) / sizeof(albumart_sizes[0])))

/* Must be >= the largest 'dim' in albumart_sizes[] above. */
#define ALBUMART_CACHE_MAX_DIM 128

#endif /* _ALBUMART_SIZES_H_ */
