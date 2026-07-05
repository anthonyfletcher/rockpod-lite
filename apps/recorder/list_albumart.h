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

#ifndef _LIST_ALBUMART_H_
#define _LIST_ALBUMART_H_

#ifdef HAVE_ALBUMART

#include <stdbool.h>
#include "skin_engine/skin_engine.h" /* struct dim */

/* Default thumbnail size assumed by the bulk cache builder
 * (apps/tagtree.c's tagtree_build_albumart_cache()), matching what
 * Themify_2's Album_List/No_Scroll_Album_List viewports currently request
 * via %La. A theme requesting a different %La size simply won't benefit
 * from the pre-built cache -- its rows fall back to the existing on-demand
 * path instead, still correct, just not pre-warmed. */
#define LIST_ALBUMART_DEFAULT_WIDTH  48
#define LIST_ALBUMART_DEFAULT_HEIGHT 48

/* Look up album art for a browsed (not necessarily playing) track, for
 * use by list rows. 'row_key' should be a value that stays stable for
 * the same album across redraws (tagtree's per-row extraseek works) so
 * repeated lookups for a visible row hit the cache instead of re-decoding.
 *
 * Decoding a cache miss happens synchronously inline, so the return value
 * is always final: NULL means no art exists for this row, non-NULL is a
 * ready-to-draw bitmap. The returned pointer is only valid until the next
 * call into this module (the backing buffer can be reused by the LRU
 * cache), so callers must draw with it immediately rather than holding
 * onto it.
 */
const struct bitmap *list_albumart_get_bitmap(const char *path,
                                               const char *album,
                                               const char *albumartist,
                                               int row_key,
                                               const struct dim *requested_size);

/* Checks whether 'row_key' has already been resolved (found or confirmed
 * absent), without touching tagcache or the filesystem. Callers MUST check
 * this before resolving path/album/artist for list_albumart_get_bitmap(),
 * since that resolution costs real tagcache disk queries -- a themed list
 * redraws a row far more often than its underlying album actually changes,
 * so skipping resolution entirely on an already-cached row is the
 * difference between a one-time cost per album and paying it on every
 * redraw. Returns true and sets *out (possibly NULL, meaning confirmed no
 * art) if cached; returns false if row_key is unknown and the caller needs
 * to resolve and call list_albumart_get_bitmap(). */
bool list_albumart_is_cached(int row_key, const struct bitmap **out);

/* Releases all cached art and its backing buffers, e.g. when leaving the
 * Database browser. */
void list_albumart_clear_cache(void);

/* Wipes the on-disk pfraw cache (apps/recorder/list_albumart.c's AA_CACHE_DIR)
 * and the RAM cache above, e.g. from a settings-menu "Clear Album Art Cache"
 * action. Each cached file is keyed by (album, albumartist, size), so
 * switching to a theme that requests a different %La size never serves stale
 * art -- it just leaves the old size's files as unused disk space, which is
 * what this is for. Also marks the cache incomplete so the next visit to the
 * Albums screen rebuilds it (see tagtree_build_albumart_cache()). */
void list_albumart_clear_disk_cache(void);

/* Bulk-build support (apps/tagtree.c's tagtree_build_albumart_cache()) --
 * ensures art for one album is disk-cached at 'size' without touching the
 * small browse-time RAM cache (aa_cache[] above is sized/LRU-managed for
 * currently-visible rows, not a sequential pass over the whole library).
 * No-ops quickly if this album is already cached, so re-running a bulk
 * build only does real work for newly-added albums. */
void list_albumart_precache_one(const char *path, const char *album,
                                 const char *albumartist,
                                 const struct dim *size);

/* Whether a prior tagtree_build_albumart_cache() ran to completion without
 * being aborted. */
bool list_albumart_cache_is_complete(void);
void list_albumart_cache_mark_complete(void);
void list_albumart_cache_mark_incomplete(void);

#endif /* HAVE_ALBUMART */
#endif /* _LIST_ALBUMART_H_ */
