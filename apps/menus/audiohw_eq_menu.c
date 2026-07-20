/***************************************************************************
 *             __________               __   ___.
 *   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
 *   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
 *   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
 *   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \
 *                     \/            \/     \/    \/            \/
 * $Id$
 *
 * Copyright (C) 2010 Michael Sevakis
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
#include <stdio.h>
#include "config.h"
#include "sound.h"
#include "settings.h"
#include "lang.h"
#include "menu.h"
#include "talk.h"

#define HW_EQ_IDX(band, setting)  ((void *)(((setting) << 8) | (band)))
#define HW_EQ_IDX_BAND(data)      ((uint8_t)(uintptr_t)(data))
#define HW_EQ_IDX_SETTING(data)   ((uint8_t)((uintptr_t)(data) >> 8))

static unsigned short hw_eq_setting_lang_ids[AUDIOHW_EQ_SETTING_NUM] =
{
    LANG_HW_EQ_GAIN,
};

static char * hw_eq_get_name(int selected_item, void * data,
                             char *buffer, size_t buffer_len)
{
    snprintf(buffer, buffer_len,
             str(hw_eq_setting_lang_ids[HW_EQ_IDX_SETTING(data)]),
             HW_EQ_IDX_BAND(data) + 1);
    return buffer;
    (void)selected_item;
}

static int hw_eq_speak_item(int selected_item, void * data)
{
    talk_id(hw_eq_setting_lang_ids[HW_EQ_IDX_SETTING(data)], false);
    talk_number(HW_EQ_IDX_BAND(data) + 1, true);
    return 0;
    (void)selected_item;
}

static int hw_eq_do_band_setting(void *param)
{
    int band = HW_EQ_IDX_BAND(param);
    int setting = HW_EQ_IDX_SETTING(param);
    char desc[MAX_PATH];
    struct menu_callback_with_desc cbwdesc =
    {
        .menu_callback = NULL,
        .desc = hw_eq_get_name(0, param, desc, sizeof(desc)),
        .icon_id = Icon_NOICON
    };
    struct menu_item_ex item =
    {
        .flags = MT_SETTING_W_TEXT | MENU_HAS_DESC,
        { .variable = (void*)(&global_settings.hw_eq_bands[band].gain + setting) },
        { .callback_and_desc = &cbwdesc }
    };
    do_setting_from_menu(&item, NULL);
    return 0;
}

MENUITEM_FUNCTION_DYNTEXT_W_PARAM(hw_eq_band1_gain, 0,
                                  hw_eq_do_band_setting,
                                  HW_EQ_IDX(AUDIOHW_EQ_BAND1, AUDIOHW_EQ_GAIN),
                                  hw_eq_get_name, hw_eq_speak_item,
                                  HW_EQ_IDX(AUDIOHW_EQ_BAND1, AUDIOHW_EQ_GAIN),
                                  NULL, Icon_Menu_setting);
#ifdef AUDIOHW_HAVE_EQ_BAND1_FREQUENCY
MENUITEM_FUNCTION_DYNTEXT_W_PARAM(hw_eq_band1_frequency, 0,
                                  hw_eq_do_band_setting,
                                  HW_EQ_IDX(AUDIOHW_EQ_BAND1, AUDIOHW_EQ_FREQUENCY),
                                  hw_eq_get_name, hw_eq_speak_item,
                                  HW_EQ_IDX(AUDIOHW_EQ_BAND1, AUDIOHW_EQ_FREQUENCY),
                                  NULL, Icon_NOICON);
#endif

/* Submenu for multiple "tone controls". Gain + all advanced settings. */
MAKE_MENU(hardware_eq_tone_controls_advanced, ID2P(LANG_HW_EQ_TONE_CONTROLS_ADVANCED),
          NULL, Icon_NOICON
          ,&hw_eq_band1_gain
#ifdef AUDIOHW_HAVE_EQ_BAND1_FREQUENCY
          ,&hw_eq_band1_frequency
#endif
    );
/* Shows only the gains + advanced settings submenu */
MAKE_MENU(audiohw_eq_tone_controls, ID2P(LANG_HW_EQ_TONE_CONTROLS),
          NULL, Icon_NOICON
          ,&hw_eq_band1_gain
          ,&hardware_eq_tone_controls_advanced
    );

