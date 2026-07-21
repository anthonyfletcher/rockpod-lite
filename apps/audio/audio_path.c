/***************************************************************************
 * RockPod-Lite
 *
 * Original code from RockBox
 * was: apps/audio_path.c
 * Audio signal path management APIs
 *
 * Copyright (C) 2007 by Michael Sevakis
 * GNU General Public License (version 2+)
 *
 * Selects the audio input source. Vestigial on these targets, which have
 * no recording or line-in.
 ****************************************************************************/
#include "system.h"
#include "cpu.h"
#include "audio.h"
#include "general.h"
#include "settings/settings.h"

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



