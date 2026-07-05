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

/* Releases all cached art and its backing buffers, e.g. when leaving the
 * Database browser. */
void list_albumart_clear_cache(void);

#endif /* HAVE_ALBUMART */
#endif /* _LIST_ALBUMART_H_ */
