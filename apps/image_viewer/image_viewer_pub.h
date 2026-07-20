/***************************************************************************
 *             __________               __   ___.
 *   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
 *   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
 *   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
 *   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \
 *                     \/            \/     \/    \/            \/
 *
 * Public entry point for the core-linked image viewer. Kept separate from
 * image_viewer.h (which defines internal status codes that would clash with
 * plugin.h) so call sites that also include plugin.h can use it.
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

#ifndef _IMAGE_VIEWER_PUB_H_
#define _IMAGE_VIEWER_PUB_H_

/* Open `file` (jpg/png/bmp/gif/ppm) in the full-screen image viewer, or the
 * current track's album art when `file` is NULL. Returns a GO_TO_* code. */
int image_viewer(const char *file);

#endif /* _IMAGE_VIEWER_PUB_H_ */
