/***************************************************************************
 * RockPod-Lite
 *
 * Original code from RockBox
 * was: apps/screens.c (runtime info screen)
 * Copyright (C) 2002 Björn Stenberg
 * GNU General Public License (version 2+)
 *
 * Running-time and top-time statistics screen, with the option to reset
 * either counter.
 ****************************************************************************/

#include <stdbool.h>
#include <stdio.h>
#include "config.h"
#include "lang.h"
#include "kernel.h"
#include "input/action.h"
#include "settings/settings.h"
#include "speech/talk.h"
#include "widgets/list.h"
#include "widgets/yesno.h"
#include "system/format_time.h"
#include "runtime_info.h"


static const char* runtime_get_data(int selected_item, void* data,
                                    char* buffer, size_t buffer_len)
{
    (void)data;
    long t;
    switch (selected_item)
    {
        case 0:return str(LANG_RUNNING_TIME);
        case 1: {
            update_runtime();
             t = global_status.runtime;
            break;
        }
        case 2: return str(LANG_TOP_TIME);
        case 3: t = global_status.topruntime;   break;
        default:
            return "";
    }

    format_time_auto(buffer, buffer_len, t, UNIT_SEC, false);

    return buffer;
}

static int runtime_speak_data(int selected_item, void* data)
{
    (void)data;
    talk_ids(false,(selected_item < 2) ? LANG_RUNNING_TIME : LANG_TOP_TIME,
             TALK_ID((selected_item < 2) ? global_status.runtime
                                         : global_status.topruntime, UNIT_TIME));
    return 0;
}

static int runtime_info_cb(int action, struct gui_synclist *lists)
{
    static const char *lines[]={ ID2P(LANG_RUNNING_TIME), ID2P(LANG_CLEAR_TIME),
                                 ID2P(LANG_TOP_TIME),     ID2P(LANG_CLEAR_TIME)};

    if (action == ACTION_NONE)
        return ACTION_REDRAW;

    if(action == ACTION_STD_OK) {
        int selected = (gui_synclist_get_sel_pos(lists));

        const struct text_message message={lines + selected, 2};

        if(gui_syncyesno_run(&message, NULL, NULL)==YESNO_YES)
        {
            if (selected == 0)
                global_status.runtime = 0;
            else /*selected == 2*/
                global_status.topruntime = 0;
            gui_synclist_speak_item(lists);
        }
        action = ACTION_REDRAW;
    }
    return action;
}

int view_runtime(void)
{
    struct simplelist_info info;

    simplelist_info_init(&info, NULL, 2, NULL);
    info.get_name = runtime_get_data;
    info.action_callback = runtime_info_cb;
    info.timeout = HZ;
    info.selection_size = 2;
    if(global_settings.talk_menu)
        info.get_talk = runtime_speak_data;
    info.scroll_all = true;
    return simplelist_show_list(&info);
}

