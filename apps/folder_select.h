/***************************************************************************
 *             __________               __   ___.
 *   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
 *   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
 *   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
 *   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \
 *                     \/            \/     \/    \/            \/
 *
 * Public entry point for the core folder-tree picker (apps/folder_select.c),
 * ported from the db_folder_select plugin.
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

#ifndef _FOLDER_SELECT_H_
#define _FOLDER_SELECT_H_

#include <stdbool.h>

/* Show a folder tree under `header_text`; `setting` is a ':'-delimited list of
 * selected paths, loaded on entry and (after a "save changes?" prompt) written
 * back, up to `setting_len` bytes. Returns true if the setting was changed. */
bool folder_select(char *header_text, char *setting, int setting_len);

#endif /* _FOLDER_SELECT_H_ */
