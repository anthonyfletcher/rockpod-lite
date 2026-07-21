/* was: apps/misc.h (volume and replaygain) */
/***************************************************************************
 *             __________               __   ___.
 *   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
 *   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
 *   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
 *   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \
 *                     \/            \/     \/    \/            \/
 * $Id$
 *
 * Copyright (C) 2002 by Daniel Stenberg
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

#ifndef _VOLUME_H_
#define _VOLUME_H_

#include <stdbool.h>
#include <stddef.h>

enum volume_adjust_mode
{
    VOLUME_ADJUST_DIRECT,       /* adjust in units of the volume step size */
    VOLUME_ADJUST_PERCEPTUAL,   /* adjust using perceptual steps */
};

/* min/max values for global_settings.volume_adjust_norm_steps */
#define MIN_NORM_VOLUME_STEPS 10
#define MAX_NORM_VOLUME_STEPS 100

/* check range, set volume and save settings */
void setvol(void);
void set_normalized_volume(int vol);
int get_normalized_volume(void);
void adjust_volume(int steps);
void adjust_volume_ex(int steps, enum volume_adjust_mode mode);

/* Return current ReplayGain mode a file should have (REPLAYGAIN_TRACK or
 * REPLAYGAIN_ALBUM) if ReplayGain processing is enabled, or -1 if no
 * information present.
 */
struct mp3entry;
int id3_get_replaygain_mode(const struct mp3entry *id3);
void replaygain_update(void);

/* Format a sound value like: "-1.05 dB"    (negative values)
 *                            " 1.05 dB"    (positive values include leading space)
 */
void format_sound_value(char *buf, size_t buf_sz, int snd, int val);

/* Set skin_token parameter to true to format a sound value for
 * display in themes, like:   "-1.05"       (negative values)
 *                            "1.05"        (positive values without leading space)
 *
 * (The new formatting includes a unit based on the AUDIOHW_SETTING
 * definition -- on all targets, it's defined to be "dB". But the
 * old formatting was just an integer value, and many themes append
 * "dB" manually. So we need to strip the unit to unbreak all those
 * existing themes.)
 */
void format_sound_value_ex(char *buf, size_t buf_sz, int snd, int val, bool skin_token);

/* Convert a volume (in tenth dB) in the range [min_vol, max_vol]
 * to a normalized linear value in the range [0, max_norm]. */
long to_normalized_volume(long vol, long min_vol, long max_vol, long max_norm);

/* Inverse of to_normalized_volume(), returns the volume in tenth dB
 * for the given normalized volume. */
long from_normalized_volume(long norm, long min_vol, long max_vol, long max_norm);


#endif /* _VOLUME_H_ */
