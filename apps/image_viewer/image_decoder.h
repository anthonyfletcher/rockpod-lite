/***************************************************************************
 *             __________               __   ___.
 *   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
 *   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
 *   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
 *   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \
 *                     \/            \/     \/    \/            \/
 * $Id$
 *
 * Image decoder selection (core build).
 *
 * The plugin loaded each format's decoder as a separate .ovl overlay; the core
 * build links them all in and picks one from a static table by type.
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

#ifndef _IMAGE_DECODER_H
#define _IMAGE_DECODER_H

#include "image_viewer.h"

enum image_type {
    IMAGE_UNKNOWN = -1,
    IMAGE_BMP = 0,
    IMAGE_JPEG,
    IMAGE_PNG,
    IMAGE_PPM,
    IMAGE_GIF,
    IMAGE_JPEG_PROGRESSIVE,

    MAX_IMAGE_TYPES
};

/* Check file type by magic number or file extension */
enum image_type get_image_type(const char *name, bool quiet);
/* Get the decoder for a given image type (static table lookup). */
const struct image_decoder *get_image_decoder(enum image_type type);

/* Each decoder exports its own struct image_decoder. */
extern const struct image_decoder bmp_decoder;
extern const struct image_decoder jpeg_decoder;
extern const struct image_decoder jpegp_decoder;
extern const struct image_decoder png_decoder;
extern const struct image_decoder ppm_decoder;
extern const struct image_decoder gif_decoder;

#endif /* _IMAGE_DECODER_H */
