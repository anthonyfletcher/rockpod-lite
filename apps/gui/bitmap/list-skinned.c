/***************************************************************************
 *             __________               __   ___.
 *   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
 *   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
 *   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
 *   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \
 *                     \/            \/     \/    \/            \/
 * $Id$
 *
 * Copyright (C) 2011 by Jonathan Gordon
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
#include "system.h"
#include "lcd.h"
#include "font.h"
#include "button.h"
#include "string.h"
#include "settings.h"
#include "kernel.h"
#include "file.h"

#include "action.h"
#include "screen_access.h"
#include "list.h"
#include "scrollbar.h"
#include "lang.h"
#include "sound.h"
#include "misc.h"
#include "viewport.h"
#include "statusbar-skinned.h"
#include "skin_engine/skin_engine.h"
#include "skin_engine/skin_display.h"
#include "skin_engine/skin_albumart_color.h"
#include "appevents.h"

static struct listitem_viewport_cfg *listcfg[NB_SCREENS] = {NULL};
static struct gui_synclist *current_list;

static int current_row;
static int current_column;

void skinlist_set_cfg(enum screen_type screen,
                      struct listitem_viewport_cfg *cfg)
{
    if (listcfg[screen] != cfg)
    {
        if (listcfg[screen])
            screens[screen].scroll_stop_viewport(&listcfg[screen]->selected_item_vp.vp);
        listcfg[screen] = cfg;
        current_list = NULL;
        current_column = -1;
        current_row = -1;
    }
}

static bool skinlist_is_configured(enum screen_type screen,
                                    struct gui_synclist *list)
{
    return (listcfg[screen] != NULL) &&
            (!list || (list && list->selected_size == 1));
}
static int current_drawing_line;
static int offset_to_item(int offset, bool wrap)
{
    int item = current_drawing_line + offset;
    if (!current_list || current_list->nb_items == 0)
        return -1;
    if (item < 0)
    {
        if (!wrap)
            return -1;
        else
            item = (item + current_list->nb_items) % current_list->nb_items;
    }
    else if (item >= current_list->nb_items && !wrap)
        return -1;
    else
        item = item % current_list->nb_items;
    return item;
}

int skinlist_get_item_number()
{
    return current_drawing_line;
}

int skinlist_get_item_row()
{
    return current_row;
}

int skinlist_get_item_column()
{
    return current_column;
}


const char* skinlist_get_item_text(int offset, bool wrap, char* buf, size_t buf_size)
{
    int item = offset_to_item(offset, wrap);
    if (item < 0 || !current_list)
        return NULL;
    const char* ret = current_list->callback_get_item_name(
                    item, current_list->data, buf, buf_size);
    return P2STR((unsigned char*)ret);
}

enum themable_icons skinlist_get_item_icon(int offset, bool wrap)
{
    int item = offset_to_item(offset, wrap);
    if (item < 0 || !current_list || current_list->callback_get_item_icon == NULL)
        return Icon_NOICON;
    return current_list->callback_get_item_icon(item, current_list->data);
}

#ifdef HAVE_ALBUMART
const struct bitmap* skinlist_get_item_albumart(int offset, bool wrap, struct dim *size)
{
    int item = offset_to_item(offset, wrap);
    if (item < 0 || !current_list || current_list->callback_get_item_albumart == NULL)
        return NULL;
    return current_list->callback_get_item_albumart(item, current_list->data, size);
}

/* Whether this row is an album-art row -- i.e. one the list draws taller than the
 * skin's own pitch. This is what %?La tests, so a theme can lay art rows out
 * differently (art + inset text) from ordinary rows sharing the same list config.
 * Deliberately structural, not "is a thumbnail loaded": the row keeps its art
 * layout while the background cache is still catching up, rather than each row
 * flipping layout as its cover appears. */
bool skinlist_item_is_art_row(enum screen_type screen, int offset, bool wrap)
{
    int item = offset_to_item(offset, wrap);
    if (item < 0 || !current_list ||
        current_list->callback_get_item_albumart == NULL ||
        current_list->callback_get_item_height == NULL ||
        listcfg[screen] == NULL || listcfg[screen]->tile)
    {
        return false;
    }
    return list_item_height(current_list, screen, item) > listcfg[screen]->height;
}
#endif

static bool is_selected = false;
bool skinlist_is_selected_item(void)
{
    return is_selected;
}

/* Variable row heights need a single column of rows to accumulate down, so they
 * are not supported in tile mode (a grid is uniform by construction). True when
 * this list's rows may differ in height. */
static bool skinlist_var_height(enum screen_type screen,
                                struct gui_synclist *list)
{
    return list && list->callback_get_item_height &&
           listcfg[screen] && !listcfg[screen]->tile;
}

bool skinlist_is_tile_mode(enum screen_type screen)
{
    return listcfg[screen] != NULL && listcfg[screen]->tile;
}

int skinlist_row_height(enum screen_type screen, struct gui_synclist *list)
{
    if (!skinlist_is_configured(screen, list) || listcfg[screen]->tile)
        return -1;
    return listcfg[screen]->height;
}

/* How many items, starting at `start`, fit in `avail` pixels of variable rows.
 * At least 1, so a row taller than the viewport still draws (clipped) rather
 * than leaving the list stuck at zero visible items. */
static int skinlist_items_fitting(enum screen_type screen,
                                  struct gui_synclist *list,
                                  int start, int avail)
{
    int n = 0, used = 0;

    while (start + n < list->nb_items)
    {
        int h = list_item_height(list, screen, start + n);
        if (used + h > avail)
            break;
        used += h;
        n++;
    }
    return n > 0 ? n : 1;
}

int skinlist_get_line_count(enum screen_type screen, struct gui_synclist *list)
{
    struct viewport *parent = (list->parent[screen]);
    if (!skinlist_is_configured(screen, list))
        return -1;
    if (listcfg[screen]->tile == true)
    {
        int rows = (parent->height / listcfg[screen]->height);
        int cols = (parent->width / listcfg[screen]->width);
        return rows*cols;
    }
    else if (skinlist_var_height(screen, list))
    {
        /* rows differ, so how many fit depends on where the window starts */
        return skinlist_items_fitting(screen, list, list->start_item[screen],
                                      parent->height);
    }
    else
        return  (parent->height / listcfg[screen]->height);
}

static int current_item;
static int current_nbitems;
static bool needs_scrollbar[NB_SCREENS];
bool skinlist_needs_scrollbar(enum screen_type screen)
{
    return needs_scrollbar[screen];
}

void skinlist_get_scrollbar(int* nb_item, int* first_shown, int* last_shown)
{
    if (!skinlist_is_configured(0, NULL))
    {
        *nb_item = 0;
        *first_shown = 0;
        *last_shown = 0;
    }
    else
    {
        *nb_item = current_item;
        *first_shown = 0;
        *last_shown = current_nbitems;
    }
}

bool skinlist_get_item(struct screen *display, struct gui_synclist *list, int x, int y, int *item)
{
    const int screen = display->screen_type;
    if (!skinlist_is_configured(screen, list))
        return false;

    if (skinlist_var_height(screen, list))
    {
        /* rows are not a fixed pitch: walk down them to find the one holding y */
        int start = list->start_item[screen];
        int used = 0, n = 0;

        while (start + n < list->nb_items)
        {
            int h = list_item_height(list, screen, start + n);
            if (y < used + h)
                break;
            used += h;
            n++;
        }
        *item = n;      /* an offset into the window, as below */
        return true;
    }

    int row = y / listcfg[screen]->height;
    int column = x / listcfg[screen]->width;
    struct viewport *parent = (list->parent[screen]);
    int cols = (parent->width / listcfg[screen]->width);
    *item = row * cols+ column;
    return true;
}

bool skinlist_draw(struct screen *display, struct gui_synclist *list)
{
    int cur_line, display_lines;
    const int screen = display->screen_type;
    struct viewport *parent = (list->parent[screen]);
    char* label = NULL;
    const int list_start_item = list->start_item[screen];
    struct gui_wps wps;
    if (!skinlist_is_configured(screen, list))
        return false;

    current_list = list;
#if defined(HAVE_ALBUMART) && defined(HAVE_LCD_COLOR)
    dynamic_colors_check_extraction(-1);
#endif
    wps.display = display;
    wps.data = listcfg[screen]->data;
    display_lines = skinlist_get_line_count(screen, list);
    label = (char *)SKINOFFSETTOPTR(get_skin_buffer(wps.data), listcfg[screen]->label);
    if (!label)
        return false;

    display->set_viewport(parent);
#if defined(HAVE_ALBUMART) && defined(HAVE_LCD_COLOR)
    unsigned int dc_saved_fg = parent->fg_pattern;
    unsigned int dc_saved_bg = parent->bg_pattern;
    parent->fg_pattern = dynamic_colors_resolve(dc_saved_fg);
    parent->bg_pattern = dynamic_colors_resolve(dc_saved_bg);
    display->set_foreground(parent->fg_pattern);
    display->set_background(parent->bg_pattern);
#endif
    display->clear_viewport();
    current_item = list->selected_item;
    current_nbitems = list->nb_items;
    needs_scrollbar[screen] = list->nb_items > display_lines;

    const bool var_height = skinlist_var_height(screen, list);
    int row_top = 0;    /* top of the current row, relative to parent */

    if (var_height && list->nb_items > 0)
    {
        /* Tall (album) rows rarely divide the viewport evenly, leaving dead
         * space at the bottom. Centre the rows in that leftover so the list
         * isn't top-heavy. Uses the (uniform) row height, so the offset stays
         * put while scrolling instead of jumping near the list's end. */
        int rh = list_item_height(list, screen, list_start_item);
        if (rh > 0)
            row_top = (parent->height % rh) / 2;
    }

    for (cur_line = 0; cur_line < display_lines; cur_line++)
    {
        struct skin_element* viewport;
        struct skin_viewport* skin_viewport = NULL;
        int row_height;
        if (list_start_item+cur_line+1 > list->nb_items)
            break;
        current_drawing_line = list_start_item+cur_line;
        is_selected = list_start_item+cur_line == list->selected_item;
        row_height = var_height
                   ? list_item_height(list, screen, list_start_item+cur_line)
                   : listcfg[screen]->height;

        for (viewport = SKINOFFSETTOPTR(get_skin_buffer(wps.data), listcfg[screen]->data->tree);
             viewport;
             viewport = SKINOFFSETTOPTR(get_skin_buffer(wps.data), viewport->next))
        {
            int original_x, original_y;
            skin_viewport = SKINOFFSETTOPTR(get_skin_buffer(wps.data), viewport->data);
            char *viewport_label = NULL;
            if (skin_viewport)
                viewport_label = SKINOFFSETTOPTR(get_skin_buffer(wps.data), skin_viewport->label);
            if (viewport->children == 0 || !viewport_label ||
                (skin_viewport->label && strcmp(label, viewport_label))
                )
                continue;
            if (is_selected)
            {
                memcpy(&listcfg[screen]->selected_item_vp, skin_viewport, sizeof(struct skin_viewport));
                skin_viewport = &listcfg[screen]->selected_item_vp;
            }
            original_x = skin_viewport->vp.x;
            original_y = skin_viewport->vp.y;
            if (listcfg[screen]->tile)
            {
                int cols = (parent->width / listcfg[screen]->width);
                current_column = (cur_line)%cols;
                current_row = (cur_line)/cols;

                skin_viewport->vp.x = parent->x + listcfg[screen]->width*current_column + original_x;
                skin_viewport->vp.y = parent->y + listcfg[screen]->height*current_row + original_y;
            }
            else
            {
                current_column = 1;
                current_row = cur_line;
                skin_viewport->vp.x = parent->x + original_x;
                skin_viewport->vp.y = parent->y + original_y +
                                   (var_height ? row_top
                                               : listcfg[screen]->height*cur_line);
                /* Variable rows only change where each row STARTS (row_top), not
                 * the size of the viewports within it: each %Vl keeps its declared
                 * height and the theme positions elements inside the taller row
                 * with their own y-offsets (e.g. a cover on the left and the name
                 * bar centred beside it). Stretching them here instead would
                 * inflate the selection bar into a full-height block. */
            }
            display->set_viewport(&skin_viewport->vp);
#if defined(HAVE_ALBUMART) && defined(HAVE_LCD_COLOR)
            /* Dynamic colors: resolve from stored originals */
            skin_viewport->vp.fg_pattern =
                dynamic_colors_resolve(skin_viewport->dc_orig_fg);
            skin_viewport->vp.bg_pattern =
                dynamic_colors_resolve(skin_viewport->dc_orig_bg);
            display->set_foreground(skin_viewport->vp.fg_pattern);
            display->set_background(skin_viewport->vp.bg_pattern);
#endif
            /* Set images to not to be displayed */
            struct skin_token_list *imglist = SKINOFFSETTOPTR(get_skin_buffer(wps.data), wps.data->images);
            while (imglist)
            {
                struct wps_token *token = SKINOFFSETTOPTR(get_skin_buffer(wps.data), imglist->token);
                struct gui_img *img = NULL;
                if (token)
                    img = SKINOFFSETTOPTR(get_skin_buffer(wps.data), token->value.data);
                if (img)
                    img->display = -1;
                imglist = SKINOFFSETTOPTR(get_skin_buffer(wps.data), imglist->next);
            }
            struct skin_element** children = SKINOFFSETTOPTR(get_skin_buffer(wps.data), viewport->children);
            if (children && *children)
                skin_render_viewport(SKINOFFSETTOPTR(get_skin_buffer(wps.data), (intptr_t)children[0]),
                                     &wps, skin_viewport, SKIN_REFRESH_ALL);
            wps_display_images(&wps, &skin_viewport->vp);
            /* force disableing scroll because it breaks later */
            if (!is_selected)
            {
                /* skin_viewport points into the skin buffer here (the selected
                 * row works on a copy), so every field we moved must go back */
                display->scroll_stop_viewport(&skin_viewport->vp);
                skin_viewport->vp.x = original_x;
                skin_viewport->vp.y = original_y;
            }
        }
        row_top += row_height;
    }
    current_column = -1;
    current_row = -1;
#if defined(HAVE_ALBUMART) && defined(HAVE_LCD_COLOR)
    parent->fg_pattern = dc_saved_fg;
    parent->bg_pattern = dc_saved_bg;
#endif
    display->set_viewport(parent);
    if (list_need_full_update() | skin_render_pending_update())
    {
        display->set_viewport(NULL);
        display->update();
        sb_skin_force_next_update();
    }
    else
        display->update_viewport();
    current_drawing_line = list->selected_item;
    return true;
}
