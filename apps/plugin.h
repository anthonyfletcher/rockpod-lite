/***************************************************************************
 *             __________               __   ___.
 *   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
 *   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
 *   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
 *   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \
 *                     \/            \/     \/    \/            \/
 *
 * Copyright (C) 2002 by Björn Stenberg
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

/*
 * Scaffolding only -- deliberately empty.
 *
 * The plugin system is gone from this fork. This header once carried the
 * plugin API struct, the loader declarations and a block of ~60 convenience
 * includes; all of it was dead and has been removed. Its two genuinely live
 * exports moved out:
 *
 *   plugin_get_buffer()   -> plugin_buffer.h
 *   enum plugin_status    -> deleted; its last user, playlist_viewer.c, had
 *                            its own private protocol inlined instead
 *
 * The file itself survives for exactly one reason: lib/rbcodec/metadata/hes.c
 * still has a vestigial #include "plugin.h". That include needs nothing from
 * here -- the build succeeds with this file empty -- but lib/ is outside the
 * scope this fork's cleanup work is confined to, so the include stays and this
 * stub keeps it satisfied.
 *
 * To finish the job: delete the include at lib/rbcodec/metadata/hes.c:12, then
 * delete this file.
 */

#ifndef _PLUGIN_H_
#define _PLUGIN_H_

#endif /* _PLUGIN_H_ */
