/***************************************************************************
 *             __________               __   ___.
 *   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
 *   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
 *   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
 *   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \
 *                     \/            \/     \/    \/            \/
 * $Id$
 *
 * Copyright (C) 2014 Jonathan Gordon
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

/* Formerly apps/plugins/main_menu_config.c -- ported to core so it's always
 * resident (no plugin-load flicker) and cooperates directly with root_menu.c's
 * tagnavi-driven entries instead of going through the plugin API boundary. */

#include <stdio.h>
#include "string-extra.h"
#include "config.h"
#include "lang.h"
#include "action.h"
#include "settings.h"
#include "menu.h"
#include "misc.h"
#include "talk.h"
#include "yesno.h"
#include "splash.h"
#include "screen_access.h"
#include "viewport.h"
#include "list.h"
#include "root_menu.h"
#include "exported_menus.h"
#ifdef HAVE_TOUCHSCREEN
#include "touchscreen.h"
#endif

static struct menu_table *menu_table;
static int menu_item_count;

#define MAX_ITEM_NAME 64
#define MAX_ITEMS 16
struct items
{
    unsigned char *name;
    char string[MAX_ITEM_NAME];
    bool enabled;
};

static struct items menu_items[MAX_ITEMS];


static const char * menu_get_name(int selected_item, void * data,
                                   char * buffer, size_t buffer_len)
{
    (void)data;
    unsigned char *p = menu_items[selected_item].name;
    int id = P2ID(p);

    snprintf(buffer, buffer_len, "%s: %s", (id != -1) ? str(id) : (char *)p,
             menu_items[selected_item].enabled ?
             str(LANG_ON) : str(LANG_OFF));

    return buffer;
}

static enum themable_icons menu_get_icon(int selected_item, void * data)
{
    (void)data;

    return menu_items[selected_item].enabled ? Icon_Config : Icon_NOICON;
}

static unsigned char *item_name(int n)
{
    const struct menu_item_ex *item = menu_table[n].item;
    return (item->flags & MENU_HAS_DESC) ?
      item->callback_and_desc->desc :
      (strcmp("wps", menu_table[n].string) ?
       (unsigned char *)menu_table[n].string :
       ID2P(LANG_RESUME_PLAYBACK));
}

static void load_from_cfg(void)
{
    char config_str[128];
    char *token, *save;
    int done = 0;
    int i = 0;
    bool found = false;

    config_str[0] = '\0';
    root_menu_write_to_cfg(NULL, config_str, sizeof(config_str));

    token = strtok_r(config_str, ", ", &save);

    while (token)
    {
        for (i = 0, found = false; i < menu_item_count; i++)
        {
            found = strcmp(token, menu_table[i].string) == 0;
            if (found) break;
        }
        if (found)
        {
            menu_items[done].name = item_name(i);
            strcpy(menu_items[done].string, token);
            menu_items[done].enabled = true;
            done++;
        }
        token = strtok_r(NULL, ", ", &save);
    }

    if (done < menu_item_count)
    {
        for (i = 0; i < menu_item_count; i++)
        {
            found = false;
            for (int j = 0; !found && j < done; j++)
            {
                found = strcmp(menu_table[i].string, menu_items[j].string) == 0;
            }

            if (!found)
            {
                menu_items[done].name = item_name(i);
                strcpy(menu_items[done].string, menu_table[i].string);
                menu_items[done].enabled = false;
                done++;
            }
        }
    }
}

static void save_to_cfg(void)
{
    char out[128];
    int i, j = 0;

    out[0] = '\0';
    for (i = 0; i < menu_item_count; i++)
    {
        if (menu_items[i].enabled)
        {
            j += snprintf(&out[j], sizeof(out) - j, "%s, ", menu_items[i].string);
        }
    }

    root_menu_load_from_cfg(&global_settings.root_menu_customized, out);
}

static void swap_items(int a, int b)
{
    unsigned char *name;
    char temp[MAX_ITEM_NAME];
    bool enabled;

    name = menu_items[a].name;
    strcpy(temp, menu_items[a].string);
    enabled = menu_items[a].enabled;
    menu_items[a].name = menu_items[b].name;
    strcpy(menu_items[a].string,
            menu_items[b].string);
    menu_items[a].enabled = menu_items[b].enabled;
    menu_items[b].name = name;
    strcpy(menu_items[b].string, temp);
    menu_items[b].enabled = enabled;
}

static int menu_speak_item(int selected_item, void *data)
{
    (void) data;
    int id = P2ID(menu_items[selected_item].name);

    if (id != -1)
    {
        talk_number(selected_item + 1, false);
        talk_id(id, true);
        talk_id(menu_items[selected_item].enabled ? LANG_ON : LANG_OFF, true);
    }

    return 0;
}

int main_menu_config(void)
{
    bool show_icons = global_settings.show_icons;
    global_settings.show_icons = true;
    struct gui_synclist list;
    bool done = false;
    bool changed = false;
    int action, cur_sel;
    int ret = 0;

    menu_table = root_menu_get_options(&menu_item_count);
    load_from_cfg();

    /* This screen builds its own gui_synclist directly instead of going
     * through do_menu() (it needs custom OK-to-toggle and a move-up/down
     * context menu, which don't fit do_menu()'s model), so it has to enable
     * theme rendering itself -- otherwise it draws with the bare, unthemed
     * list style instead of the theme's menu skin. */
    FOR_NB_SCREENS(i)
        viewportmanager_theme_enable(i, true, NULL);

    gui_synclist_init(&list, menu_get_name, NULL, false, 1, NULL);
    if (global_settings.talk_menu)
        gui_synclist_set_voice_callback(&list, menu_speak_item);
    gui_synclist_set_icon_callback(&list, menu_get_icon);
    gui_synclist_set_nb_items(&list, menu_item_count);
    gui_synclist_set_title(&list, str(LANG_MAIN_MENU), Icon_Rockbox);
    gui_synclist_draw(&list);
    gui_synclist_speak_item(&list);

#ifdef HAVE_TOUCHSCREEN
    touchscreen_set_mode(global_settings.touch_mode);
#endif

    while (!done)
    {
        cur_sel = gui_synclist_get_sel_pos(&list);
        action = get_action(CONTEXT_LIST, HZ/10);
        if (gui_synclist_do_button(&list, &action))
            continue;

        switch (action)
        {
            case ACTION_STD_OK:
                menu_items[cur_sel].enabled = !menu_items[cur_sel].enabled;
                gui_synclist_speak_item(&list);
                changed = true;
                break;
            case ACTION_STD_CONTEXT:
            {
                MENUITEM_STRINGLIST(menu, ID2P(LANG_MAIN_MENU), NULL,
                                    ID2P(LANG_MOVE_ITEM_UP),
                                    ID2P(LANG_MOVE_ITEM_DOWN),
                                    ID2P(LANG_LOAD_DEFAULT_CONFIGURATION));
                switch (do_menu(&menu, NULL, NULL, false))
                {
                    case 0:
                        if (cur_sel == 0)
                        {
                            splash(HZ, ID2P(LANG_FAILED));
                            break;
                        }
                        swap_items(cur_sel, cur_sel - 1);
                        gui_synclist_select_item(&list, cur_sel - 1); /* speaks */
                        changed = true;
                        break;
                    case 1:
                        if (cur_sel + 1 == menu_item_count)
                        {
                            splash(HZ, ID2P(LANG_FAILED));
                            break;
                        }
                        swap_items(cur_sel, cur_sel + 1);
                        gui_synclist_select_item(&list, cur_sel + 1); /* speaks */
                        changed = true;
                        break;
                    case 2:
                        if (yesno_pop_confirm(ID2P(LANG_LOAD_DEFAULT_CONFIGURATION)))
                        {
                            root_menu_set_default(&global_settings.root_menu_customized, NULL);
                            load_from_cfg();
                        }
                        /* fall-through */
                    default:
                        gui_synclist_speak_item(&list);
                }
                gui_synclist_set_title(&list, str(LANG_MAIN_MENU), Icon_Rockbox);
                break;
            }
            case ACTION_STD_CANCEL:
            case ACTION_STD_MENU:
                done = true;
                break;
            default:
                if (default_event_handler(action) == SYS_USB_CONNECTED)
                {
                    ret = SYS_USB_CONNECTED;
                    done = true;
                }
                continue;
        }

        if (!done)
            gui_synclist_draw(&list);
    }

    if (changed)
    {
        save_to_cfg();
        global_settings.root_menu_customized = true;
        settings_save();
    }
    global_settings.show_icons = show_icons;

    FOR_NB_SCREENS(i)
        viewportmanager_theme_undo(i, false);

    return ret;
}
