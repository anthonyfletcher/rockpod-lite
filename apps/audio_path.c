/***************************************************************************
 *             __________               __   ___.
 *   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
 *   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
 *   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
 *   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \
 *                     \/            \/     \/    \/            \/
 * $Id$
 *
 * Audio signal path management APIs
 *
 * Copyright (C) 2007 by Michael Sevakis
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
#include "system.h"
#include "cpu.h"
#include "audio.h"
#include "general.h"
#include "settings.h"

/* Some audio sources may require a boosted CPU */



/**
 * Selects an audio source for recording or playback
 * powers/unpowers related devices and sets up monitoring.
 */
void audio_set_input_source(int source, unsigned flags)
{
    /** Do power up/down of associated device(s) **/

    /** SPDIF **/

    /* Set the appropriate feed for spdif output */

    /* set hardware inputs */
    audio_input_mux(source, flags);
} /* audio_set_source */



