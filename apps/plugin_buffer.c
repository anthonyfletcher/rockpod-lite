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
 * The plugin system has been removed from this fork. All that remains of it in
 * the core is the plugin RAM region (pluginbuf, from the linker script), which
 * a handful of core screens still borrow as a scratch buffer via
 * plugin_get_buffer() -- bookmark.c, cuesheet.c, fileop.c and the album-art
 * carousel among them. No plugin is ever loaded now, so the whole region is
 * always free.
 *
 * The rb-> API struct, the .rock loader, plugin.h and the rest of the plugin
 * machinery are all gone.
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
#include "config.h"
#include "plugin_buffer.h"

/* The plugin RAM region, laid out by the linker script. */
extern unsigned char pluginbuf[];

/* Returns the plugin buffer for use as core scratch space. */
void* plugin_get_buffer(size_t *buffer_size)
{
    *buffer_size = PLUGIN_BUFFER_SIZE;
    return pluginbuf;
}
