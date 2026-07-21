/* was: apps/gui/pitchscreen.c */
/***************************************************************************
 *             __________               __   ___.
 *   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
 *   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
 *   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
 *   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \
 *                     \/            \/     \/    \/            \/
 * $Id$
 *
 * Copyright (C) 2002 Björn Stenberg
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
#include "pitchscreen.h"
#include "sound.h"
#include "dsp_proc_settings.h"

int gui_syncpitchscreen_run(void)
{
    /* The pitch screen UI was a plugin (pitch_screen.rock); with the plugin
     * system gone it is not available. reset_pitch() below still works. */
    return 0;
}

int reset_pitch(void)
{
    sound_set_pitch(PITCH_SPEED_100);
    dsp_set_timestretch(PITCH_SPEED_100);
    return 0;
}
