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

/* Formerly apps/plugins/main_menu_config.c -- ported to core */

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
#include "audio.h" /* audio_status() -- see item_is_locked() */

static struct menu_table *menu_table;
static int menu_item_count;

#define MAX_ITEM_NAME 64
/* root_menu.c's menu_table[] now holds the ~9 fixed entries plus one slot
 * per tagnavi.config root-menu row (TAGNAVI_MAIN_MENU_SLOTS, see
 * root_menu.h) -- this used to be a safe fixed 16, but the tagnavi slots
 * alone can already exceed that, silently overflowing menu_items[] below
 * (a real crash, not hypothetical -- this is exactly what happened before
 * this was fixed). Give it real headroom over the fixed-entry count rather
 * than guessing a bigger fixed number again. */
#define MAX_ITEMS (16 + TAGNAVI_MAIN_MENU_SLOTS)

/* Same class of bug as MAX_ITEMS above, same fix: a fixed 128-byte buffer
 * for the "key, key, key, ..." string was sized back when this only ever
 * held a handful of fixed entries, long before tagnavi slots existed. With
 * MAX_ITEMS now up to 36, a fully-populated list (keys up to 11 chars,
 * e.g. "pictureflow"/"system_menu"/"tagnavi19", plus ", ") can need over
 * 400 bytes -- root_menu_write_to_cfg()'s own buf_len bookkeeping doesn't
 * defend against running past the end of an undersized buffer (int
 * buf_len going negative on overflow, then reinterpreted as a huge size_t
 * by the next snprintf() call), so this was a real, silent stack buffer
 * overflow whenever enough items were enabled at once -- not merely a
 * truncated string. 13 bytes/item is the worst case above with a small
 * safety margin. */
#define CFG_STR_SIZE (MAX_ITEMS * 13)

struct items
{
    unsigned char *name;
    /* Backing buffer for dynamic-name items (e.g. root_menu.c's tagnavi
     * entries, MENU_DYNAMIC_DESC) -- unlike MENU_HAS_DESC's static, always
     * P2STR-resolvable 'desc' pointer, a dynamic name only exists once its
     * list_get_name() callback actually runs, so it has to be resolved into
     * a real buffer up front (see item_name()) rather than deferred. */
    char dyn_name[MAX_ITEM_NAME];
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

/* 'dest' is the menu_items[] slot this name is being resolved for -- needed
 * because a MENU_DYNAMIC_DESC item's name doesn't exist as a static pointer
 * anywhere; some callbacks (see list_get_name's own declared return type)
 * render into the buffer they're handed and return that same pointer back,
 * but others -- e.g. root_menu.c's get_wps_item_name(), which just returns
 * ID2P(LANG_NOW_PLAYING)/ID2P(LANG_RESUME_PLAYBACK) directly -- never touch
 * the buffer at all and only communicate via the return value. Previously
 * this discarded that return value and unconditionally used dest->dyn_name
 * instead (matching only the first kind of callback), which for the WPS
 * item meant reading back whatever dest->dyn_name happened to already
 * contain -- all zero bytes the first time any given slot is used, i.e. an
 * empty string. apps/menu.c's own get_menu_item_name() is the correct,
 * established pattern to follow here (capture and use the return value). */
static unsigned char *item_name(int n, struct items *dest)
{
    const struct menu_item_ex *item = menu_table[n].item;

    if (item->flags & MENU_DYNAMIC_DESC)
    {
        const struct menu_get_name_and_icon *dyn = item->menu_get_name_and_icon;
        return (unsigned char *)dyn->list_get_name(0, dyn->list_get_name_data,
                           dest->dyn_name, sizeof(dest->dyn_name));
    }

    return (item->flags & MENU_HAS_DESC) ?
      item->callback_and_desc->desc :
      (strcmp("wps", menu_table[n].string) ?
       (unsigned char *)menu_table[n].string :
       ID2P(LANG_RESUME_PLAYBACK));
}

static void load_from_cfg(void)
{
    char config_str[CFG_STR_SIZE];
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
        if (found && done < MAX_ITEMS)
        {
            menu_items[done].name = item_name(i, &menu_items[done]);
            strcpy(menu_items[done].string, token);
            menu_items[done].enabled = true;
            done++;
        }
        token = strtok_r(NULL, ", ", &save);
    }

    if (done < menu_item_count)
    {
        for (i = 0; i < menu_item_count && done < MAX_ITEMS; i++)
        {
            found = false;
            for (int j = 0; !found && j < done; j++)
            {
                found = strcmp(menu_table[i].string, menu_items[j].string) == 0;
            }

            if (!found)
            {
                menu_items[done].name = item_name(i, &menu_items[done]);
                strcpy(menu_items[done].string, menu_table[i].string);
                menu_items[done].enabled = false;
                done++;
            }
        }
    }
}

static void save_to_cfg(void)
{
    char out[CFG_STR_SIZE];
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

/* Settings and Resume Playback/Now Playing are breaking changes to turn
 * off by accident: disabling Settings leaves no easy way back into the
 * menu that could re-enable it, and disabling the WPS item while
 * something's actually playing leaves no menu-based way to reach playback
 * controls at all. Requested explicitly to be un-toggleable while that
 * risk actually applies -- Settings always, the WPS item only while
 * audio_status() is true (nothing stops disabling it when nothing's
 * playing, which is a legitimate, safe choice). This only blocks the
 * toggle here, in the UI; root_menu.c's root_menu_build_display_list()
 * separately guarantees the WPS item is still shown live in the actual
 * main menu whenever something's playing, even if it was left disabled
 * from an earlier moment when nothing was. */
static bool item_is_locked(int n)
{
    if (strcmp(menu_items[n].string, "settings") == 0)
        return true;
    if (strcmp(menu_items[n].string, "wps") == 0)
        return audio_status() != 0;
    return false;
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
    gui_synclist_set_title(&list, str(LANG_MAIN_MENU_SETTINGS), Icon_Rockbox);
    /* Draw twice to settle the top row: entering from a list of a different
     * row height (e.g. a tall album-art list) leaves the shared %?La album
     * conditional transiently true on the first row, flashing its album-layout
     * viewports for one frame -- the same issue tree.c's update_dir() fixes.
     * The first pass is flush-inhibited so that transient frame never reaches
     * the screen. */
    gui_synclist_inhibit_flush(true);
    gui_synclist_draw(&list);
    gui_synclist_inhibit_flush(false);
    gui_synclist_draw(&list);
    gui_synclist_speak_item(&list);


    while (!done)
    {
        cur_sel = gui_synclist_get_sel_pos(&list);
        /* list_do_action (not a raw get_action) so the themed status bar's
         * per-render viewport re-init is flush-inhibited during input polling.
         * Without it those clears reach the display before the list repaints,
         * showing as constant flicker under a heavy SBS (the failsafe SBS,
         * which barely draws, doesn't trigger it). HZ so the status bar still
         * redraws while idle, matching option_select.c's settings lists. */
        if (list_do_action(CONTEXT_LIST, HZ, &list, &action))
            continue;

        switch (action)
        {
            case ACTION_STD_OK:
                if (item_is_locked(cur_sel))
                {
                    splash(HZ, ID2P(LANG_FAILED));
                    break;
                }
                menu_items[cur_sel].enabled = !menu_items[cur_sel].enabled;
                gui_synclist_speak_item(&list);
                changed = true;
                break;
            case ACTION_STD_CONTEXT:
            {
                MENUITEM_STRINGLIST(menu, ID2P(LANG_MAIN_MENU_SETTINGS), NULL,
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
                gui_synclist_set_title(&list, str(LANG_MAIN_MENU_SETTINGS), Icon_Rockbox);
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
