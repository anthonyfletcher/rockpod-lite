/***************************************************************************
 *             __________               __   ___.
 *   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
 *   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
 *   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
 *   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \
 *                     \/            \/     \/    \/            \/
 *
 * Public entry point for the core-linked video viewer, ported from the
 * mpegplayer plugin. Kept separate from the internal mpegplayer.h (which
 * defines status codes and pulls in the whole engine) so call sites in the
 * file browser can use it with a minimal include.
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

#ifndef _VIDEO_VIEWER_H_
#define _VIDEO_VIEWER_H_

/* Open `file` (mpg/mpeg/mpv/m2v) in the full-screen MPEG video viewer.
 * Returns a GO_TO_* code. */
int video_viewer(const char *file);

#endif /* _VIDEO_VIEWER_H_ */
