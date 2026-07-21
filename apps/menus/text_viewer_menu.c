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

/* Settings menu for the core text viewer (apps/text_viewer). The same menu
 * is reached from Settings and, in the viewer, by holding Menu. */

#include <stdbool.h>
#include "config.h"
#include "lang.h"
#include "settings.h"
#include "menu.h"
#include "tree.h"            /* browse_context, rockbox_browse */
#include "filetypes.h"       /* SHOW_FONT */
#include "rbpaths.h"         /* FONT_DIR */
#include "recorder/icons.h"

MENUITEM_SETTING(text_viewer_colour_mode,
                 &global_settings.text_viewer_colour_mode, NULL);
MENUITEM_SETTING(text_viewer_margin,
                 &global_settings.text_viewer_margin, NULL);
MENUITEM_SETTING(text_viewer_line_spacing,
                 &global_settings.text_viewer_line_spacing, NULL);
MENUITEM_SETTING(text_viewer_page_number,
                 &global_settings.text_viewer_page_number, NULL);

/* Pick a .fnt from the fonts folder, storing just its name in the setting;
 * unlike the theme's own font item this does not touch the global UI font. */
static int text_viewer_font_pick(void)
{
    char path[MAX_PATH], name[MAX_FILENAME + 10];
    struct browse_context browse = {
        .dirfilter = SHOW_FONT,
        .flags = BROWSE_SELECTONLY | BROWSE_NO_CONTEXT_MENU,
        .title = ID2P(LANG_CUSTOM_FONT),
        .icon = Icon_Font,
        .root = FONT_DIR,
        .selected = NULL,
        .buf = path,
        .bufsize = sizeof path,
    };

    if (global_settings.text_viewer_font_file[0])
    {
        snprintf(name, sizeof name, "%s.fnt",
                 global_settings.text_viewer_font_file);
        browse.selected = name;
    }

    rockbox_browse(&browse);

    if (browse.flags & BROWSE_SELECTED)
        set_file(path, (char *)global_settings.text_viewer_font_file);
    return 0;
}
MENUITEM_FUNCTION(text_viewer_font_item, 0, ID2P(LANG_CUSTOM_FONT),
                  text_viewer_font_pick, NULL, Icon_Font);

/* Drop back to the theme's UI font. */
static int text_viewer_font_reset(void)
{
    global_settings.text_viewer_font_file[0] = '\0';
    settings_save();
    return 0;
}
MENUITEM_FUNCTION(text_viewer_font_reset_item, 0,
                  ID2P(LANG_TEXT_VIEWER_FONT_DEFAULT),
                  text_viewer_font_reset, NULL, Icon_Font);

MAKE_MENU(text_viewer_menu, ID2P(LANG_TEXT_VIEWER), NULL, Icon_Menu_setting,
          &text_viewer_colour_mode,
          &text_viewer_margin,
          &text_viewer_line_spacing,
          &text_viewer_page_number,
          &text_viewer_font_item,
          &text_viewer_font_reset_item);
