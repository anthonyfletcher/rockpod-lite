/* was: apps/text_viewer/text_viewer.h */
/***************************************************************************
 *             __________               __   ___.
 *   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
 *   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
 *   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
 *   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \
 *                     \/            \/     \/    \/            \/
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
#ifndef _TEXT_VIEWER_H_
#define _TEXT_VIEWER_H_

/* Core-linked document viewer, built on the ts_* extraction engine (see
 * txt_source.h). Opens `file` -- plain text, markdown, html, rtf, fb2, epub,
 * docx or pdf -- and pages through its text. Returns a GO_TO_* code. */
int text_viewer(const char *file);

#endif /* _TEXT_VIEWER_H_ */
