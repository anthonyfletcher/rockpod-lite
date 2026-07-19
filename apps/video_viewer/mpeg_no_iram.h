/***************************************************************************
 *             __________               __   ___.
 *   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
 *   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
 *   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
 *   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \
 *                     \/            \/     \/    \/            \/
 *
 * The video viewer is linked into the core binary. Unlike the mpegplayer
 * plugin -- which had its own IRAM bank swapped in on load -- a core-linked
 * module shares the (small) core IRAM, and the decoder's code/data does not
 * fit there. Force all of the viewer's would-be-IRAM into DRAM by neutralizing
 * the IRAM section attributes. Include this AFTER config.h.
 *
 * NOCACHEBSS_ATTR / SHAREDBSS_ATTR are deliberately left alone: on the dual-
 * core PP5022 they place inter-processor data in uncached DRAM (not IRAM) and
 * are needed for correctness.
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

#ifndef MPEG_NO_IRAM_H
#define MPEG_NO_IRAM_H

#include "config.h"

#undef ICODE_ATTR
#define ICODE_ATTR
#undef ICONST_ATTR
#define ICONST_ATTR
#undef IDATA_ATTR
#define IDATA_ATTR
#undef IBSS_ATTR
#define IBSS_ATTR

#endif /* MPEG_NO_IRAM_H */
