/* was: apps/recorder/spectrum_meter.h */
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

#ifndef __SPECTRUM_METER_H__
#define __SPECTRUM_METER_H__

#define SPECTRUM_FPS 10
#define SPECTRUM_MAX_BANDS 8

/* Recomputes all band levels from the current playback PCM buffer. Meant
 * to be called every tick from skin_wait_for_action(), the same way
 * peak_meter_peek() is. Cheap no-op if too little fresh audio data is
 * available since the last call. */
void spectrum_meter_peek(void);

/* Returns a 0-100 smoothed level for bar 'bar' (0-based) out of 'nbars'
 * total bars. 'nbars' is clamped to SPECTRUM_MAX_BANDS. */
int spectrum_meter_get_bar(int bar, int nbars);

#endif /* __SPECTRUM_METER_H__ */
