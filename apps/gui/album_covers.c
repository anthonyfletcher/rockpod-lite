/***************************************************************************
*             __________               __   ___.
*   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
*   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
*   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
*   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \
*                     \/            \/     \/    \/            \/
* $Id$
*
* Copyright (C) 2007 Jonas Hurrelmann (j@outpo.st)
* Copyright (C) 2007 Nicolas Pennequin
* Copyright (C) 2007 Ariya Hidayat (ariya@kde.org) (original Qt Version)
*
* Original code: http://code.google.com/p/pictureflow/
*
* Formerly apps/plugins/pictureflow/pictureflow.c -- ported to core  - MIT License
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
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "string-extra.h"
#include "config.h"
#include "system.h"          /* MIN/MAX/ALIGN_BUFFER/ALIGN_DOWN */
#include "action.h"           /* get_action/get_custom_action/button_mapping */
#include "screen_access.h"    /* FOR_NB_SCREENS, screens[] */
#include "kernel.h"           /* threads, mutex, queue, current_tick */
#include "core_alloc.h"       /* buflib types for buf_ctx (see init()) */
#include "plugin.h"           /* plugin_get_buffer() -- see init() */
#include "tagcache.h"
#include "playlist.h"
#include "playlist_catalog.h"
#include "settings.h"
#include "lang.h"
#include "splash.h"
#include "bitmaps/no_album_cover.h" /* bm_no_album_cover -- see create_empty_slide() */
#include "viewport.h"
#include "misc.h"             /* default_event_handler, warn_on_pl_erase, fix_path_part */
#include "onplay.h"           /* onplay_show_playlist_cat_menu/menu */
#include "albumart.h"         /* find_albumart / search_albumart_files */
#include "albumart_cache.h"   /* shared database-driven thumbnail cache */
#include "metadata.h"         /* struct mp3entry, get_metadata */
#include "dir.h"
#include "file.h"
#include "yesno.h"            /* gui_syncyesno_run */
#include "root_menu.h"
#include "tree.h"
#include "tagtree.h"
#include "screens.h"          /* browse_id3 */
#include "audio.h"            /* audio_status, audio_current_track */
#include "lcd.h"
#include "font.h"
#include "icons.h"
#include "menu.h"             /* MENUITEM_STRINGLIST, do_menu */
#include "bmp.h"              /* read_bmp_file */
#ifdef HAVE_JPEG
#include "jpeg_load.h"        /* read_jpeg_file */
#endif
#include "power.h"
#include "powermgmt.h"        /* reset_poweroff_timer */
#include "backlight.h"        /* backlight_set_timeout(_plugged) */
#ifdef HAVE_ADJUSTABLE_CPU_FREQ
#include "cpu.h"
#endif
#include "skin_engine/skin_engine.h"  /* skin_render_inhibit_flush */
#include "skin_engine/skin_albumart_color.h" /* dynamic_colors_resolve */
#include "statusbar-skinned.h" /* sb_set_persistent_title */
#include "album_covers.h"
#include "carousel.h"     /* shared carousel engine interface (pf_idx, model, ...) */

/******************************* Globals ***********************************/
static fb_data *lcd_fb;

#define PF_PREV ACTION_STD_PREV
#define PF_PREV_REPEAT ACTION_STD_PREVREPEAT
#define PF_NEXT ACTION_STD_NEXT
#define PF_NEXT_REPEAT ACTION_STD_NEXTREPEAT
#define PF_SELECT ACTION_STD_OK
#define PF_CONTEXT ACTION_STD_CONTEXT
#define PF_BACK ACTION_STD_CANCEL
#define PF_MENU ACTION_STD_MENU
#define PF_WPS ACTION_TREE_WPS
#define PF_JMP ACTION_LISTTREE_PGDOWN
#define PF_JMP_PREV ACTION_LISTTREE_PGUP

#define PF_QUIT (LAST_ACTION_PLACEHOLDER + 1)
#define PF_TRACKLIST (LAST_ACTION_PLACEHOLDER + 2)
#define PF_SORTING_NEXT (LAST_ACTION_PLACEHOLDER + 3)
#define PF_SORTING_PREV (LAST_ACTION_PLACEHOLDER + 4)

/* Both this fork's targets are CONFIG_KEYPAD == IPOD_4G_PAD (confirmed via
 * firmware/export/config/ipod6g.h and ipodvideo.h), so only that keypad's
 * button mapping is kept -- every other target's mapping the original
 * cross-target plugin supported has been dropped rather than left as dead
 * conditional code. */
const struct button_mapping pf_context_album_scroll[] =
{
    {PF_JMP_PREV,     BUTTON_LEFT,                BUTTON_NONE},
    {PF_JMP_PREV,     BUTTON_LEFT|BUTTON_REPEAT,  BUTTON_NONE},
    {PF_JMP,          BUTTON_RIGHT,               BUTTON_NONE},
    {PF_JMP,          BUTTON_RIGHT|BUTTON_REPEAT, BUTTON_NONE},
    {ACTION_NONE,     BUTTON_LEFT|BUTTON_REL,     BUTTON_LEFT},
    {ACTION_NONE,     BUTTON_RIGHT|BUTTON_REL,    BUTTON_RIGHT},
    {ACTION_NONE,     BUTTON_LEFT|BUTTON_REPEAT,  BUTTON_LEFT},
    {ACTION_NONE,     BUTTON_RIGHT|BUTTON_REPEAT, BUTTON_RIGHT},
    LAST_ITEM_IN_LIST__NEXTLIST(CONTEXT_PLUGIN|1)
};

const struct button_mapping pf_context_buttons[] =
{
    {PF_MENU,         BUTTON_MENU|BUTTON_REPEAT,  BUTTON_MENU},
    {PF_QUIT,         BUTTON_MENU|BUTTON_REL,     BUTTON_MENU},
    {PF_SORTING_NEXT, BUTTON_SELECT|BUTTON_MENU,  BUTTON_NONE},
    {PF_SORTING_PREV, BUTTON_SELECT|BUTTON_PLAY,  BUTTON_NONE},
    LAST_ITEM_IN_LIST__NEXTLIST(CONTEXT_TREE)
};
const struct button_mapping *pf_contexts[] =
{
    pf_context_album_scroll,
    pf_context_buttons
};

#define LCD_BUF lcd_fb
#define G_PIX LCD_RGBPACK
#define N_PIX LCD_RGBPACK
#define G_BRIGHT(y) LCD_RGBPACK(y,y,y)
#define N_BRIGHT(y) LCD_RGBPACK(y,y,y)
#define BUFFER_WIDTH LCD_WIDTH
#define BUFFER_HEIGHT LCD_HEIGHT
/* pix_t, the index structs, the scroll-line enum, the SUCCESS/ERROR_* codes and
 * struct carousel_model now live in carousel.h (shared with artist_portraits.c). */

static pix_t pf_bg_color;     /* theme background, replaces G_BRIGHT(0) */
pix_t pf_fg_color;     /* theme foreground, replaces G_BRIGHT(255) */
static pix_t pf_lss_color;    /* selector start color for gradient */
static pix_t pf_lse_color;    /* selector end color for gradient */
static pix_t pf_lst_color;    /* selector text color */

/* Pre-extracted RGB565 channel masks for fade_color() performance */
static unsigned int pf_bg_rb;  /* pf_bg_color & 0xf81f */
static unsigned int pf_bg_g;   /* pf_bg_color & 0x7e0 */

static void pf_update_dynamic_colors(void)
{
    /* Swapped relative to the rest of the UI's bg/fg convention, requested
     * explicitly: this screen's background should match the status bar's
     * (fg_color), with text/foreground drawn in what's normally the
     * background colour. */
    pf_bg_color = (pix_t)dynamic_colors_resolve(global_settings.fg_color);
    pf_fg_color = (pix_t)dynamic_colors_resolve(global_settings.bg_color);
    pf_lss_color = (pix_t)dynamic_colors_resolve(global_settings.lss_color);
    pf_lse_color = (pix_t)dynamic_colors_resolve(global_settings.lse_color);
    pf_lst_color = (pix_t)dynamic_colors_resolve(global_settings.lst_color);
    pf_bg_rb = pf_bg_color & 0xf81f;
    pf_bg_g  = pf_bg_color & 0x7e0;
}

/* for fixed-point arithmetic, we need minimum 32-bit long
   long long (64-bit) might be useful for multiplication and division */
#define PFreal long
#define PFREAL_SHIFT 10
#define PFREAL_FACTOR (1 << PFREAL_SHIFT)
#define PFREAL_ONE (1 << PFREAL_SHIFT)
#define PFREAL_HALF (PFREAL_ONE >> 1)

#define IANGLE_MAX 1024
#define IANGLE_MASK 1023

#define DISPLAY_HEIGHT (LCD_HEIGHT * 2 / 3)
#define DISPLAY_WIDTH MAX((LCD_HEIGHT * LCD_PIXEL_ASPECT_HEIGHT / \
    LCD_PIXEL_ASPECT_WIDTH / 2), (LCD_WIDTH * 2 / 5))
#define CAM_DIST MAX(MIN(LCD_HEIGHT,LCD_WIDTH),120)
#define CAM_DIST_R (CAM_DIST << PFREAL_SHIFT)
#define DISPLAY_LEFT_R (PFREAL_HALF - LCD_WIDTH * PFREAL_HALF)
#define MAXSLIDE_LEFT_R (PFREAL_HALF - DISPLAY_WIDTH * PFREAL_HALF)

#define SLIDE_CACHE_SIZE 100

#define MAX_SLIDES_COUNT 10
/* Number of side slides rendered per side (3 visible + 1 animation buffer).
 * Was a user-configurable field in the original plugin's config struct, but
 * was never actually exposed in any settings menu, so it's now a fixed
 * constant. */
#define ALBUM_COVERS_NUM_SLIDES 4

/* Not theme-controlled: layout is fixed/proportional (see init()) and
 * colours come from the theme's normal fg/bg + the dynamic (album-art
 * derived) colour scheme, same as everywhere else. An earlier version
 * tried to let the theme drive layout, colours and text rendering via
 * declaration-only SBS viewports (Album_Covers_Viewport/Panel/Name/Artist)
 * that this screen read once and then drew into itself -- reverted, since
 * every attempt to route any part of this through the skin engine at
 * runtime (colours inherited from ambient, non-deterministic SBS viewport
 * state; skin_update()-rendered text racing this screen's own lcd_update();
 * %Vd()'d content left "shown" by whatever screen preceded this one
 * bleeding through) turned out to fight this screen's own per-frame
 * raw-framebuffer redraw model. None of that is fixable while this screen
 * keeps redrawing its own pixels every frame outside the SBS's normal
 * render cycle, so it isn't themeable beyond the theme's plain fg/bg. */
#define THREAD_STACK_SIZE DEFAULT_STACK_SIZE + 0x200
#define CACHE_PREFIX ROCKBOX_DIR "/album_covers"
#define ALBUM_INDEX CACHE_PREFIX "/album_covers.idx"

#define EV_EXIT 9999
#define EV_WAKEUP 1337

#define EMPTY_SLIDE CACHE_PREFIX "/emptyslide.pfraw"
/* "?" slide source bitmap: compiled in as bm_no_album_cover (apps/bitmaps/
 * native/SOURCES) rather than read from a file -- see create_empty_slide().
 * Used to live under the old pictureflow plugin's demos-category folder
 * (PLUGIN_DEMOS_DIR), a path that no longer exists now that plugins are a
 * flat list (see apps/plugins/CATEGORIES and viewers.config), and this was
 * never really a plugin asset to begin with. */

/* Ordered Bayer dithering for 24-bit to RGB565 conversion */
static const unsigned char pf_dither_table[16] =
    {   0,192, 48,240, 12,204, 60,252,  3,195, 51,243, 15,207, 63,255 };
#define PF_DITHERY(y) (pf_dither_table[(y) & 15] & 0xAA)
#define PF_DITHERX(x) (pf_dither_table[(x) & 15])
#define PF_DITHERXDY(x,dy) (PF_DITHERX(x) ^ dy)

/* some magic numbers for cache_version. */
#define CACHE_REBUILD   0

/* current version for cover cache */
#define CACHE_VERSION 5
#define CONFIG_VERSION 1
#define CONFIG_FILE ROCKBOX_DIR "/album_covers.cfg"
#define INDEX_HDR "PFID"

/** structs we use */
struct pf_config_t
{
     /* internal cache/resume state -- NOT user settings, see
      * apps/menus/album_covers_menu.c and global_settings for the real
      * user-facing settings this struct used to also hold. */
     int cache_version;
     bool update_albumart;
     int last_album;
};

struct albumart_t {
    struct bitmap input_bmp;
    char pfraw_file[MAX_PATH];
    char file[MAX_PATH];
    int idx;
    int slides;
    int inspected;
    void * buf;
    size_t buf_sz;
};

struct slide_data {
    int slide_index;
    int angle;
    PFreal cx;
    PFreal cy;
    PFreal distance;
    int cached_slot; /* last cache slot this slide resolved to (see surface()) */
};

struct slide_cache {
    int index;      /* index of the cached slide */
    int hid;        /* handle ID of the cached slide */
    short next; /* "next" slide, with LRU last */
    short prev; /* "previous" slide */
};

struct rect {
    int left;
    int right;
    int top;
    int bottom;
};

struct load_slide_event_data {
    int slide_index;
    int cache_index;
};

struct pf_slide_cache
{
    struct slide_cache cache[SLIDE_CACHE_SIZE];
    int free;
    int used;
    int left_idx;
    int right_idx;
    int center_idx;
};

struct pf_scroll_line_info {
    long ticks;         /* number of ticks between each move */
    long delay;         /* number of ticks to delay starting scrolling */
    int step;           /* pixels to move */
    long next_scroll;   /* tick of the next move */
};

struct pf_scroll_line {
    int width;          /* width of the string */
    int offset;         /* x coordinate of the string */
    int step;           /* 0 if scroll is disabled. otherwise, pixels to move */
    long start_tick;    /* tick when to start scrolling */
};

struct pfraw_header {
    int32_t width;          /* bmap width in pixels */
    int32_t height;         /* bmap height in pixels */
};

static struct albumart_t aa_cache;
static struct pf_config_t pf_cfg;

/** below we allocate the memory we want to use **/

static pix_t *buffer; /* for now it always points to the lcd framebuffer */
int pf_height;           /* viewport height (LCD_HEIGHT minus status bar) */
static int pf_half_height;      /* rows of drawable (post text-margin) height above center */
static int pf_lower_half;       /* rows of drawable (post text-margin) height below center */
static int pf_draw_y_shift;     /* rows skipped below pf_vp_y for a TOP text caption's margin */
/* Album name font: global_settings.bold_font_file loaded via font_load(),
 * or the real UI font if none is configured. Provided by font_get_ui_bold()
 * (settings.c), which owns the font's lifecycle -- this screen must not unload
 * it. Set once in init(). */
int pf_bold_font;
static int pf_vp_y;             /* viewport y-offset (status bar height) */
static struct viewport pf_vp;   /* PF rendering viewport (below status bar) */
static struct frame_buffer_t pf_framebuffer; /* bypass backdrop in clear_viewport */

static struct slide_data center_slide;
static struct slide_data left_slides[MAX_SLIDES_COUNT];
static struct slide_data right_slides[MAX_SLIDES_COUNT];
static int slide_frame;
static int step;
static int target;
static int fade;
/* Always 0 in this port -- the old in-house zoom-in/zoom-out cover
 * animation that used to drive this was removed along with the in-house
 * track list, but render_all_slides's fade math still references it
 * verbatim, so it's kept as a no-op. */
static int extra_fade;
int center_index = 0; /* index of the slide that is in the center */
static int itilt;
static PFreal offsetX;
static PFreal auto_slide_spacing;
static PFreal offsetY;
static int number_of_slides;

static struct pf_slide_cache pf_sldcache;

/* use long for aligning */
unsigned long thread_stack[THREAD_STACK_SIZE / sizeof(long)];
/* queue (as array) for scheduling load_surface */

static int empty_slide_hid;

/* Index of the "coverflow" size in the shared albumart cache (albumart_sizes.h),
 * resolved once in init(); -1 if unavailable (falls back to the local pfraw
 * cache). */
static int pf_cover_size_idx = -1;

unsigned int thread_id;
struct event_queue thread_q;

static struct tagcache_search tcs;

static struct buflib_context buf_ctx;

struct pf_index_t pf_idx;

/* Scratch id3 used to resolve the currently playing/selected track into an
 * album index (id3_get_index()) and to look up album art (retrieve_id3()).
 */
static struct mp3entry id3;

static bool thread_is_running;
static bool wants_to_quit;

/*
    Prevent picture loading thread from allocating
    buflib memory while the main thread may be
    performing buffer-shifting operations.
*/
static struct mutex buf_ctx_mutex;
static bool buf_ctx_locked;

static struct pf_scroll_line_info scroll_line_info;
static struct pf_scroll_line scroll_lines[PF_MAX_SCROLL_LINES];

enum ePFS{ePFS_ARTIST = 0, ePFS_ALBUM};

/*
 * States: pf_idle <-> pf_scrolling (browsing covers); SELECT on a cover
 * jumps straight into the core database's track list for that album (see
 * tagtree_enter_album_tracks_on_next_load(), called from
 * album_covers_loop()) instead of an in-house zoom animation and track-list
 * screen -- there are no separate cover-zoom/track-list browsing states
 * anymore, unlike the original plugin.
 */
enum pf_states {
    pf_idle = 0,
    pf_scrolling
};

static int pf_state;

/** code */
static bool free_slide_prio(int prio);
bool load_new_slide(void);
int load_surface(int);
static void draw_progressbar(int step, int count, char *msg);
static void free_all_slide_prio(int prio);

/* struct carousel_model (the engine<->data seam) is declared in carousel.h. */

static int  album_build_index(void);
static int  album_count(void);
static bool get_slide_dir(const int slide_index, char *dir, int dirlen);
static bool album_legacy_art(int index, char *path, int len);
static int  album_enter(int index);
static int  jmp_idx_prev(void);
static int  jmp_idx_next(void);
static void draw_album_text(void);
static void album_sort_next(void);
static void album_sort_prev(void);
static void set_initial_slide(const char *selected_file);
static int  album_on_menu(void);
static void album_idle(void);
static void album_prepare(void);

static const struct carousel_model album_model = {
    .build_index = album_build_index,
    .count       = album_count,
    .slide_art   = get_slide_dir,
    .legacy_art  = album_legacy_art,
    .enter       = album_enter,
    .jump_prev   = jmp_idx_prev,
    .jump_next   = jmp_idx_next,
    .draw_text   = draw_album_text,
    .sort_next   = album_sort_next,
    .sort_prev   = album_sort_prev,
    .set_initial = set_initial_slide,
    .on_menu     = album_on_menu,
    .idle        = album_idle,
    .prepare     = album_prepare,
    .has_pfraw_cache = true,
    .title       = "Album Covers",
};

/* The artist-portraits model lives in artist_portraits.c (over the same
 * carousel engine). The active model is selected by the entry point
 * (album_covers / artist_portraits) before init() runs. */
static const struct carousel_model *model = &album_model;

static inline void buf_ctx_lock(void)
{
    mutex_lock(&buf_ctx_mutex);
    buf_ctx_locked = true;
}

static inline void buf_ctx_unlock(void)
{
    mutex_unlock(&buf_ctx_mutex);
    buf_ctx_locked = false;
}

/* Minimal stand-in for the plugin-only apps/plugins/lib/configfile.c (which
 * can't be reused as-is -- it derives its file path via
 * plugin_get_current_filename(), a genuinely plugin-lifecycle-only call).
 * This is deliberately bare-bones (a single fixed path, no version-gated
 * migration) since it's a transitional bridge -- real settings persistence
 * moves to global_settings/settings_list.c shortly after this lands. Reuses
 * the exact same line-parsing core functions (read_line/settings_parseline,
 * apps/misc.h) the plugin-lib version itself wraps via the plugin API's
 * function-pointer indirection. */
#define TYPE_INT  1
#define TYPE_ENUM 2
#define TYPE_BOOL 4

struct configdata
{
    int type;
    int min;
    int max;    /* enum: number of values */
    union {
        int *int_p;
        bool *bool_p;
    };
    const char *name;
    const char * const *values; /* enum only */
};

static void config_load(const char *filename, const struct configdata *cfg,
                        int num_items)
{
    int fd;
    int i, j;
    char *name;
    char *val;
    char buf[128];
    int tmp;

    fd = open(filename, O_RDONLY);
    if (fd < 0)
        return;

    while (read_line(fd, buf, sizeof(buf)) > 0)
    {
        settings_parseline(buf, &name, &val);
        for (i = 0; i < num_items; i++)
        {
            if (strcmp(cfg[i].name, name))
                continue;
            switch (cfg[i].type)
            {
                case TYPE_INT:
                    tmp = atoi(val);
                    if (tmp >= cfg[i].min && tmp <= cfg[i].max)
                        *cfg[i].int_p = tmp;
                    break;
                case TYPE_BOOL:
                    *cfg[i].bool_p = (bool)atoi(val);
                    break;
                case TYPE_ENUM:
                    for (j = 0; j < cfg[i].max; j++)
                    {
                        if (!strcmp(cfg[i].values[j], val))
                            *cfg[i].int_p = j;
                    }
                    break;
            }
            break;
        }
    }
    close(fd);
}

static void config_save(const char *filename, const struct configdata *cfg,
                        int num_items)
{
    int fd = creat(filename, 0666);
    int i;

    if (fd < 0)
        return;

    for (i = 0; i < num_items; i++)
    {
        switch (cfg[i].type)
        {
            case TYPE_INT:
                fdprintf(fd, "%s: %d\n", cfg[i].name, *cfg[i].int_p);
                break;
            case TYPE_BOOL:
                fdprintf(fd, "%s: %d\n", cfg[i].name, (int)*cfg[i].bool_p);
                break;
            case TYPE_ENUM:
                fdprintf(fd, "%s: %s\n", cfg[i].name,
                         cfg[i].values[*cfg[i].int_p]);
                break;
        }
    }
    close(fd);
}

static struct configdata config[] =
{
    { TYPE_INT, 0, 100, { .int_p = &pf_cfg.cache_version }, "cache version", NULL },
    { TYPE_BOOL, 0, 1, { .bool_p = &pf_cfg.update_albumart }, "update albumart", NULL },
    { TYPE_INT, 0, 999999, { .int_p = &pf_cfg.last_album }, "last album", NULL },
    { TYPE_INT, 0, 999999, { .int_p = &aa_cache.idx }, "art cache pos", NULL },
    { TYPE_INT, 0, 999999, { .int_p = &aa_cache.inspected }, "art cache inspected", NULL },
};

#define CONFIG_NUM_ITEMS (sizeof(config) / sizeof(struct configdata))

static void pf_config_save(void)
{
    config_save(CONFIG_FILE, config, CONFIG_NUM_ITEMS);
}

static void pf_config_load(void)
{
    config_load(CONFIG_FILE, config, CONFIG_NUM_ITEMS);
}

static bool check_database(void)
{
    bool needwarn = true;
    int spin = 5;

    struct tagcache_stat *stat = tagcache_get_stat();

    while ( !(stat->initialized && stat->ready) )
    {
        if (--spin > 0)
        {
            sleep(HZ/5);
        }
        else if (needwarn)
        {
            needwarn = false;
            splash(0, ID2P(LANG_TAGCACHE_BUSY));
        }
        else
            return false;

        yield();
        stat = tagcache_get_stat();
    }
    return true;
}

static bool progress_cancel(int step, int count, char *msg)
{
    const struct text_message prompt = {
            (const char*[]) {"Quit?", "Progress will be lost"}, 2};

    int action = get_action(CONTEXT_STD,TIMEOUT_NOBLOCK);
    if (action == ACTION_STD_CANCEL || action == ACTION_STD_MENU)
    {
        if (gui_syncyesno_run(&prompt, NULL, NULL) == YESNO_YES)
            return true;
        lcd_clear_display();
    }

    if (count)
        draw_progressbar(step, count, msg);

    return false;
}

static void config_set_defaults(struct pf_config_t *cfg)
{
     cfg->cache_version = CACHE_REBUILD;
     cfg->update_albumart = false;
     cfg->last_album = 0;
}

static inline PFreal fmul(PFreal a, PFreal b)
{
    return (a*b) >> PFREAL_SHIFT;
}

/**
 * This version preshifts each operand, which is useful when we know how many
 * of the least significant bits will be empty, or are worried about overflow
 * in a particular calculation
 */
static inline PFreal fmuln(PFreal a, PFreal b, int ps1, int ps2)
{
    return ((a >> ps1) * (b >> ps2)) >> (PFREAL_SHIFT - ps1 - ps2);
}

/* ARMv5+ has a clz instruction equivalent to our function.
 */
#if (defined(CPU_ARM) && (ARM_ARCH > 4))
static inline int clz(uint32_t v)
{
    return __builtin_clz(v);
}

/* Otherwise, use our clz, which can be inlined */
#elif defined(CPU_COLDFIRE)
/* This clz is based on the log2(n) implementation at
 * http://graphics.stanford.edu/~seander/bithacks.html#IntegerLog
 * A clz benchmark plugin showed this to be about 14% faster on coldfire
 * than the LUT-based version.
 */
static inline int clz(uint32_t v)
{
    int r = 32;
    if (v >= 0x10000)
    {
        v >>= 16;
        r -= 16;
    }
    if (v & 0xff00)
    {
        v >>= 8;
        r -= 8;
    }
    if (v & 0xf0)
    {
        v >>= 4;
        r -= 4;
    }
    if (v & 0xc)
    {
        v >>= 2;
        r -= 2;
    }
    if (v & 2)
    {
        v >>= 1;
        r -= 1;
    }
    r -= v;
    return r;
}
#else
static const char clz_lut[16] = { 4, 3, 2, 2, 1, 1, 1, 1,
                                  0, 0, 0, 0, 0, 0, 0, 0 };
/* This clz is based on the log2(n) implementation at
 * http://graphics.stanford.edu/~seander/bithacks.html#IntegerLogLookup
 * It is not any faster than the one above, but trades 16B in the lookup table
 * for a savings of 12B per each inlined call.
 */
static inline int clz(uint32_t v)
{
    int r = 28;
    if (v >= 0x10000)
    {
        v >>= 16;
        r -= 16;
    }
    if (v & 0xff00)
    {
        v >>= 8;
        r -= 8;
    }
    if (v & 0xf0)
    {
        v >>= 4;
        r -= 4;
    }
    return r + clz_lut[v];
}
#endif

/* Return the maximum possible left shift for a signed int32, without
 * overflow
 */
static inline int allowed_shift(int32_t val)
{
    uint32_t uval = val ^ (val >> 31);
    return clz(uval) - 1;
}

/* Calculate num/den, with the result shifted left by PFREAL_SHIFT, by shifting
 * num and den before dividing.
 */
static inline PFreal fdiv(PFreal num, PFreal den)
{
    int shift = allowed_shift(num);
    shift = MIN(PFREAL_SHIFT, shift);
    num <<= shift;
    den >>= PFREAL_SHIFT - shift;
    return num / den;
}

#define fmin(a,b) (((a) < (b)) ? (a) : (b))
#define fmax(a,b) (((a) > (b)) ? (a) : (b))
#define fabs(a) (a < 0 ? -a : a)
#define fbound(min,val,max) (fmax((min),fmin((max),(val))))

#define MULUQ(a, b) ((a) * (b))

/* warning: regenerate the table if IANGLE_MAX and PFREAL_SHIFT are changed! */
static const short sin_tab[] = {
        0,   100,   200,   297,   392,   483,   569,   650,
      724,   792,   851,   903,   946,   980,  1004,  1019,
     1024,  1019,  1004,   980,   946,   903,   851,   792,
      724,   650,   569,   483,   392,   297,   200,   100,
        0,  -100,  -200,  -297,  -392,  -483,  -569,  -650,
     -724,  -792,  -851,  -903,  -946,  -980, -1004, -1019,
    -1024, -1019, -1004,  -980,  -946,  -903,  -851,  -792,
     -724,  -650,  -569,  -483,  -392,  -297,  -200,  -100,
        0
};

static inline PFreal fsin(int iangle)
{
    iangle &= IANGLE_MASK;

    int i = (iangle >> 4);
    PFreal p = sin_tab[i];
    PFreal q = sin_tab[(i+1)];
    PFreal g = (q - p);
    return p + g * (iangle-i*16)/16;
}

static inline PFreal fcos(int iangle)
{
    return fsin(iangle + (IANGLE_MAX >> 2));
}

static void output_row_8_transposed(uint32_t row, void * row_in,
                                       struct scaler_context *ctx)
{
    pix_t *dest = (pix_t*)ctx->bm->data + row;
    pix_t *end = dest + ctx->bm->height * ctx->bm->width;
#ifdef USEGSLIB
    uint8_t *qp = (uint8_t*)row_in;
    for (; dest < end; dest += ctx->bm->height)
        *dest = *qp++;
#else
    struct uint8_rgb *qp = (struct uint8_rgb*)row_in;
    unsigned r, g, b;
    int col = 0;
    uint8_t dy = PF_DITHERY(row);
    for (; dest < end; dest += ctx->bm->height, col++)
    {
        int delta = ctx->dither ? PF_DITHERXDY(col, dy) : 127;
        r = (31 * qp->red + (qp->red >> 3) + delta) >> 8;
        g = (63 * qp->green + (qp->green >> 2) + delta) >> 8;
        b = (31 * qp->blue + (qp->blue >> 3) + delta) >> 8;
        qp++;
        *dest = FB_RGBPACK_LCD(r, g, b);
    }
#endif
}

/* read_image_file() is called without FORMAT_TRANSPARENT so
 * it's safe to ignore alpha channel in the next two functions */
static void output_row_32_transposed(uint32_t row, void * row_in,
                                       struct scaler_context *ctx)
{
    pix_t *dest = (pix_t*)ctx->bm->data + row;
    pix_t *end = dest + ctx->bm->height * ctx->bm->width;
#ifdef USEGSLIB
    uint32_t *qp = (uint32_t*)row_in;
    for (; dest < end; dest += ctx->bm->height)
        *dest = SC_OUT(*qp++, ctx);
#else
    struct uint32_argb *qp = (struct uint32_argb*)row_in;
    int r, g, b;
    int col = 0;
    uint8_t dy = PF_DITHERY(row);
    for (; dest < end; dest += ctx->bm->height, col++)
    {
        int delta = ctx->dither ? PF_DITHERXDY(col, dy) : 127;
        r = SC_OUT(qp->r, ctx);
        g = SC_OUT(qp->g, ctx);
        b = SC_OUT(qp->b, ctx);
        qp++;
        r = (31 * r + (r >> 3) + delta) >> 8;
        g = (63 * g + (g >> 2) + delta) >> 8;
        b = (31 * b + (b >> 3) + delta) >> 8;
        *dest = FB_RGBPACK_LCD(r, g, b);
    }
#endif
}

#ifdef HAVE_LCD_COLOR
static void output_row_32_transposed_fromyuv(uint32_t row, void * row_in,
                                       struct scaler_context *ctx)
{
    pix_t *dest = (pix_t*)ctx->bm->data + row;
    pix_t *end = dest + ctx->bm->height * ctx->bm->width;
    struct uint32_argb *qp = (struct uint32_argb*)row_in;
    int col = 0;
    uint8_t dy = PF_DITHERY(row);
    for (; dest < end; dest += ctx->bm->height, col++)
    {
        unsigned r, g, b, y, u, v;
        int delta = ctx->dither ? PF_DITHERXDY(col, dy) : 127;
        y = SC_OUT(qp->b, ctx);
        u = SC_OUT(qp->g, ctx);
        v = SC_OUT(qp->r, ctx);
        qp++;
        yuv_to_rgb(y, u, v, &r, &g, &b);
        r = (31 * r + (r >> 3) + delta) >> 8;
        g = (63 * g + (g >> 2) + delta) >> 8;
        b = (31 * b + (b >> 3) + delta) >> 8;
        *dest = FB_RGBPACK_LCD(r, g, b);
    }
}
#endif

static unsigned int get_size(struct bitmap *bm)
{
    return bm->width * bm->height * sizeof(pix_t);
}

const struct custom_format format_transposed = {
    .output_row_8 = output_row_8_transposed,
#ifdef HAVE_LCD_COLOR
    .output_row_32 = {
        output_row_32_transposed,
        output_row_32_transposed_fromyuv
    },
#else
    .output_row_32 = output_row_32_transposed,
#endif
    .get_size = get_size
};

static const struct button_mapping* get_context_map(int context)
{
    /* action_code_lookup() ORs in CONTEXT_REMOTE for any button with
     * BUTTON_REMOTE bits set (true on this target, which supports an
     * accessory/headphone remote) -- without masking it here too,
     * pf_contexts[context & ~CONTEXT_PLUGIN] indexes ~2 billion entries
     * past this 2-element array on a remote button press. */
    context &= ~(CONTEXT_LOCKED | CONTEXT_REMOTE);
    return pf_contexts[context & ~CONTEXT_PLUGIN];
}

/* scrolling */
static void init_scroll_lines(void)
{
    int i;
    static const char scroll_tick_table[16] = {
     /* Hz values:
        1, 1.25, 1.55, 2, 2.5, 3.12, 4, 5, 6.25, 8.33, 10, 12.5, 16.7, 20, 25, 33 */
        100, 80, 64, 50, 40, 32, 25, 20, 16, 12, 10, 8, 6, 5, 4, 3
    };

    scroll_line_info.ticks = scroll_tick_table[global_settings.scroll_speed];
    scroll_line_info.step = global_settings.scroll_step;
    scroll_line_info.delay = global_settings.scroll_delay / (HZ / 10);
    scroll_line_info.next_scroll = current_tick;
    for (i = 0; i < PF_MAX_SCROLL_LINES; i++)
        scroll_lines[i].step = 0;
}

void set_scroll_line(const char *str, enum pf_scroll_line_type type)
{
    struct pf_scroll_line *s = &scroll_lines[type];
    s->width = lcd_getstringsize(str, NULL, NULL);
    s->step = 0;
    s->offset = 0;
    s->start_tick = current_tick + scroll_line_info.delay;
    if (LCD_WIDTH - s->width < 0)
        s->step = scroll_line_info.step;
    else
        s->offset = (LCD_WIDTH - s->width) / 2;
}

int get_scroll_line_offset(enum pf_scroll_line_type type)
{
    return scroll_lines[type].offset;
}

static void update_scroll_lines(void)
{
    int i;

    if (TIME_BEFORE(current_tick, scroll_line_info.next_scroll))
        return;

    scroll_line_info.next_scroll = current_tick + scroll_line_info.ticks;

    for (i = 0; i < PF_MAX_SCROLL_LINES; i++)
    {
        struct pf_scroll_line *s = &scroll_lines[i];
        if (s->step && TIME_BEFORE(s->start_tick, current_tick))
        {
            s->offset -= s->step;

            if (s->offset >= 0) {
                /* at beginning of line */
                s->offset = 0;
                s->step = scroll_line_info.step;
                s->start_tick = current_tick + scroll_line_info.delay * 2;
            }
            if (s->offset <= LCD_WIDTH - s->width) {
                /* at end of line */
                s->offset = LCD_WIDTH - s->width;
                s->step = -scroll_line_info.step;
                s->start_tick = current_tick + scroll_line_info.delay * 2;
            }
        }
    }
}

/* Create the lookup table with the scaling values for the reflections */

static int compare_albums (const void *a_v, const void *b_v)
{
    uint32_t artist_a = ((struct album_data *)a_v)->artist_idx;
    uint32_t artist_b = ((struct album_data *)b_v)->artist_idx;

    uint32_t album_a = ((struct album_data *)a_v)->name_idx;
    uint32_t album_b = ((struct album_data *)b_v)->name_idx;

    int year_a = ((struct album_data *)a_v)->year;
    int year_b = ((struct album_data *)b_v)->year;

    switch (global_settings.album_covers_sort_albums_by)
    {
        case SORT_BY_ARTIST_AND_NAME:
            if (artist_a - artist_b == 0)
                return (int)(album_a - album_b);
            break;
        case SORT_BY_ARTIST_AND_YEAR:
            if (artist_a - artist_b == 0)
            {
                if (global_settings.album_covers_year_sort_order == ASCENDING)
                    return year_a - year_b;
                else
                    return year_b - year_a;
            }
            break;
        case SORT_BY_YEAR:
            if (year_a - year_b != 0)
            {
                if (global_settings.album_covers_year_sort_order == ASCENDING)
                    return year_a - year_b;
                else
                    return year_b - year_a;
            }
            break;
        case SORT_BY_NAME:
            if (album_a - album_b != 0)
                return (int)(album_a - album_b);
            break;
    }

    return (int)(artist_a - artist_b);
}

static int compare_album_artists (const void *a_v, const void *b_v)
{
    uint32_t a = ((struct album_data *)a_v)->artist_idx;
    uint32_t b = ((struct album_data *)b_v)->artist_idx;
    return (int)(a - b);
}

static void write_album_index(int idx, int name_idx,
                              long album_seek, int artist_idx, long artist_seek)
{
    pf_idx.album_index[idx].name_idx = name_idx;
    pf_idx.album_index[idx].seek = album_seek;
    pf_idx.album_index[idx].artist_idx = artist_idx;
    pf_idx.album_index[idx].artist_seek = artist_seek;
    pf_idx.album_index[idx].year = 0;
}

static inline void write_album_entry(struct tagcache_search *tcs,
                                     int name_idx, unsigned int len)
{
    write_album_index(-pf_idx.album_ct, name_idx, tcs->result_seek, 0, -1);
    pf_idx.album_len += len;
    pf_idx.album_ct++;

    if (pf_idx.album_untagged_seek == -1 && strcmp(UNTAGGED, tcs->result) == 0)
    {
        pf_idx.album_untagged_idx = name_idx;
        pf_idx.album_untagged_seek = tcs->result_seek;
    }
}

static void write_artist_entry(struct tagcache_search *tcs,
                               int name_idx, unsigned int len)
{
    pf_idx.artist_index[-pf_idx.artist_ct].name_idx = name_idx;
    pf_idx.artist_index[-pf_idx.artist_ct].seek = tcs->result_seek;
    pf_idx.artist_len += len;
    pf_idx.artist_ct++;
}

/* adds tagcache_search results into artist/album index */
static int get_tcs_search_res(int type, struct tagcache_search *tcs,
                              void **buf, size_t *bufsz)
{
    char tcs_buf[TAGCACHE_BUFSZ];
    const long tcs_bufsz = sizeof(tcs_buf);
    int ret = SUCCESS;
    unsigned int l, name_idx = 0;
    void (*writefn)(struct tagcache_search *, int, unsigned int);
    int data_size;
    if (type == ePFS_ARTIST)
    {
        writefn = &write_artist_entry;
        data_size = sizeof(struct artist_data);
    }
    else
    {
        writefn = &write_album_entry;
        data_size = sizeof(struct album_data);
    }

    while (tagcache_get_next(tcs, tcs_buf, tcs_bufsz))
    {
        if (progress_cancel(0, 0, NULL))
        {
            ret = ERROR_USER_ABORT;
            break;
        }

        l = tcs->result_len;

        /* Check before subtracting -- *bufsz is unsigned, so subtracting
         * data_size (or l) once the real remaining space is smaller than
         * that would wrap to a huge value instead of going negative,
         * permanently defeating this check for the rest of the scan and
         * letting strcpy()/writefn() below walk past the end of
         * pf_idx.buf on any library large enough to fill it. */
        if ((size_t)data_size > *bufsz || l > *bufsz - data_size)
        {
            /* not enough memory */
            ret = ERROR_BUFFER_FULL;
            break;
        }

        *bufsz -= data_size;

        strcpy(*buf, tcs->result);

        *bufsz -= l;
        *buf = l + (char *)*buf;

        writefn(tcs, name_idx, l);

        name_idx += l;
    }
    tagcache_search_finish(tcs);
    return ret;
}

#define STR_STEP_INDEXING_UNTAGGED "1/5 Find " UNTAGGED
#define STR_STEP_ASSIGNING_ALBUMS "2/5 Find Albums"
#define STR_STEP_ASSIGNING_ALBUM_YEAR "3/5 Check Album Year"
#define STR_STEP_REMOVING_DUPLICATES "4/5 Remove Duplicates"
#define STR_STEP_PREPARING_ARTWORK "5/5 Prepare Artwork"

/*adds <untagged> albums/artist to existing album index */
static int create_album_untagged(struct tagcache_search *tcs, size_t *bufsz)
{
    static char tcs_buf[TAGCACHE_BUFSZ];
    const long tcs_bufsz = sizeof(tcs_buf);
    int ret = SUCCESS;
    int album_count = pf_idx.album_ct; /* store existing count */
    int total_count = pf_idx.album_ct + pf_idx.artist_ct * 2;
    long seek;
    int last, final, retry;
    int i, j;
    splash_progress_set_delay(HZ / 2);
    draw_progressbar(0, total_count, STR_STEP_INDEXING_UNTAGGED);

    /* search tagcache for all <untagged> albums & save the albumartist seek pos */
    if (tagcache_search(tcs, tag_albumartist))
    {
        tagcache_search_add_filter(tcs, tag_album, pf_idx.album_untagged_seek);

        while (tagcache_get_next(tcs, tcs_buf, tcs_bufsz))
        {
            if (progress_cancel(pf_idx.album_ct, total_count, STR_STEP_INDEXING_UNTAGGED))
            {
                tagcache_search_finish(tcs);
                return ERROR_USER_ABORT;
            }

            if (tcs->result_seek ==
                pf_idx.album_index[-(pf_idx.album_ct - 1)].artist_seek)
                continue;

            if (sizeof(struct album_data) > *bufsz)
            {
                /* not enough memory */
                ret = ERROR_BUFFER_FULL;
                break;
            }

            *bufsz -= sizeof(struct album_data);
            write_album_index(-pf_idx.album_ct, pf_idx.album_untagged_idx,
                               pf_idx.album_untagged_seek, -1, tcs->result_seek);

            pf_idx.album_ct++;
        }
        tagcache_search_finish(tcs);

        if (ret == SUCCESS) {
            draw_progressbar(0, pf_idx.album_ct, STR_STEP_INDEXING_UNTAGGED);

            last = 0;
            final = pf_idx.artist_ct;
            retry = 0;

            /* map the artist_seek position to the artist name index */
            for (j = album_count; j < pf_idx.album_ct; j++)
            {
                if (progress_cancel(j, pf_idx.album_ct, STR_STEP_INDEXING_UNTAGGED))
                    return ERROR_USER_ABORT;

                seek = pf_idx.album_index[-j].artist_seek;

    retry_artist_lookup:
                retry++;
                for (i = last; i < final; i++)
                {
                    if (seek == pf_idx.artist_index[i].seek)
                    {
                        int idx = pf_idx.artist_index[i].name_idx;
                        pf_idx.album_index[-j].artist_idx = idx;
                        last = i; /* last match, start here next loop */
                        final = pf_idx.artist_ct;
                        retry = 0;
                        break;
                    }
                }
                if (retry > 0 && retry < 2)
                {
                    /* no match start back at beginning */
                    final = last;
                    last = 0;
                    goto retry_artist_lookup;
                }
            }
        }
    }

    return ret;
}

/* Create an index of all artists from the database */
int build_artist_index(struct tagcache_search *tcs,
                                 void **buf, size_t *bufsz)
{
    int i, res = SUCCESS;
    struct artist_data* tmp_artist;

    /* artist index starts at end of buf it will be rearranged when finalized */
    pf_idx.artist_index = ((struct artist_data *)(*bufsz + (char *) *buf)) - 1;
    pf_idx.artist_ct = 0;
    pf_idx.artist_len = 0;
    /* artist names starts at beginning of buf */
    pf_idx.artist_names = *buf;

    tagcache_search(tcs, tag_albumartist);
    res = get_tcs_search_res(ePFS_ARTIST, tcs, &(*buf), bufsz);
    tagcache_search_finish(tcs);
    if (res < SUCCESS)
        return res;

    /* finalize the artist index */
    ALIGN_BUFFER(*buf, *bufsz, alignof(struct artist_data));
    tmp_artist = (struct artist_data*)*buf;
    for (i = pf_idx.artist_ct - 1; i >= 0; i--)
        tmp_artist[i] = pf_idx.artist_index[-i];

    pf_idx.artist_index = tmp_artist;
    /* move buf ptr to end of artist_index */
    *buf = pf_idx.artist_index + pf_idx.artist_ct;

    if (res == SUCCESS)
    {
        if (pf_idx.artist_ct > 0)
            res = pf_idx.artist_ct;
        else
            res = ERROR_NO_ALBUMS;
    }

    return res;
}

static int assign_album_year(void)
{
    char tcs_buf[TAGCACHE_BUFSZ];
    const long tcs_bufsz = sizeof(tcs_buf);
    splash_progress_set_delay(HZ / 2);
    draw_progressbar(0, pf_idx.album_ct, STR_STEP_ASSIGNING_ALBUM_YEAR);
    for (int album_idx = 0; album_idx < pf_idx.album_ct; album_idx++)
    {
        /* Prevent idle poweroff */
        reset_poweroff_timer();

        if (progress_cancel(album_idx, pf_idx.album_ct, STR_STEP_ASSIGNING_ALBUM_YEAR))
            return ERROR_USER_ABORT;

        int album_year = 0;

        if (tagcache_search(&tcs, tag_year))
        {
            tagcache_search_add_filter(&tcs, tag_album,
                                       pf_idx.album_index[album_idx].seek);

            if (pf_idx.album_index[album_idx].artist_idx >= 0)
                tagcache_search_add_filter(&tcs, tag_albumartist,
                    pf_idx.album_index[album_idx].artist_seek);

            while (tagcache_get_next(&tcs, tcs_buf, tcs_bufsz)) {
                int track_year = tagcache_get_numeric(&tcs, tag_year);
                if (track_year > album_year)
                    album_year = track_year;
            }
        }
        tagcache_search_finish(&tcs);

        pf_idx.album_index[album_idx].year = album_year;
    }
    return SUCCESS;
}

/**
  Create an index of all artists and albums from the database.
  Also store the artists and album names so we can access them later.
 */
static int create_album_index(void)
{
    static char tcs_buf[TAGCACHE_BUFSZ];
    const long tcs_bufsz = sizeof(tcs_buf);
    void *buf = pf_idx.buf;
    size_t buf_size = pf_idx.buf_sz;

    struct album_data* tmp_album;

    int i, j, last, final, retry, res;

    ALIGN_BUFFER(buf, buf_size, sizeof(long));

    /* Artists */
    res = build_artist_index(&tcs, &buf, &buf_size);
    if (res < SUCCESS)
        return res;

    /* Albums */
    pf_idx.album_ct = 0;
    pf_idx.album_len =0;
    pf_idx.album_untagged_idx = -1;
    pf_idx.album_untagged_seek = -1;

    /* album_index starts at end of buf it will be rearranged when finalized */
    pf_idx.album_index = ((struct album_data *)(buf_size + (char *)buf)) - 1;
    /* album_names starts at the beginning of buf */
    pf_idx.album_names = buf;

    tagcache_search(&tcs, tag_album);
    res = get_tcs_search_res(ePFS_ALBUM, &tcs, &buf, &buf_size);
    tagcache_search_finish(&tcs);
    if (res < SUCCESS)
        return res;

    /* Build artist list for untagged albums */
    res = create_album_untagged(&tcs, &buf_size);

    if (res < SUCCESS)
        return res;

    /* finalize the album index */
    ALIGN_BUFFER(buf, buf_size, alignof(struct album_data));
    tmp_album = (struct album_data*)buf;
    for (i = pf_idx.album_ct - 1; i >= 0; i--)
        tmp_album[i] = pf_idx.album_index[-i];

    pf_idx.album_index = tmp_album;
    /* move buf ptr to end of album_index */
    buf = pf_idx.album_index + pf_idx.album_ct;

    /* Assign indices */
    splash_progress_set_delay(HZ / 2);
    draw_progressbar(0, pf_idx.album_ct, STR_STEP_ASSIGNING_ALBUMS);
    for (j = 0; j < pf_idx.album_ct; j++)
    {
        /* Prevent idle poweroff */
        reset_poweroff_timer();

        if (progress_cancel(j, pf_idx.album_ct, STR_STEP_ASSIGNING_ALBUMS))
            return ERROR_USER_ABORT;

        if (pf_idx.album_index[j].artist_seek >= 0) { continue; }

        tagcache_search(&tcs, tag_albumartist);
        tagcache_search_add_filter(&tcs, tag_album, pf_idx.album_index[j].seek);

        last = 0;
        final = pf_idx.artist_ct;
        retry = 0;
        if (tagcache_get_next(&tcs, tcs_buf, tcs_bufsz))
        {

retry_artist_lookup:
            retry++;
            for (i = last; i < final; i++)
            {
                if (tcs.result_seek == pf_idx.artist_index[i].seek)
                {
                    int idx = pf_idx.artist_index[i].name_idx;
                    pf_idx.album_index[j].artist_idx = idx;
                    pf_idx.album_index[j].artist_seek = tcs.result_seek;
                    last = i; /* last match, start here next loop */
                    final = pf_idx.artist_ct;
                    retry = 0;
                    break;
                }
            }
            if (retry > 0 && retry < 2)
            {
                /* no match start back at beginning */
                final = last;
                last = 0;
                goto retry_artist_lookup;
            }
        }
        tagcache_search_finish(&tcs);
    }

    res = assign_album_year();

    if (res < SUCCESS)
        return res;

    /* sort list order to find duplicates */
    qsort(pf_idx.album_index, pf_idx.album_ct,
              sizeof(struct album_data), compare_album_artists);

    splash_progress_set_delay(HZ / 2);
    draw_progressbar(0, pf_idx.album_ct, STR_STEP_REMOVING_DUPLICATES);
    /* mark duplicate albums for deletion */
    for (i = 0; i < pf_idx.album_ct - 1; i++) /* -1 don't check last entry */
    {
        /* Prevent idle poweroff */
        reset_poweroff_timer();

        if (progress_cancel(i, pf_idx.album_ct, STR_STEP_REMOVING_DUPLICATES))
            return ERROR_USER_ABORT;

        int idxi = pf_idx.album_index[i].artist_idx;
        int seeki = pf_idx.album_index[i].seek;

        for (j = i + 1; j < pf_idx.album_ct; j++)
        {
            if (idxi > 0 &&
            idxi == pf_idx.album_index[j].artist_idx &&
            seeki == pf_idx.album_index[j].seek)
            {
                pf_idx.album_index[j].artist_idx = -1;
            }
            else
            {
                i = j - 1;
                break;
            }
        }
    }

    /* now fix the album list order */
    qsort(pf_idx.album_index, pf_idx.album_ct,
              sizeof(struct album_data), compare_album_artists);

    /* remove any extra untagged albums
     * extra space is orphaned till restart */
    pf_idx.album_index += pf_idx.album_untagged_idx + 1;
    pf_idx.album_ct -= pf_idx.album_untagged_idx + 1;

    pf_idx.buf = buf;
    pf_idx.buf_sz = buf_size;
    pf_idx.artist_index = 0;

    qsort(pf_idx.album_index, pf_idx.album_ct,
                          sizeof(struct album_data), compare_albums);

    return (pf_idx.album_ct > 0) ? 0 : ERROR_NO_ALBUMS;
}

/*Saves the album index into a binary file to be recovered the
 next time PictureFlow is launched*/

static int save_album_index(void){
    int fd = creat(ALBUM_INDEX,0666);

    struct pf_index_t data;
    memcpy(&data, &pf_idx, sizeof(struct pf_index_t));

    if(fd >= 0)
    {
        memcpy(&data.header, INDEX_HDR, sizeof(pf_idx.header));

        write(fd, &data, sizeof(struct pf_index_t));

        write(fd, data.artist_names, data.artist_len);
        write(fd, data.album_names, data.album_len);

        write(fd, data.album_index, data.album_ct * sizeof(struct album_data));

        close(fd);
        return 0;
    }
    return -1;
}

/* reads data from save file to buffer */
static inline int read2buf(int fildes, void *buf, size_t nbyte){
    int nread;
    nread = read(fildes, buf, nbyte);
    if (nread < (int)nbyte)
        return 0;

    return nread;
}

/*Loads the album_index information stored in the hard drive*/
static int load_album_index(void){

    int i, fr = open(ALBUM_INDEX, O_RDONLY);
    struct pf_index_t data;

    void *bufstart = pf_idx.buf;
    unsigned int bufstart_sz = pf_idx.buf_sz;

    void* buf = pf_idx.buf;
    size_t buf_size = pf_idx.buf_sz;

    unsigned int name_sz, album_idx_sz;
    int album_idx, artist_idx;

    if (fr >= 0){
        const unsigned long fsize = filesize(fr);
        if (fsize > sizeof(data))
        {
            if (read(fr, &data, sizeof(data)) == sizeof(data) &&
                memcmp(&(data.header), INDEX_HDR, sizeof(data.header)) == 0)
            {
                name_sz = data.artist_len + data.album_len;
                album_idx_sz = data.album_ct * sizeof(struct album_data);

                if (name_sz + album_idx_sz > bufstart_sz)
                    goto failure;

                //lseek(fr, sizeof(data) + 1, SEEK_SET);
                /* artist names */
                if (read2buf(fr, buf, data.artist_len) == 0)
                    goto failure;

                data.artist_names = buf;
                buf = (char *)buf + data.artist_len;
                buf_size -= data.artist_len;

                /* album names */
                if (read2buf(fr, buf, data.album_len) == 0)
                    goto failure;

                data.album_names = buf;
                buf = (char *)buf + data.album_len;
                buf_size -= data.album_len;

                /* index of album names */
                ALIGN_BUFFER(buf, buf_size, alignof(struct album_data));
                if (read2buf(fr, buf, album_idx_sz) == 0)
                    goto failure;

                data.album_index = buf;
                buf = (char *)buf + album_idx_sz;
                buf_size -= album_idx_sz;

                close(fr);

                /* sanity check loaded data */
                for (i = 0; i < data.album_ct; i++)
                {
                    album_idx = data.album_index[i].name_idx;
                    artist_idx = data.album_index[i].artist_idx;
                    if (album_idx >= (int) data.album_len ||
                        artist_idx >= (int) data.artist_len)
                    {
                        goto failure;
                    }
                }

                memcpy(&pf_idx, &data, sizeof(struct pf_index_t));
                pf_idx.buf = buf;
                pf_idx.buf_sz = buf_size;

                qsort(pf_idx.album_index, pf_idx.album_ct,
                          sizeof(struct album_data), compare_albums);

                return 0;
            }
        }
    }

failure:
    splash(HZ/2, "Failed to load index");
    if (fr >= 0)
        close(fr);

    pf_idx.buf = bufstart;
    pf_idx.buf_sz = bufstart_sz;
    pf_idx.artist_ct = 0;
    pf_idx.album_ct = 0;
    return -1;

}

/* carousel_model.build_index for the album model: reuse the on-disk index when
 * it matches the current cache version, otherwise rebuild it (and persist it).
 * Returns SUCCESS or one of the ERROR_* codes. */
static int album_build_index(void)
{
    int ret = SUCCESS;

    /* Scan will trigger when no file is found or the option was activated */
    if ((pf_cfg.cache_version != CACHE_VERSION) || (load_album_index() < 0))
    {
        ret = create_album_index();

        if (ret == 0)
        {
            pf_cfg.cache_version = CACHE_REBUILD;
            pf_config_save();
            if (save_album_index() < 0)
                splash(HZ, "Could not write index");
        }
    }
    return ret;
}

/**
 Return a pointer to the album name of the given slide_index
 */
static char* get_album_name(const int slide_index)
{
    char *name = pf_idx.album_names + pf_idx.album_index[slide_index].name_idx;
    return name;
}

/**
 Return a pointer to the album name of the given slide_index
 */
static char* get_album_name_idx(const int slide_index, int *idx)
{
    *idx = pf_idx.album_index[slide_index].name_idx;
    char *name = pf_idx.album_names + pf_idx.album_index[slide_index].name_idx;
    return name;
}

/**
 Return a pointer to the album artist of the given slide_index
 */
static char* get_album_artist(const int slide_index)
{
    if (slide_index < pf_idx.album_ct && slide_index >= 0){
        int idx = pf_idx.album_index[slide_index].artist_idx;
        if (idx >= 0 && idx < (int) pf_idx.artist_len) {
            char *name = pf_idx.artist_names + idx;
            return name;
        }
    }
    return "?";
}


static char* get_slide_name(const int slide_index, bool artist)
{
    if (artist)
        return get_album_artist(slide_index);

    return get_album_name(slide_index);
}

static int jmp_idx_prev(void)
{
    if (aa_cache.inspected < pf_idx.album_ct)
    {
#ifdef USEGSLIB
        grey_show(false);
        lcd_clear_display();
        lcd_update();
#endif
        splash(HZ*2, str(LANG_WAIT_FOR_CACHE));
#ifdef USEGSLIB
        grey_show(true);
#endif
        return center_index;
    }

    if (global_settings.album_covers_sort_albums_by == SORT_BY_YEAR)
    {
        int current_year = pf_idx.album_index[center_index].year;

        for (int i = center_index - 1; i > 0; i-- )
        {
            if(pf_idx.album_index[i].year != current_year)
                current_year = pf_idx.album_index[i].year;
            while (i > 0)
            {
                if (pf_idx.album_index[i-1].year != current_year)
                    break;
                i--;
            }
            return i;
        }
    }
    else
    {
        bool by_artist = global_settings.album_covers_sort_albums_by != SORT_BY_NAME;
        char *current_selection = get_slide_name(center_index, by_artist);

        for (int i = center_index - 1; i > 0; i-- )
        {
            if(strncmp(get_slide_name(i, by_artist), current_selection, 1))
                current_selection = get_slide_name(i, by_artist);
            while (i > 0)
            {
                if (strncmp(get_slide_name(i-1, by_artist), current_selection, 1))
                    break;
                i--;
            }
            return i;
        }
    }

    return 0;
}

static int jmp_idx_next(void)
{
    if (aa_cache.inspected < pf_idx.album_ct)
    {
#ifdef USEGSLIB
        grey_show(false);
        lcd_clear_display();
        lcd_update();
#endif
        splash(HZ*2, str(LANG_WAIT_FOR_CACHE));
#ifdef USEGSLIB
        grey_show(true);
#endif
        return center_index;
    }

    if (global_settings.album_covers_sort_albums_by == SORT_BY_YEAR)
    {
        int current_year = pf_idx.album_index[center_index].year;
        for (int i = center_index + 1; i < pf_idx.album_ct; i++ )
            if(pf_idx.album_index[i].year != current_year)
                return i;
    }
    else
    {
        bool by_artist = global_settings.album_covers_sort_albums_by != SORT_BY_NAME;
        char *current_selection = get_slide_name(center_index, by_artist);
        for (int i = center_index + 1; i < pf_idx.album_ct; i++ )
            if(strncmp(get_slide_name(i, by_artist), current_selection, 1))
                return i;
    }
    return pf_idx.album_ct - 1;
}

static int id3_get_index(struct mp3entry *id3)
{
    char* current_artist = UNTAGGED;
    char* current_album  = UNTAGGED;

    if(id3)
    {
        /* we could be looking for the artist in either field */
        if(id3->albumartist)
            current_artist = id3->albumartist;
        else if(id3->artist)
            current_artist = id3->artist;

        if (id3->album && strlen(id3->album) > 0)
            current_album = id3->album;

        //splashf(1000, "%s, %s", current_album, current_artist);

        int i;
        int album_idx, artist_idx;

        for (i = 0; i < pf_idx.album_ct; i++ )
        {
            album_idx = pf_idx.album_index[i].name_idx;
            artist_idx = pf_idx.album_index[i].artist_idx;

            if(!strcmp(pf_idx.album_names + album_idx, current_album) &&
                !strcasecmp(pf_idx.artist_names + artist_idx, current_artist))
                return i;
        }

    }
    splash(HZ/2, "Album Not Found!");
    return pf_cfg.last_album;
}

bool retrieve_id3(struct mp3entry *id3, const char* file)
{
#if defined (HAVE_TAGCACHE) && defined(HAVE_TC_RAMCACHE) && defined(HAVE_DIRCACHE)
    if (tagcache_fill_tags(id3, file))
    {
        strlcpy(id3->path, file, sizeof(id3->path));
        return true;
    }
#endif

    return get_metadata(id3, -1, file);
}

/**
  Determine filename of the album art for the given slide_index and
  store the result in buf.
  The algorithm looks for the first track of the given album uses
  find_albumart to find the filename.
 */
static bool get_albumart_for_index_from_db(const int slide_index, char *buf,
                                    int buflen)
{
    bool ret;
    char tcs_buf[TAGCACHE_BUFSZ];
    const long tcs_bufsz = sizeof(tcs_buf);
    if (tcs.valid || !tagcache_search(&tcs, tag_filename))
        return false;

    /* find the first track of the album */
    tagcache_search_add_filter(&tcs, tag_album,
                                   pf_idx.album_index[slide_index].seek);

    tagcache_search_add_filter(&tcs, tag_albumartist,
                                   pf_idx.album_index[slide_index].artist_seek);

    ret = tagcache_get_next(&tcs, tcs_buf, tcs_bufsz) &&
          retrieve_id3(&id3, tcs.result) &&
          search_albumart_files(&id3, ":", buf, buflen);

    tagcache_search_finish(&tcs);
    return ret;
}

/**
  Draw a progress popup using the shared firmware progress widget, so
  index-building looks like every other long-running operation (e.g. the
  Album art cache builder) instead of a bespoke full-screen bar.
 */
static void draw_progressbar(int step, int count, char *msg)
{
    splash_progress(step, count, "%s", msg);
}

/* Calculate modified FNV hash of string
 * has good avalanche behaviour and uniform distribution
 * see http://home.comcast.net/~bretm/hash/ */
static unsigned int mfnv(char *str)
{
    const unsigned int p = 16777619;
    unsigned int hash = 0x811C9DC5; // 2166136261;

    if (!str)
        return 0;

    while(*str)
        hash = (hash ^ *str++) * p;
    hash += hash << 13;
    hash ^= hash >> 7;
    hash += hash << 3;
    hash ^= hash >> 17;
    hash += hash << 5;
    return hash;
}

/**
 Save the given bitmap as filename in the pfraw format
 */
static bool save_pfraw(char* filename, struct bitmap *bm)
{
    struct pfraw_header bmph;
    bmph.width = bm->width;
    bmph.height = bm->height;
    int fh = open(filename, O_WRONLY|O_CREAT|O_TRUNC, 0666);
    if( fh < 0 ) return false;
    write( fh, &bmph, sizeof( struct pfraw_header ) );
    write( fh, bm->data , sizeof( pix_t ) * bm->width *  bm->height );
    close( fh );
    return true;
}

/* Inlined equivalent of apps/plugins/lib/read_image.c's read_image_file() --
 * that plugin-lib helper isn't linkable from core code, but the two core
 * functions it wraps are ordinary core functions. */
static int pf_read_image_file(const char *filename, struct bitmap *bm,
                              int maxsize, int format,
                              const struct custom_format *cformat)
{
    size_t namelen = strlen(filename);
    if (strcmp(filename + namelen - 4, ".bmp"))
#ifdef HAVE_JPEG
        return read_jpeg_file(filename, bm, maxsize, format, cformat);
#else
        return 0;
#endif
    else
        return read_bmp_file(filename, bm, maxsize, format, cformat);
}

static bool incremental_albumart_cache(bool verbose)
{
    if (!aa_cache.buf)
        goto aa_failure;

    if (aa_cache.inspected >= pf_idx.album_ct)
        return false;

    /* Prevent idle poweroff */
    reset_poweroff_timer();

    int idx, ret;
    unsigned int hash_artist, hash_album;
    unsigned int format = FORMAT_NATIVE | FORMAT_DITHER;

    if (global_settings.album_covers_resize)
        format |= FORMAT_RESIZE|FORMAT_KEEP_ASPECT;

    idx = aa_cache.idx;
    if (idx >= pf_idx.album_ct || idx < 0) { idx = 0; } /* Rollover */


    aa_cache.idx++;
    aa_cache.inspected++;
    if (aa_cache.idx >= pf_idx.album_ct) { aa_cache.idx = 0; } /* Rollover */


    hash_artist = mfnv(get_album_artist(idx));
    hash_album = mfnv(get_album_name(idx));

    snprintf(aa_cache.pfraw_file, sizeof(aa_cache.pfraw_file),
                 CACHE_PREFIX "/%x%x.pfraw", hash_album, hash_artist);

    if(pf_cfg.update_albumart && file_exists(aa_cache.pfraw_file)) {
        aa_cache.slides++;
        goto aa_success;
    }

    if (!get_albumart_for_index_from_db(idx, aa_cache.file, sizeof(aa_cache.file)))
        goto aa_failure;


    aa_cache.input_bmp.data = aa_cache.buf;
    aa_cache.input_bmp.width = DISPLAY_WIDTH;
    aa_cache.input_bmp.height = DISPLAY_HEIGHT;

    ret = pf_read_image_file(aa_cache.file, &aa_cache.input_bmp,
                          aa_cache.buf_sz, format, &format_transposed);
    if (ret <= 0) {
        if (verbose) {
            splashf(HZ, "Album art is bad: %s", get_album_name(idx));
        }

        goto aa_failure;
    }
    remove(aa_cache.pfraw_file);

    if (!save_pfraw(aa_cache.pfraw_file, &aa_cache.input_bmp))
    {
        if (verbose) { splash(HZ, "Could not write bmp"); }
        goto aa_failure;
    }
    aa_cache.slides++;

aa_failure:
    if (verbose)
    {
        if (aa_cache.inspected >= pf_idx.album_ct)
            pf_config_save();
        return false;
    }

aa_success:
    if (aa_cache.inspected >= pf_idx.album_ct)
    {
        pf_config_save();
        free_all_slide_prio(0);
        if (pf_state == pf_idle)
            queue_post(&thread_q, EV_WAKEUP, 0);
    }

    if(verbose)/* direct interaction with user */
        return true;

    return false;
}

/**
 Precomupte the album art images and store them in CACHE_PREFIX.
 Use the "?" bitmap if image is not found.
 */
static bool create_albumart_cache(void)
{
    /* Phase 3 v2: generation moved to the background album-art cache; Cover
     * Flow no longer builds its own pfraw thumbnails. Do nothing. The code
     * below is intentionally left in place (unreached) so the generation
     * helpers it references stay referenced rather than triggering a large,
     * risky removal cascade -- it can be deleted outright later. */
    return true;

    splash_progress_set_delay(HZ / 2);
    draw_progressbar(0, pf_idx.album_ct, STR_STEP_PREPARING_ARTWORK);
    aa_cache.inspected = 0;
    for (int i=0; i < pf_idx.album_ct; i++)
    {
        incremental_albumart_cache(true);
        draw_progressbar(aa_cache.inspected, pf_idx.album_ct,
                          STR_STEP_PREPARING_ARTWORK);
        if (button_get(false) > BUTTON_NONE)
            return true;
    }
    if ( aa_cache.slides == 0 ) {
        /* Warn the user that we couldn't find any albumart */
        splash(2*HZ, ID2P(LANG_NO_ALBUMART_FOUND));
        return false;
    }
    return true;
}

/**
  Create the "?" slide, that is shown while loading
  or when no cover was found.
 */
static int create_empty_slide(bool force)
{
    if (!aa_cache.buf)
        return false;

    if ( force || ! file_exists( EMPTY_SLIDE ) )  {
        /* bm_no_album_cover is compiled in (apps/bitmaps/native/SOURCES),
         * not read from a file -- this fork targets a single fixed LCD
         * size, so DISPLAY_WIDTH/DISPLAY_HEIGHT are themselves build-time
         * constants, and center-cropping (never scaling) is enough to fit
         * whatever size the source .bmp happens to be. Target dimension is
         * DISPLAY_WIDTH x DISPLAY_WIDTH (square), not DISPLAY_WIDTH x
         * DISPLAY_HEIGHT: DISPLAY_WIDTH < DISPLAY_HEIGHT on this target
         * (128 x 160), and real album art -- square, 1:1 -- always ends up
         * constrained by the narrower dimension after
         * FORMAT_KEEP_ASPECT's resize, i.e. 128x128, with the leftover
         * vertical space reserved for the text caption. A square crop
         * matches that, rather than stretching the placeholder to fill
         * the taller box no real cover ever actually fills. If the source
         * is smaller than that in some dimension, that dimension is used
         * as-is (no letterboxing) -- render_slide()'s own vertical_offset
         * centering already handles a slide shorter than the box, same as
         * any real cover art would need. */
        int src_w = BMPWIDTH_no_album_cover;
        int src_h = BMPHEIGHT_no_album_cover;
        int dst_w = MIN(src_w, DISPLAY_WIDTH);
        int dst_h = MIN(src_h, DISPLAY_WIDTH);
        int x_off = (src_w - dst_w) / 2;
        int y_off = (src_h - dst_h) / 2;
        pix_t *dst = (pix_t*)aa_cache.buf;
        int y;

        for (y = 0; y < dst_h; y++)
        {
            memcpy(dst + y * dst_w,
                   no_album_cover + (y + y_off) * src_w + x_off,
                   dst_w * sizeof(pix_t));
        }

        aa_cache.input_bmp.width = dst_w;
        aa_cache.input_bmp.height = dst_h;
#if LCD_DEPTH > 1
        aa_cache.input_bmp.format = FORMAT_NATIVE;
#endif
        aa_cache.input_bmp.data = (char*)aa_cache.buf;

        if (!save_pfraw(EMPTY_SLIDE, &aa_cache.input_bmp))
            return false;
    }

    return true;
}

/**
 Thread used for loading and preparing bitmaps in the background
 */
static void thread(void)
{
    /* SSD mode: poll more frequently since disk access is cheap */
#ifdef HAVE_DISK_STORAGE
    long sleep_time = (global_settings.storage_mode == 2)
                      ? HZ : 5 * HZ;
#else
    long sleep_time = 5 * HZ;
#endif
    struct queue_event ev;
    while (1) {
        queue_wait_w_tmo(&thread_q, &ev, sleep_time);
        switch (ev.id) {
            case EV_EXIT:
                return;
            case EV_WAKEUP:
                /* we just woke up */
                break;
        }

        if(ev.id != SYS_TIMEOUT) {
            while ( queue_empty(&thread_q) ) {
                buf_ctx_lock();
                bool slide_loaded = load_new_slide();
                buf_ctx_unlock();
                if (!slide_loaded)
                    break;
                yield();
            }
        }
    }
}


/**
 End the thread by posting the EV_EXIT event
 */
static void end_pf_thread(void)
{
    if ( thread_is_running ) {
        queue_post(&thread_q, EV_EXIT, 0);
        thread_wait(thread_id);
        /* remove the thread's queue from the broadcast list */
        queue_delete(&thread_q);
        thread_is_running = false;
    }
}


/**
 Create the thread an setup the event queue
 */
static bool create_pf_thread(void)
{
    /* put the thread's queue in the bcast list */
    queue_init(&thread_q, true);
    if ((thread_id = create_thread(
                           thread,
                           thread_stack,
                           sizeof(thread_stack),
                            0,
                           "Picture load thread"
                               IF_PRIO(, PRIORITY_BUFFERING)
                               IF_COP(, CPU)
                                      )
        ) == 0) {
        return false;
    }
    thread_is_running = true;
    queue_post(&thread_q, EV_WAKEUP, 0);
    return true;
}


static void initialize_slide_cache(void)
{
    int i= 0;
    for (i = 0; i < SLIDE_CACHE_SIZE; i++) {
        pf_sldcache.cache[i].hid = 0;
        pf_sldcache.cache[i].index = 0;
        pf_sldcache.cache[i].next = i + 1;
        pf_sldcache.cache[i].prev = i - 1;
    }
    pf_sldcache.cache[0].prev = i - 1;
    pf_sldcache.cache[i - 1].next = 0;

    pf_sldcache.free = 0;
    pf_sldcache.used = -1;
    pf_sldcache.left_idx = -1;
    pf_sldcache.right_idx = -1;
    pf_sldcache.center_idx = -1;
}


/*
 * The following functions implement the linked-list-in-array used to manage
 * the LRU cache of slides, and the list of free cache slots.
 */

#define _SEEK_RIGHT_WHILE(start, cond) \
({ \
    int ind_, next_ = (start); \
    int i_ = 0; \
    do { \
        ind_ = next_; \
        next_ = pf_sldcache.cache[ind_].next; \
        i_++; \
    } while (next_ != pf_sldcache.used && (cond) && i_ < SLIDE_CACHE_SIZE); \
    if (i_ >= SLIDE_CACHE_SIZE) \
    /* TODO: Not supposed to happen */ \
        ind_ = -1; \
    ind_; \
})

#define _SEEK_LEFT_WHILE(start, cond) \
({ \
    int ind_, next_ = (start); \
    int i_ = 0; \
    do { \
        ind_ = next_; \
        next_ = pf_sldcache.cache[ind_].prev; \
        i_++; \
    } while (ind_ != pf_sldcache.used && (cond) && i_ < SLIDE_CACHE_SIZE); \
    if (i_ >= SLIDE_CACHE_SIZE) \
    /* TODO: Not supposed to happen */ \
        ind_ = -1; \
    ind_; \
})

/**
 Pop the given item from the linked list starting at *head, returning the next
 item, or -1 if the list is now empty.
*/
static inline int lla_pop_item (int *head, int i)
{
    int prev = pf_sldcache.cache[i].prev;
    int next = pf_sldcache.cache[i].next;
    if (i == next)
    {
        *head = -1;
        return -1;
    }
    else if (i == *head)
        *head = next;
    pf_sldcache.cache[next].prev = prev;
    pf_sldcache.cache[prev].next = next;
    return next;
}


/**
 Pop the head item from the list starting at *head, returning the index of the
 item, or -1 if the list is already empty.
*/
static inline int lla_pop_head (int *head)
{
    int i = *head;
    if (i != -1)
        lla_pop_item(head, i);
    return i;
}

/**
 Insert the item at index i before the one at index p.
*/
static inline void lla_insert (int i, int p)
{
    int next = p;
    int prev = pf_sldcache.cache[next].prev;
    pf_sldcache.cache[next].prev = i;
    pf_sldcache.cache[prev].next = i;
    pf_sldcache.cache[i].next = next;
    pf_sldcache.cache[i].prev = prev;
}


/**
 Insert the item at index i at the end of the list starting at *head.
*/
static inline void lla_insert_tail (int *head, int i)
{
    if (*head == -1)
    {
        *head = i;
        pf_sldcache.cache[i].next = i;
        pf_sldcache.cache[i].prev = i;
    } else
        lla_insert(i, *head);
}

/**
 Insert the item at index i before the one at index p.
*/
static inline void lla_insert_after(int i, int p)
{
    p = pf_sldcache.cache[p].next;
    lla_insert(i, p);
}


/**
 Insert the item at index i before the one at index p in the list starting at
 *head
*/
static inline void lla_insert_before(int *head, int i, int p)
{
    lla_insert(i, p);
    if (*head == p)
        *head = i;
}


/**
 Free the used slide at index i, and its buffer, and move it to the free
 slides list.
*/
static inline void free_slide(int i)
{
    if (pf_sldcache.cache[i].hid != empty_slide_hid)
        buflib_free(&buf_ctx, pf_sldcache.cache[i].hid);
    pf_sldcache.cache[i].index = -1;
    lla_pop_item(&pf_sldcache.used, i);
    lla_insert_tail(&pf_sldcache.free, i);
    if (pf_sldcache.used == -1)
    {
        pf_sldcache.right_idx = -1;
        pf_sldcache.left_idx = -1;
        pf_sldcache.center_idx = -1;
    }
}


/**
 Free one slide ranked above the given priority. If no such slide can be found,
 return false.
*/
static bool free_slide_prio(int prio)
{
    if (pf_sldcache.used == -1)
        return false;

    int i, prio_max;
    int l = pf_sldcache.used;
    int r = pf_sldcache.cache[pf_sldcache.used].prev;

    int prio_l = pf_sldcache.cache[l].index < center_index ?
           center_index - pf_sldcache.cache[l].index : 0;
    int prio_r = pf_sldcache.cache[r].index > center_index ?
           pf_sldcache.cache[r].index - center_index : 0;
    if (prio_l > prio_r)
    {
        i = l;
        prio_max = prio_l;
    } else {
        i = r;
        prio_max = prio_r;
    }
    if (prio_max > prio)
    {
        if (i == pf_sldcache.left_idx)
            pf_sldcache.left_idx = pf_sldcache.cache[i].next;
        if (i == pf_sldcache.right_idx)
            pf_sldcache.right_idx = pf_sldcache.cache[i].prev;
        free_slide(i);
        return true;
    } else
        return false;
}


/**
 Free all slides ranked above the given priority.
*/
static void free_all_slide_prio(int prio)
{
    while (free_slide_prio(prio))
    {;;}
}


/**
 Read the pfraw image given as filename and return the hid of the buffer
 */
static int read_pfraw(char* filename, int prio)
{
    struct pfraw_header bmph;
    int fh = open(filename, O_RDONLY);
    if( fh < 0 ) {
        /* pf_cfg.cache_version = CACHE_UPDATE; -- don't invalidate on missing pfraw */
        return empty_slide_hid;
    }

    /* A short read (truncated/corrupted cache file, e.g. from a power-off
     * mid-write) leaves bmph partially or fully uninitialized, and a
     * width/height larger than anything save_pfraw() could have written
     * (bounded by DISPLAY_WIDTH/DISPLAY_HEIGHT) means the file is corrupt
     * either way -- trusting either lets the size calculation below and
     * every later access via bm->width/height walk arbitrarily far past
     * this allocation. Treat both as "no cached image" rather than
     * propagating garbage dimensions. */
    if (read(fh, &bmph, sizeof(struct pfraw_header)) != sizeof(struct pfraw_header) ||
        bmph.width <= 0 || bmph.height <= 0 ||
        bmph.width > DISPLAY_WIDTH || bmph.height > DISPLAY_HEIGHT)
    {
        close(fh);
        return empty_slide_hid;
    }

    int size =  sizeof(struct dim) +
                sizeof( pix_t ) * bmph.width * bmph.height;

    int hid;
    do {
        hid = buflib_alloc(&buf_ctx, size);
    } while (hid < 0 && free_slide_prio(prio));

    if (hid < 0) {
        close( fh );
        return -1;
    }

    struct dim *bm = buflib_get_data(&buf_ctx, hid);

    bm->width = bmph.width;
    bm->height = bmph.height;
    pix_t *data = (pix_t*)(sizeof(struct dim) + (char *)bm);

    read( fh, data , sizeof( pix_t ) * bm->width * bm->height );
    close( fh );
    return hid;
}


/* Resolve the album's folder (directory of its first track) for a slide, so
 * its thumbnail can be looked up in the folder-keyed shared cache. Runs on the
 * picture-load thread, so the tagcache search here doesn't stall rendering. */
static bool get_slide_dir(const int slide_index, char *dir, int dirlen)
{
    struct tagcache_search tcs_l;
    char tcs_buf[TAGCACHE_BUFSZ];
    bool ret = false;

    if (!tagcache_search(&tcs_l, tag_filename))
        return false;

    tagcache_search_add_filter(&tcs_l, tag_album,
                               pf_idx.album_index[slide_index].seek);
    tagcache_search_add_filter(&tcs_l, tag_albumartist,
                               pf_idx.album_index[slide_index].artist_seek);

    if (tagcache_get_next(&tcs_l, tcs_buf, sizeof(tcs_buf)))
    {
        const char *sep = strrchr(tcs_l.result, '/');
        int len = sep ? (int)(sep - tcs_l.result) : 0;
        if (len >= dirlen)
            len = dirlen - 1;
        if (len > 0)
        {
            memcpy(dir, tcs_l.result, len);
            dir[len] = 0;
            ret = true;
        }
    }
    tagcache_search_finish(&tcs_l);
    return ret;
}

/* carousel_model.count for the album model. */
static int album_count(void)
{
    return pf_idx.album_ct;
}

/* Read a shared-cache thumbnail (.aat: struct albumart_cache_header followed by
 * row-major native pixels) into a buflib surface, transposing to the
 * column-major layout render_slide() expects. Returns a buflib handle,
 * empty_slide_hid on a missing/corrupt file, or -1 on allocation failure. */
static int read_aat_transposed(const char *filename, int prio)
{
    struct albumart_cache_header hdr;
    pix_t rowbuf[DISPLAY_WIDTH];
    int row, col, w, h, size, hid;
    int fh = open(filename, O_RDONLY);
    if (fh < 0)
        return empty_slide_hid;

    if (read(fh, &hdr, sizeof(hdr)) != sizeof(hdr) ||
        hdr.magic != ALBUMART_CACHE_MAGIC ||
        hdr.version != ALBUMART_CACHE_FORMAT_VERSION ||
        hdr.width == 0 || hdr.height == 0 ||
        hdr.width > DISPLAY_WIDTH || hdr.height > DISPLAY_HEIGHT)
    {
        close(fh);
        return empty_slide_hid;
    }

    w = hdr.width;
    h = hdr.height;
    size = sizeof(struct dim) + sizeof(pix_t) * w * h;

    do {
        hid = buflib_alloc(&buf_ctx, size);
    } while (hid < 0 && free_slide_prio(prio));

    if (hid < 0)
    {
        close(fh);
        return -1;
    }

    struct dim *bm = buflib_get_data(&buf_ctx, hid);
    bm->width = w;
    bm->height = h;
    pix_t *dst = (pix_t*)(sizeof(struct dim) + (char *)bm);

    for (row = 0; row < h; row++)
    {
        if (read(fh, rowbuf, sizeof(pix_t) * w) != (ssize_t)(sizeof(pix_t) * w))
        {
            close(fh);
            buflib_free(&buf_ctx, hid);
            return empty_slide_hid;
        }
        for (col = 0; col < w; col++)
            dst[col * h + row] = rowbuf[col];
    }
    close(fh);
    return hid;
}

/* carousel_model.legacy_art for the album model: this screen's own per-album
 * pfraw cache, keyed by a hash of the album+artist names. Always available. */
static bool album_legacy_art(int index, char *path, int len)
{
    snprintf(path, len, CACHE_PREFIX "/%x%x.pfraw",
             mfnv(get_album_name(index)), mfnv(get_album_artist(index)));
    return true;
}

/**
  Load the surface for the given slide_index into the cache at cache_index.
 */
static inline bool load_and_prepare_surface(const int slide_index,
                                            const int cache_index,
                                            const int prio)
{
    int hid = -1;
    bool got_shared = false;

    /* Prefer the shared, database-driven thumbnail cache (folder-keyed). */
    if (pf_cover_size_idx >= 0)
    {
        char dir[MAX_PATH];
        char aat_file[MAX_PATH];
        if (model->slide_art(slide_index, dir, sizeof(dir)) &&
            albumart_cache_lookup(dir, pf_cover_size_idx, aat_file,
                                  sizeof(aat_file), NULL))
        {
            hid = read_aat_transposed(aat_file, prio);
            if (hid < 0)
                return false; /* allocation failure: retry later */
            got_shared = (hid != empty_slide_hid);
        }
    }

    /* Fall back to this screen's own pfraw cache when a shared thumbnail
     * isn't available yet (e.g. background generation hasn't reached it),
     * so nothing regresses versus before the shared cache existed. A model
     * without a legacy cache (returns false) just shows the empty slide. */
    if (!got_shared)
    {
        char pfraw_file[MAX_PATH];
        if (model->legacy_art(slide_index, pfraw_file, sizeof(pfraw_file)))
        {
            hid = read_pfraw(pfraw_file, prio);
            if (hid < 0)
                return false; /* allocation failure: retry later */
        }
        else
            hid = empty_slide_hid;
    }

    pf_sldcache.cache[cache_index].hid = hid;
    if (cache_index < SLIDE_CACHE_SIZE)
        pf_sldcache.cache[cache_index].index = slide_index;

    return true;
}


/**
 Load the "next" slide that we can load, freeing old slides if needed, provided
 that they are further from center_index than the current slide
*/
bool load_new_slide(void)
{
    if (wants_to_quit)
        return false;

    int i = -1;

    if (pf_sldcache.center_idx != -1)
    {
        int next, prev;
        if (pf_sldcache.cache[pf_sldcache.center_idx].index != center_index)
        {
            if (pf_sldcache.cache[pf_sldcache.center_idx].index < center_index)
            {
                pf_sldcache.center_idx = _SEEK_RIGHT_WHILE(pf_sldcache.center_idx,
                                pf_sldcache.cache[next_].index <= center_index);
                if (pf_sldcache.center_idx == -1)
                    goto fatal_fail;

                prev = pf_sldcache.center_idx;
                next = pf_sldcache.cache[pf_sldcache.center_idx].next;
            }
            else
            {
                pf_sldcache.center_idx = _SEEK_LEFT_WHILE(pf_sldcache.center_idx,
                                pf_sldcache.cache[next_].index >= center_index);
                if (pf_sldcache.center_idx == -1)
                    goto fatal_fail;

                next = pf_sldcache.center_idx;
                prev = pf_sldcache.cache[pf_sldcache.center_idx].prev;
            }
            if (pf_sldcache.cache[pf_sldcache.center_idx].index != center_index)
            {
                if (pf_sldcache.free == -1)
                    free_slide_prio(0);

                i = lla_pop_head(&pf_sldcache.free);
                if (!load_and_prepare_surface(center_index, i, 0))
                    goto fail_and_refree;

                if (pf_sldcache.cache[next].index == -1)
                {
                    if (pf_sldcache.cache[prev].index == -1)
                        goto insert_first_slide;
                    else
                        next = pf_sldcache.cache[prev].next;
                }
                lla_insert(i, next);
                if (pf_sldcache.cache[i].index < pf_sldcache.cache[pf_sldcache.used].index)
                    pf_sldcache.used = i;

                pf_sldcache.center_idx = i;
                pf_sldcache.left_idx = i;
                pf_sldcache.right_idx = i;
                return true;
            }
        }
        int left, center, right;
        left = pf_sldcache.cache[pf_sldcache.left_idx].index;
        center = pf_sldcache.cache[pf_sldcache.center_idx].index;
        right = pf_sldcache.cache[pf_sldcache.right_idx].index;

        if (left > center)
            pf_sldcache.left_idx = pf_sldcache.center_idx;
        if (right < center)
            pf_sldcache.right_idx = pf_sldcache.center_idx;

        pf_sldcache.left_idx = _SEEK_LEFT_WHILE(pf_sldcache.left_idx,
            pf_sldcache.cache[ind_].index - 1 == pf_sldcache.cache[next_].index);

        pf_sldcache.right_idx = _SEEK_RIGHT_WHILE(pf_sldcache.right_idx,
            pf_sldcache.cache[ind_].index + 1 == pf_sldcache.cache[next_].index);
        if (pf_sldcache.right_idx == -1 || pf_sldcache.left_idx == -1)
            goto fatal_fail;


        /* update indices */
        left = pf_sldcache.cache[pf_sldcache.left_idx].index;
        center = pf_sldcache.cache[pf_sldcache.center_idx].index;
        right = pf_sldcache.cache[pf_sldcache.right_idx].index;

        int prio_l = center - left + 1;
        int prio_r = right - center + 1;
        if ((prio_l < prio_r
             || right >= number_of_slides - 1) && left > 0)
        {
            if (pf_sldcache.free == -1 && !free_slide_prio(prio_l))
            {
                return false;
            }

            i = lla_pop_head(&pf_sldcache.free);
            if (load_and_prepare_surface(left - 1, i, prio_l))
            {
                lla_insert_before(&pf_sldcache.used, i, pf_sldcache.left_idx);
                pf_sldcache.left_idx = i;
                return true;
            }
        } else if(right < number_of_slides - 1)
        {
            if (pf_sldcache.free == -1 && !free_slide_prio(prio_r))
            {
                return false;
            }

            i = lla_pop_head(&pf_sldcache.free);
            if (load_and_prepare_surface(right + 1, i, prio_r))
            {
                lla_insert_after(i, pf_sldcache.right_idx);
                pf_sldcache.right_idx = i;
                return true;
            }
        }
    } else {
        i = lla_pop_head(&pf_sldcache.free);
        if (load_and_prepare_surface(center_index, i, 0))
        {
insert_first_slide:
            pf_sldcache.cache[i].next = i;
            pf_sldcache.cache[i].prev = i;
            pf_sldcache.center_idx = i;
            pf_sldcache.left_idx = i;
            pf_sldcache.right_idx = i;
            pf_sldcache.used = i;
            return true;
        }
    }
fail_and_refree:
    if (i != -1)
    {
        lla_insert_tail(&pf_sldcache.free, i);
    }
    return false;
fatal_fail:
    free_all_slide_prio(0);
    initialize_slide_cache();
    return false;
}


/**
  Get a slide from the buffer
 */
static inline struct dim *get_slide(const int hid)
{
    if (!hid)
        return NULL;

    struct dim *bmp;

    bmp = buflib_get_data(&buf_ctx, hid);

    return bmp;
}


/**
 Return the requested surface for the given slide.

 The cache is a linked-list-in-array, so locating a slide_index is an O(n)
 walk of the used list. render_slide() calls this for every visible slide,
 every frame, and a slide normally stays in the same cache slot across frames.
 slide->cached_slot memoizes the slot found last time: a slot's index is unique
 within the used list, so if cache[slot].index still equals slide_index (and the
 slot holds a real handle) it is guaranteed to be the same slot, and we can skip
 the scan. This check reproduces the scan's result exactly on a hit and simply
 falls back to the full scan on a miss (evicted/reused/not-yet-resolved slot),
 so no explicit invalidation is needed. The bitmap data is always fetched fresh
 via get_slide() because buflib may relocate it between frames.
*/
static inline struct dim *surface(struct slide_data *slide)
{
    const int slide_index = slide->slide_index;
    if (slide_index < 0)
        return 0;
    if (slide_index >= number_of_slides)
        return 0;

    int slot = slide->cached_slot;
    if ((unsigned)slot < SLIDE_CACHE_SIZE &&
        pf_sldcache.cache[slot].index == slide_index &&
        pf_sldcache.cache[slot].hid != 0)
        return get_slide(pf_sldcache.cache[slot].hid);

    int i;
    if ((i = pf_sldcache.used ) != -1)
    {
        int j = 0;
        do {
            if (pf_sldcache.cache[i].index == slide_index) {
                slide->cached_slot = i;
                return get_slide(pf_sldcache.cache[i].hid);
            }
            i = pf_sldcache.cache[i].next;
            j++;
        } while (i != pf_sldcache.used && j < SLIDE_CACHE_SIZE);
    }
    return get_slide(empty_slide_hid);
}

/**
 adjust slides so that they are in "steady state" position
 */
static void reset_slides(void)
{
    center_slide.angle = 0;
    center_slide.cx = 0;
    center_slide.cy = 0;
    center_slide.distance = 0;
    center_slide.slide_index = center_index;

    int i;
    for (i = 0; i < ALBUM_COVERS_NUM_SLIDES; i++) {
        struct slide_data *si = &left_slides[i];
        si->angle = itilt;
        si->cx = -(offsetX + auto_slide_spacing * i);
        si->cy = offsetY;
        si->slide_index = center_index - 1 - i;
        si->distance = 0;
    }

    for (i = 0; i < ALBUM_COVERS_NUM_SLIDES; i++) {
        struct slide_data *si = &right_slides[i];
        si->angle = -itilt;
        si->cx = offsetX + auto_slide_spacing * i;
        si->cy = offsetY;
        si->slide_index = center_index + 1 + i;
        si->distance = 0;
    }
}


/**
 Updates look-up table and other stuff necessary for the rendering.
 Call this when the viewport size or slide dimension is changed.
 *
 * To calculate the offset that will provide the proper margin, we use the same
 * projection used to render the slides. The solution for xc, the slide center,
 * is:
 *                         xp * (zo + xs * sin(r))
 * xc = xp - xs * cos(r) + ───────────────────────
 *                                    z
 * TODO: support moving the side slides toward or away from the camera
 */
static void recalc_offsets(void)
{
    PFreal xs = PFREAL_HALF - DISPLAY_WIDTH * PFREAL_HALF;
    PFreal zo;
    PFreal xp = (DISPLAY_WIDTH * PFREAL_HALF - PFREAL_HALF +
                global_settings.album_covers_center_margin * PFREAL_ONE) * global_settings.album_covers_zoom / 100
                - global_settings.album_covers_slide_tuck * PFREAL_ONE;
    PFreal cosr, sinr;

    itilt = (global_settings.album_covers_parallel_slides ? 55 : 70) * IANGLE_MAX / 360;
    cosr = fcos(-itilt);
    sinr = fsin(-itilt);
    zo = CAM_DIST_R * 100 / global_settings.album_covers_zoom - CAM_DIST_R +
        fmuln(MAXSLIDE_LEFT_R, sinr, PFREAL_SHIFT - 2, 0);
    offsetX = xp - fmul(xs, cosr) + fmuln(xp,
        zo + fmuln(xs, sinr, PFREAL_SHIFT - 2, 0), PFREAL_SHIFT - 2, 0)
        / CAM_DIST;
    offsetY = DISPLAY_WIDTH / 2 * (fsin(itilt) + PFREAL_ONE / 2);

    /* auto-compute side slide spacing for 3 visible slides per side.
     * We distribute 3 visible slides across 2 intervals from offsetX
     * to cx_last (the world-space cx where the last slide's far edge
     * reaches the screen border).
     */
    {
        PFreal cx_last;
        if (global_settings.album_covers_parallel_slides) {
            /* In parallel mode, each slide is rendered at cx=0 then
             * shifted by screen_cx = CAM_DIST*cx/(CAM_DIST_R+zo).
             * Find the canonical far-edge position (right bitmap edge
             * of a right slide rendered at cx=0), then invert the
             * shift to find the cx that reaches the screen border. */
            PFreal edge_r = fdiv(CAM_DIST * fmul(-xs, cosr),
                CAM_DIST_R + zo + fmul(-xs, sinr));
            PFreal target = -DISPLAY_LEFT_R - edge_r;
            /* Invert: cx = target * (CAM_DIST_R+zo) / CAM_DIST_R
             *            = target + target * zo / CAM_DIST_R */
            cx_last = target
                + fmuln(target, zo, PFREAL_SHIFT - 2, 0) / CAM_DIST;
        } else {
            cx_last = (-DISPLAY_LEFT_R) * 100 / global_settings.album_covers_zoom
                             + fmul(xs, cosr);
        }
        PFreal span = cx_last - offsetX;
        if (span < PFREAL_ONE)
            span = PFREAL_ONE;
        auto_slide_spacing = span / 2;
    }
}

/**
   Fade the given color toward the theme background color.
   a = 256 means fully opaque (original color), a = 0 means fully bg.
 */
static inline pix_t fade_color(pix_t c, unsigned a)
{
#if (LCD_PIXELFORMAT == RGB565SWAPPED)
    unsigned int result;
    c = swap16(c);
    a = (a + 2) & 0x1fc;
    unsigned int inv_a = 0x100 - a;
    result = (((c & 0xf81f) * a + pf_bg_rb * inv_a)) & 0xf81f00;
    result |= (((c & 0x7e0) * a + pf_bg_g * inv_a)) & 0x7e000;
    result >>= 8;
    return swap16(result);

#elif LCD_PIXELFORMAT == RGB565
    unsigned int result;
    a = (a + 2) & 0x1fc;
    unsigned int inv_a = 0x100 - a;
    result = (((c & 0xf81f) * a + pf_bg_rb * inv_a)) & 0xf81f00;
    result |= (((c & 0x7e0) * a + pf_bg_g * inv_a)) & 0x7e000;
    result >>= 8;
    return result;

#elif (LCD_PIXELFORMAT == RGB888 || LCD_PIXELFORMAT == XRGB8888)
    unsigned int pixel = FB_UNPACK_SCALAR_LCD(c);
    unsigned int bg = FB_UNPACK_SCALAR_LCD(pf_bg_color);
    unsigned int result;
    a = (a + 2) & 0x1fc;
    unsigned int inv_a = 0x100 - a;
    result  = (((pixel & 0xff00ff) * a + (bg & 0xff00ff) * inv_a)) & 0xff00ff00;
    result |= (((pixel & 0x00ff00) * a + (bg & 0x00ff00) * inv_a)) & 0x00ff0000;
    result >>= 8;
    return FB_SCALARPACK(result);

#else
    unsigned val = c;
    return MULUQ(val, a) >> 8;
#endif
}

/**
 * Render a single slide
 * Where xc is the slide's horizontal offset from center, xs is the horizontal
 * on the slide from its center, zo is the slide's depth offset from the plane
 * of the display, r is the angle at which the slide is tilted, and xp is the
 * point on the display corresponding to xs on the slide, the projection
 * formulas are:
 *
 *      z * (xc + xs * cos(r))
 * xp = ──────────────────────
 *       z + zo + xs * sin(r)
 *
 *      z * (xc - xp) - xp * zo
 * xs = ────────────────────────
 *      xp * sin(r) - z * cos(r)
 *
 * We use the xp projection once, to find the left edge of the slide on the
 * display. From there, we use the xs reverse projection to find the horizontal
 * offset from the slide center of each column on the screen, until we reach
 * the right edge of the slide, or the screen. The reverse projection can be
 * optimized by saving the numerator and denominator of the fraction, which can
 * then be incremented by (z + zo) and sin(r) respectively.
 */

/* Clear only the PictureFlow viewport area (not the status bar).
 * Uses screen->clear_viewport() which fills with bg_pattern,
 * unlike lcd_fillrect(DRMODE_SOLID) which fills with fg_pattern. */
static void pf_clear_display(void)
{
    screens[SCREEN_MAIN].clear_viewport();
}

static void render_slide(struct slide_data *slide, const int alpha)
{
    struct dim *bmp = surface(slide);
    if (!bmp) {
        return;
    }
    if (slide->angle > 255 || slide->angle < -255)
        return;
    pix_t *src = (pix_t*)(sizeof(struct dim) + (char *)bmp);

    const int sw = bmp->width;
    const int sh = bmp->height;
    const PFreal slide_left = -sw * PFREAL_HALF + PFREAL_HALF;
    const int w = LCD_WIDTH;

    PFreal cosr = fcos(slide->angle);
    PFreal sinr = fsin(slide->angle);
    PFreal zo = PFREAL_ONE * slide->distance + CAM_DIST_R * 100 / global_settings.album_covers_zoom
        - CAM_DIST_R - fmuln(MAXSLIDE_LEFT_R, fabs(sinr), PFREAL_SHIFT - 2, 0);

    /* For parallel rendering, project the slide as if cx=0 (canonical tilt),
     * then shift horizontally to the true screen position. */
    PFreal screen_cx = (global_settings.album_covers_parallel_slides && slide->angle != 0)
        ? fdiv(CAM_DIST * slide->cx, CAM_DIST_R + zo) : 0;
    PFreal render_cx = (screen_cx != 0) ? 0 : slide->cx;

    PFreal xs = slide_left, xsnum, xsnumi, xsden, xsdeni;
    PFreal xp = fdiv(CAM_DIST * (render_cx + fmul(xs, cosr)),
        (CAM_DIST_R + zo + fmul(xs,sinr)));

    xp += screen_cx;
    int xi = (fmax(DISPLAY_LEFT_R, xp) - DISPLAY_LEFT_R + PFREAL_ONE - 1)
        >> PFREAL_SHIFT;
    xp = DISPLAY_LEFT_R + xi * PFREAL_ONE;
    if (xi >= w) {
        return;
    }
    PFreal xp_local = xp - screen_cx;
    xsnum = CAM_DIST * (render_cx - xp_local)
        - fmuln(xp_local, zo, PFREAL_SHIFT - 2, 0);
    xsden = fmuln(xp_local, sinr, PFREAL_SHIFT - 2, 0) - CAM_DIST * cosr;
    xs = fdiv(xsnum, xsden);

    xsnumi = -CAM_DIST_R - zo;
    xsdeni = sinr;
    int x;
    int dy = PFREAL_ONE;
    const int half_height = pf_half_height;
    const int lower_half = pf_lower_half;
    const bool perspective = (zo != 0 || slide->angle != 0);
    /* sh (the decoded cover's own height, after FORMAT_KEEP_ASPECT scaling
     * into the DISPLAY_WIDTH x DISPLAY_HEIGHT box) is rarely equal to
     * pf_height -- e.g. square album art is width-limited by DISPLAY_WIDTH,
     * so sh ends up well short of DISPLAY_HEIGHT, let alone this theme's own
     * pf_height. vertical_offset centers whatever the real sh is within the
     * viewport: positive when the image is shorter than the viewport (equal
     * blank margin top and bottom), negative when it's taller (equal crop
     * top and bottom). Without this, the split was implicitly anchored to
     * the viewport's own half_height regardless of sh, which only matched
     * the original plugin's fixed full-screen numbers by coincidence (see
     * the removed pf_display_offs) -- any other sh left the image sitting
     * top-anchored, with a blank gap only at the bottom. */
    const int vertical_offset = ((half_height + lower_half) - sh) / 2;
    const int p_start_upper = (half_height - 1 - vertical_offset) * PFREAL_ONE;
    const int p_start_lower = (half_height - vertical_offset) * PFREAL_ONE;
    for (x = xi; x < w; x++) {
        int column = (unsigned)(xs - slide_left) >> PFREAL_SHIFT;
        if (column >= sw)
            break;
        if (perspective) {
            dy = (CAM_DIST_R + zo + fmul(xs, sinr)) / CAM_DIST;
        }

        const pix_t *ptr = &src[column * sh];

#if LCD_STRIDEFORMAT == VERTICAL_STRIDE
#define PIXELSTEP_Y   1
#define LCDADDR(x, y) (&buffer[BUFFER_HEIGHT*(x) + (y)])
#else
#define PIXELSTEP_Y   BUFFER_WIDTH
#define LCDADDR(x, y) (&buffer[(y)*BUFFER_WIDTH + (x)])
#endif

        int p = p_start_upper;
        int plim = MAX(0, p - (half_height-1) * dy);
        pix_t *pixel = LCDADDR(x, half_height-1 );

        if (alpha == 256) {
            while (p >= plim) {
                *pixel = ptr[((unsigned)p) >> PFREAL_SHIFT];
                p -= dy;
                pixel -= PIXELSTEP_Y;
            }
        } else {
            while (p >= plim) {
                *pixel = fade_color(ptr[((unsigned)p) >> PFREAL_SHIFT], alpha);
                p -= dy;
                pixel -= PIXELSTEP_Y;
            }
        }
        p = p_start_lower;
        plim = MIN(sh * PFREAL_ONE, p + lower_half * dy);
        pixel = LCDADDR(x, half_height );

        if (alpha == 256) {
            while (p < plim) {
                *pixel = ptr[((unsigned)p) >> PFREAL_SHIFT];
                p += dy;
                pixel += PIXELSTEP_Y;
            }
        } else {
            while (p < plim) {
                *pixel = fade_color(ptr[((unsigned)p) >> PFREAL_SHIFT], alpha);
                p += dy;
                pixel += PIXELSTEP_Y;
            }
        }

        if (perspective)
        {
            xsnum += xsnumi;
            xsden += xsdeni;
            xs = fdiv(xsnum, xsden);
        } else
            xs += PFREAL_ONE;

    }
    /* let the music play... */
    yield();
    return;
}

void set_current_slide(const int slide_index)
{
    int old_center_index = center_index;
    step = 0;
    center_index = fbound(0, slide_index, number_of_slides - 1);
    if (old_center_index != center_index)
    {
        queue_remove_from_head(&thread_q, EV_WAKEUP);
        queue_post(&thread_q, EV_WAKEUP, 0);
    }
    target = center_index;
    slide_frame = center_index << 16;
    reset_slides();
}

static void return_to_idle_state(void)
{
    if (pf_state == pf_scrolling)
        set_current_slide(target);
    pf_state = pf_idle;
}

/* Set right before jumping to an album's track list (PF_SELECT below),
 * consumed here the next time Album covers opens. Needed because Album
 * covers doesn't resume an existing session when the user backs out of
 * that track list -- root_menu.c's GO_TO_PREVIOUS handling re-enters via a
 * fresh album_covers(NULL) call, same as any other new entry, and
 * selected_file is NULL either way -- so without this, the branch below
 * would use its normal "audio_status() ? now-playing : last_album"
 * fresh-entry heuristic and land on whatever's currently playing instead
 * of the cover the user just came from. That heuristic makes sense when
 * actually entering fresh (from the main menu, a WPS shortcut, etc., where
 * seeing "the current album" first is reasonable) but not when coming
 * straight back from browsing this exact cover's own tracks. */
bool pf_resume_last_album = false;
/* The index to resume to, captured separately from pf_cfg.last_album:
 * init() calls pf_config_load() (reloading pf_cfg from its on-disk file)
 * before set_initial_slide() runs, which clobbers whatever was just
 * assigned to pf_cfg.last_album in the PF_SELECT handler below with
 * whatever index was last saved to disk -- silently resuming the wrong
 * album (observed as always landing back on the first one). This is a
 * purely transient, in-memory signal that must never round-trip through
 * the persisted config. */
int pf_resume_album_index;

static void set_initial_slide(const char* selected_file)
{
    if (pf_resume_last_album)
    {
        pf_resume_last_album = false;
        set_current_slide(pf_resume_album_index);
        return;
    }

    if (selected_file)
        set_current_slide(retrieve_id3(&id3, selected_file) ?
                            id3_get_index(&id3) :
                            pf_cfg.last_album);
    else
        set_current_slide(audio_status() ?
                            id3_get_index(audio_current_track()) :
                            pf_cfg.last_album);

}

static void reselect(unsigned int hash_album, unsigned int hash_artist)
{
    int i, album_idx, artist_idx;
    for (i = 0; i < pf_idx.album_ct; i++ )
    {
        album_idx = pf_idx.album_index[i].name_idx;
        artist_idx = pf_idx.album_index[i].artist_idx;

        if(hash_album == mfnv(pf_idx.album_names + album_idx) &&
           hash_artist == mfnv(pf_idx.artist_names + artist_idx))
        {
            set_current_slide(i);
            pf_cfg.last_album = i;
            return;
        }
    }
    set_initial_slide(NULL);
}

static bool sort_albums(int new_sorting, bool from_settings)
{
    unsigned int hash_album, hash_artist;
    static const char* sort_options[] = {
        ID2P(LANG_ARTIST_PLUS_NAME),
        ID2P(LANG_ARTIST_PLUS_YEAR),
        ID2P(LANG_ID3_YEAR),
        ID2P(LANG_NAME)
    };

    /* Only change sorting once artwork has been inspected */
    if (aa_cache.inspected < pf_idx.album_ct)
    {
#ifdef USEGSLIB
        if (!from_settings)
            grey_show(false);
#endif
        splash(HZ*2, str(LANG_WAIT_FOR_CACHE));
#ifdef USEGSLIB
        if (!from_settings)
            grey_show(true);
#endif
        return false;
    }

    return_to_idle_state();

    global_settings.album_covers_sort_albums_by = new_sorting;
    if (!from_settings)
    {
#ifdef USEGSLIB
        grey_show(false);
#if LCD_DEPTH > 1
        lcd_set_background(N_BRIGHT(0));
        lcd_set_foreground(N_BRIGHT(255));
#endif
        lcd_clear_display();
        lcd_update();
#endif
        splash(HZ, sort_options[global_settings.album_covers_sort_albums_by]);
    }

    hash_album = mfnv(get_album_name(center_index));
    hash_artist = mfnv(get_album_artist(center_index));

    end_pf_thread(); /* stop loading of covers  */

    qsort(pf_idx.album_index, pf_idx.album_ct,
                  sizeof(struct album_data), compare_albums);

    /* Empty cache and restart cover loading thread */
    buflib_init(&buf_ctx, (void *)pf_idx.buf, pf_idx.buf_sz);
    empty_slide_hid = read_pfraw(EMPTY_SLIDE, 0);
    initialize_slide_cache();
    create_pf_thread();

    reselect(hash_album, hash_artist); /* splash if not found */

#ifdef USEGSLIB
    if (!from_settings)
        grey_show(true);
#endif
    return true;
}

/* carousel_model.sort_next/sort_prev for the album model: cycle through the
 * album sort orders and re-sort. */
static void album_sort_next(void)
{
    sort_albums((global_settings.album_covers_sort_albums_by + 1)
                % SORT_VALUES_SIZE, false);
}

static void album_sort_prev(void)
{
    sort_albums((global_settings.album_covers_sort_albums_by + (SORT_VALUES_SIZE - 1))
                % SORT_VALUES_SIZE, false);
}

void album_covers_rebuild_cache(void)
{
    pf_cfg.update_albumart = false;
    pf_cfg.cache_version = CACHE_REBUILD;
    remove(EMPTY_SLIDE);
    pf_config_save();
}

void album_covers_update_cache(void)
{
    pf_cfg.update_albumart = true;
    pf_cfg.cache_version = CACHE_REBUILD;
    remove(EMPTY_SLIDE);
    pf_config_save();
}

/**
  Start the animation for changing slides
 */
static void start_animation(void)
{
    step = (target < center_slide.slide_index) ? -1 : 1;
    pf_state = pf_scrolling;
}

static void update_scroll_animation(void);

static void show_previous_slide(void)
{
    if (step == 0) {
        if (center_index > 0) {
            target = center_index - 1;
            start_animation();
        }
    } else if ( step > 0 ) {
        target = center_index;
        step = (target <= center_slide.slide_index) ? -1 : 1;
        if (step < 0)
            update_scroll_animation();
    } else {
        target = fmax(0, center_index - 2);
    }
}

static void show_next_slide(void)
{
    if (step == 0) {
        if (center_index < number_of_slides - 1) {
            target = center_index + 1;
            start_animation();
        }
    } else if ( step < 0 ) {
        target = center_index;
        step = (target < center_slide.slide_index) ? -1 : 1;
        if (step > 0)
            update_scroll_animation();
    } else {
        target = fmin(center_index + 2, number_of_slides - 1);
    }
}

static void render_all_slides(void)
{
    lcd_set_background(pf_bg_color);
    /* TODO: Optimizes this by e.g. invalidating rects */
    pf_clear_display();

    int nleft = ALBUM_COVERS_NUM_SLIDES;
    int nright = ALBUM_COVERS_NUM_SLIDES;

    int alpha;
    int index;
    if (step == 0) {
        /* no animation, boring plain rendering */
        for (index = nleft - 2; index >= 0; index--) {
            alpha = (index < nleft - 2) ? 256 : 128;
            alpha -= extra_fade;
            if (alpha > 0 )
                render_slide(&left_slides[index], alpha);
        }
        for (index = nright - 2; index >= 0; index--) {
            alpha = (index < nright - 2) ? 256 : 128;
            alpha -= extra_fade;
            if (alpha > 0 )
                render_slide(&right_slides[index], alpha);
        }
    } else {
        /* the first and last slide must fade in/fade out */

        /* Check if the transitioning slide will be re-rendered later for
         * z-order correction.  If so, skip its first render to avoid
         * drawing an entire slide that gets immediately overwritten. */
        bool skip_right_0 = false, skip_left_0 = false;
        if (step > 0) {
            PFreal cd = (center_slide.cx >= 0) ? center_slide.cx
                                                : -center_slide.cx;
            PFreal td = (right_slides[0].cx >= 0) ? right_slides[0].cx
                                                    : -right_slides[0].cx;
            skip_right_0 = (td < cd);
        } else if (step < 0) {
            PFreal cd = (center_slide.cx >= 0) ? center_slide.cx
                                                : -center_slide.cx;
            PFreal td = (left_slides[0].cx >= 0) ? left_slides[0].cx
                                                   : -left_slides[0].cx;
            skip_left_0 = (td < cd);
        }

        /* if step<0 and nleft==1, left_slides[0] is fading in  */
        alpha = ((step > 0) ? 0 : ((nleft == 1) ? 256 : 128)) - fade / 2;
        for (index = nleft - 1; index >= 0; index--) {
            if (index == 0 && skip_left_0) {
                alpha += 128;
                if (alpha > 256) alpha = 256;
                continue;
            }
            if (alpha > 0)
                render_slide(&left_slides[index], alpha);
            alpha += 128;
            if (alpha > 256) alpha = 256;
        }
        /* if step>0 and nright==1, right_slides[0] is fading in  */
        alpha = ((step > 0) ? ((nright == 1) ? 128 : 0) : -64) + fade / 2;
        for (index = nright - 1; index >= 0; index--) {
            if (index == 0 && skip_right_0) {
                alpha += 128;
                if (alpha > 256) alpha = 256;
                continue;
            }
            if (alpha > 0)
                render_slide(&right_slides[index], alpha);
            alpha += 128;
            if (alpha > 256) alpha = 256;
        }
    }
    alpha = 256;
    if (step != 0 && ALBUM_COVERS_NUM_SLIDES <= 2) /* fading out center slide */
        alpha = (step > 0) ? 256 - fade / 2 : 128 + fade / 2;
    render_slide(&center_slide, alpha);
    /* During animation, re-render the transitioning slide on top once
     * it is closer to screen center than the outgoing center slide.
     * This gives a smooth z-order transition instead of a sudden flip. */
    if (step > 0) {
        PFreal cd = (center_slide.cx >= 0) ? center_slide.cx : -center_slide.cx;
        PFreal td = (right_slides[0].cx >= 0) ? right_slides[0].cx
                                               : -right_slides[0].cx;
        if (td < cd)
            render_slide(&right_slides[0], 256);
    } else if (step < 0) {
        PFreal cd = (center_slide.cx >= 0) ? center_slide.cx : -center_slide.cx;
        PFreal td = (left_slides[0].cx >= 0) ? left_slides[0].cx
                                              : -left_slides[0].cx;
        if (td < cd)
            render_slide(&left_slides[0], 256);
    }
}

static void update_scroll_animation(void)
{
    if (step == 0)
        return;

    int speed = 16384;
    int i;

    /* deaccelerate when approaching the target */
    const int max = 2 * 65536;

    int fi = slide_frame;
    fi -= (target << 16);
    if (fi < 0)
        fi = -fi;
    fi = fmin(fi, max);

    int ia = IANGLE_MAX * (fi - max / 2) / (max * 2);
    int accel = 16384 * (PFREAL_ONE + fsin(ia)) / PFREAL_ONE;
    speed = 512 * global_settings.album_covers_transition_speed / 100
          + accel * global_settings.album_covers_scroll_speed / 100;

    /* This advances slide_frame by a fixed amount per loop iteration,
     * not per unit of wall-clock time (the original plugin's own
     * update_scroll_animation() worked exactly the same way, so this
     * isn't a regression) -- meaning the same logical animation plays
     * out in fewer, larger jumps whenever the loop itself runs slower,
     * which is exactly what happens now that playback stays active
     * and competes with this screen for CPU time (see
     * plugin_get_buffer()'s comment in init()). A true fix would make
     * this frame-rate independent (scale by measured elapsed ticks
     * rather than a flat per-iteration constant); this is the cheaper,
     * targeted version of that: a flat multiplier specifically while
     * something is actually competing for the CPU, rather than always.
     * The 3/2 factor is a guess, not a measurement -- there's no way
     * to test actual frame timing from here, so treat this as a
     * starting point to tune against how it actually feels on
     * hardware, not a calibrated value. */
    if (audio_status() & AUDIO_STATUS_PLAY)
        speed = speed * 3 / 2;

    slide_frame += speed * step;

    int index = slide_frame >> 16;
    int pos = slide_frame & 0xffff;
    int neg = 65536 - pos;
    int tick = (step < 0) ? neg : pos;
    PFreal ftick = (tick * PFREAL_ONE) >> 16;

    /* the leftmost and rightmost slide must fade away */
    fade = pos / 256;

    if (step < 0)
        index++;
    if (center_index != index) {
        center_index = index;
        queue_post(&thread_q, EV_WAKEUP, 0);
        slide_frame = index << 16;
        /* Recalculate pos/tick/ftick/fade for the snapped slide_frame.
         * Without this, stale pre-snap values cause a one-frame alpha
         * discontinuity (flash) at boundary crossings. */
        pos = (step < 0) ? 65535 : 0;
        neg = 65536 - pos;
        tick = (step < 0) ? neg : pos;
        ftick = (tick * PFREAL_ONE) >> 16;
        fade = pos / 256;
        center_slide.slide_index = center_index;
        for (i = 0; i < ALBUM_COVERS_NUM_SLIDES; i++)
            left_slides[i].slide_index = center_index - 1 - i;
        for (i = 0; i < ALBUM_COVERS_NUM_SLIDES; i++)
            right_slides[i].slide_index = center_index + 1 + i;
    }

    center_slide.angle = (step * tick * itilt) >> 16;
    center_slide.cx = -step * fmul(offsetX, ftick);
    center_slide.cy = fmul(offsetY, ftick);

    if (center_index == target) {
        reset_slides();
        pf_state = pf_idle;
        slide_frame = center_index << 16;
        step = 0;
        fade = 256;
        return;
    }

    for (i = 0; i < ALBUM_COVERS_NUM_SLIDES; i++) {
        struct slide_data *si = &left_slides[i];
        si->angle = itilt;
        si->cx =
            -(offsetX + auto_slide_spacing * i + step
                        * fmul(auto_slide_spacing, ftick));
        si->cy = offsetY;
    }

    for (i = 0; i < ALBUM_COVERS_NUM_SLIDES; i++) {
        struct slide_data *si = &right_slides[i];
        si->angle = -itilt;
        si->cx =
            offsetX + auto_slide_spacing * i - step
                      * fmul(auto_slide_spacing, ftick);
        si->cy = offsetY;
    }

    if (step > 0) {
        PFreal ftick = (neg * PFREAL_ONE) >> 16;
        right_slides[0].angle = -(neg * itilt) >> 16;
        right_slides[0].cx = fmul(offsetX, ftick);
        right_slides[0].cy = fmul(offsetY, ftick);
    } else {
        PFreal ftick = (pos * PFREAL_ONE) >> 16;
        left_slides[0].angle = (pos * itilt) >> 16;
        left_slides[0].cx = -fmul(offsetX, ftick);
        left_slides[0].cy = fmul(offsetY, ftick);
    }

    /* must change direction ? */
    if (target < index)
        if (step > 0)
            step = -1;
    if (target > index)
        if (step < 0)
            step = 1;
}

static void cleanup(void)
{
    wants_to_quit = true;
    if (buf_ctx_locked)
        buf_ctx_unlock();

#ifdef HAVE_ADJUSTABLE_CPU_FREQ
    cpu_boost(false);
#endif
    end_pf_thread();

    /* Turn on backlight timeout (revert to settings) -- inlined equivalent
     * of apps/plugins/lib/helper.c's backlight_use_settings(), which isn't
     * linkable from core code. */
    backlight_set_timeout(global_settings.backlight_timeout);
#if CONFIG_CHARGING
    backlight_set_timeout_plugged(global_settings.backlight_timeout_plugged);
#endif

    /* Nothing to free: pf_idx.buf points into plugin_get_buffer()'s static
     * pluginbuf[] (see init()), not a buflib handle. The bold album-name font
     * is the shared font_get_ui_bold() -- owned by settings.c, not unloaded
     * here. */
}

enum {
    PF_SORT_ALBUMS_BY,
    PF_GOTO_LAST_ALBUM,
    PF_GOTO_WPS,
    PF_REBUILD_CACHE,
    PF_UPDATE_CACHE,
    PF_MENU_QUIT,
};

static int main_menu(void)
{
    int selection = 0;
    int curr_album, old_val;

    lcd_set_foreground(pf_fg_color);

    MENUITEM_STRINGLIST(main_menu, "Album covers", NULL,
                        ID2P(LANG_SORT_ALBUMS_BY),
                        ID2P(LANG_GOTO_LAST_ALBUM),
                        ID2P(LANG_GOTO_WPS),
                        ID2P(LANG_REBUILD_CACHE),
                        ID2P(LANG_UPDATE_CACHE),
                        ID2P(LANG_MENU_QUIT));

    static const struct opt_items sort_options[] = {
        { ID2P(LANG_ARTIST_PLUS_NAME) },
        { ID2P(LANG_ARTIST_PLUS_YEAR) },
        { ID2P(LANG_ID3_YEAR) },
        { ID2P(LANG_NAME) }};

    while (1)  {
        switch (do_menu(&main_menu, &selection, NULL, false)) {
            case PF_SORT_ALBUMS_BY:
                old_val = global_settings.album_covers_sort_albums_by;
                set_option(str(LANG_SORT_ALBUMS_BY),
                      &global_settings.album_covers_sort_albums_by, RB_INT,
                      sort_options, 4, NULL);
                if (old_val != global_settings.album_covers_sort_albums_by &&
                    !sort_albums(global_settings.album_covers_sort_albums_by, true))
                    global_settings.album_covers_sort_albums_by = old_val;
                if (old_val == global_settings.album_covers_sort_albums_by)
                    break;
                settings_save();
                return 0;
            case PF_GOTO_LAST_ALBUM:
                curr_album = (pf_state == pf_scrolling) ? target : center_index;
                set_current_slide(pf_cfg.last_album);
                pf_cfg.last_album = curr_album;
                pf_state = pf_idle;
                return 0;
            case PF_GOTO_WPS:
                return -2;
            case PF_REBUILD_CACHE:
                if (!yesno_pop_confirm(ID2P(LANG_REBUILD_CACHE)))
                    break;
                album_covers_rebuild_cache();
                return -3; /* re-init */
            case PF_UPDATE_CACHE:
                if (!yesno_pop_confirm(ID2P(LANG_UPDATE_CACHE)))
                    break;
                album_covers_update_cache();
                return -3; /* re-init */
            case PF_MENU_QUIT:
                return -1;

            case MENU_ATTACHED_USB:
                return MENU_ATTACHED_USB;

            default:
                return 0;
        }
    }
}

/* Restored to (almost) exactly how the original plugin's draw_album_text()
 * worked: drawn directly inside pf_vp -- the same viewport the coverflow
 * itself uses, not a separate panel -- overlaid near the top or bottom of
 * that same area, using pf_fg_color (dynamic_colors_resolve()-derived, so
 * the now-playing dynamic colour fade applies here too, same as everywhere
 * else in the UI). Differences from the original: no text_crossfade
 * (removed, see B2) so this always uses center_index rather than tracking
 * a mid-scroll transition index; no show_statusbar variant (the status bar
 * is always theme-controlled now, never toggled off by this screen); the
 * album name uses pf_bold_font rather than always FONT_UI -- a genuinely
 * separate, actually-bold font file if the theme configured one (see
 * global_settings.bold_font_file's comment in settings.h), not faked. A
 * faux-bold (double-drawn, offset by a pixel) treatment was tried here
 * first and reverted -- didn't look right. */
static void draw_album_text(void)
{
    char album_and_year[MAX_PATH];
    char *albumtxt, *artisttxt;
    int album_idx = 0;
    int char_height;
    int albumtxt_x, albumtxt_y, artisttxt_x;
    bool show_artist;

    if (global_settings.album_covers_show_album_name == ALBUM_NAME_HIDE)
        return;

    show_artist = (global_settings.album_covers_show_album_name == ALBUM_AND_ARTIST_TOP
                || global_settings.album_covers_show_album_name == ALBUM_AND_ARTIST_BOTTOM);

    albumtxt = get_album_name_idx(center_index, &album_idx);
    if (global_settings.album_covers_show_year
        && pf_idx.album_index[center_index].year > 0)
    {
        snprintf(album_and_year, sizeof(album_and_year), "%s \xe2\x80\x93 %d",
                  albumtxt, pf_idx.album_index[center_index].year);
    }
    else
        snprintf(album_and_year, sizeof(album_and_year), "%s", albumtxt);

    lcd_set_foreground(pf_fg_color);

    static int prev_albumtxt_index = -1;
    static bool prev_show_year = false;
    bool album_changed = (center_index != prev_albumtxt_index
                         || global_settings.album_covers_show_year != prev_show_year);

    /* Switched to pf_bold_font (plain FONT_UI, a no-op, if no bold font is
     * configured -- see its declaration) *before* set_scroll_line() below:
     * that measures the string's pixel width against whatever font is
     * currently active, to center/scroll it, so it has to see the album
     * name's *actual* rendering font, not FONT_UI, or a bold font with a
     * different average glyph width would end up mis-centered. */
    lcd_setfont(pf_bold_font);
    if (album_changed)
    {
        set_scroll_line(album_and_year, PF_SCROLL_ALBUM);
        prev_albumtxt_index = center_index;
        prev_show_year = global_settings.album_covers_show_year;
    }

    char_height = screens[SCREEN_MAIN].getcharheight();
    switch (global_settings.album_covers_show_album_name)
    {
        case ALBUM_AND_ARTIST_TOP:
            albumtxt_y = 0;
            break;
        case ALBUM_NAME_BOTTOM:
        case ALBUM_AND_ARTIST_BOTTOM:
            albumtxt_y = pf_height - (char_height * 9 / 4);
            break;
        case ALBUM_NAME_TOP:
        default:
            albumtxt_y = char_height / 2;
            break;
    }

    albumtxt_x = get_scroll_line_offset(PF_SCROLL_ALBUM);

    if (show_artist)
    {
        if (pf_idx.album_index[center_index].seek != pf_idx.album_untagged_seek)
            lcd_putsxy(albumtxt_x, albumtxt_y, album_and_year);
        /* Restored before the artist line: render_all_slides()/the FPS
         * overlay/etc all assume pf_vp's font is the real UI font, and the
         * artist name itself is never bold. screens[SCREEN_MAIN].getuifont(),
         * not the FONT_UI constant -- see init()'s comment on pf_bold_font. */
        lcd_setfont(screens[SCREEN_MAIN].getuifont());

        artisttxt = get_album_artist(center_index);
        if (album_changed)
            set_scroll_line(artisttxt, PF_SCROLL_ARTIST);
        artisttxt_x = get_scroll_line_offset(PF_SCROLL_ARTIST);
        lcd_putsxy(artisttxt_x, albumtxt_y + char_height * 3 / 4, artisttxt);
    }
    else
    {
        lcd_putsxy(albumtxt_x, albumtxt_y, album_and_year);
        lcd_setfont(screens[SCREEN_MAIN].getuifont());
    }
}

static void error_wait(const char *message)
{
    splashf(0, "%s. Press any button to continue.", message);
    while (get_action(CONTEXT_STD, 1) == ACTION_NONE)
        yield();
    sleep(2 * HZ);
}

static bool init(void)
{
    int ret = SUCCESS;
    void *buf;
    size_t buf_size;

#ifdef HAVE_ADJUSTABLE_CPU_FREQ
    cpu_boost(true); /* revert in cleanup */
#endif

    wants_to_quit = false;

    /* must appear before config load */
    memset(&aa_cache, 0, sizeof(struct albumart_t));

    config_set_defaults(&pf_cfg); /* must appear before pf_config_load */
    pf_config_load();

    /* Layout is always below the theme's status bar -- the theme owns
     * showing/hiding it now, unlike the old plugin's show_statusbar toggle. */
    FOR_NB_SCREENS(i)
        viewportmanager_theme_enable(i, true, NULL);
    viewport_set_defaults(&pf_vp, SCREEN_MAIN);
    FOR_NB_SCREENS(i)
        viewportmanager_theme_undo(i, false);

    pf_vp.buffer = &pf_framebuffer;
    pf_vp.x = 0;
    pf_vp.width = LCD_WIDTH;

    pf_vp_y = pf_vp.y;
    pf_height = pf_vp.height;

    /* Reserve real, empty vertical space for draw_album_text()'s overlay,
     * rather than just biasing where the up/down split falls: since
     * pf_lower_half is *defined* as pf_height - pf_half_height, upper and
     * lower always summed to exactly pf_height no matter how the split was
     * biased (a "+8"-style offset, tried first, only shifts which rows go
     * above vs below center -- it never actually shrinks the total drawn
     * area, so the art always ran the full height of the viewport
     * regardless). Actually keeping clear of the text means genuinely
     * reducing the drawable height by a margin sized to match
     * draw_album_text()'s own geometry, and -- for a *top*-anchored
     * caption -- also shifting the render origin down so the reserved
     * strip is skipped rather than just repositioning within it.
     *
     * Margin sizes mirror draw_album_text()'s own Y math exactly:
     * TOP captions span up to 1.75 * char_height (name+artist) or
     * 1.5 * char_height (name only) from the top; BOTTOM captions start
     * at (pf_height - 2.25 * char_height) regardless of whether the artist
     * line is also shown (it's added below the album line, within the same
     * reserved strip, not beyond it). 2 * char_height / 2.25 * char_height
     * cover both variants of each with a little safety margin. */
    {
        int char_height = font_get(screens[SCREEN_MAIN].getuifont())->height;
        int text_margin;
        bool text_at_top;
        int draw_height;

        switch (global_settings.album_covers_show_album_name)
        {
            case ALBUM_NAME_HIDE:
                text_margin = 0;
                text_at_top = false;
                break;
            case ALBUM_NAME_TOP:
            case ALBUM_AND_ARTIST_TOP:
                text_margin = char_height * 2;
                text_at_top = true;
                break;
            case ALBUM_NAME_BOTTOM:
            case ALBUM_AND_ARTIST_BOTTOM:
            default:
                text_margin = char_height * 9 / 4;
                text_at_top = false;
                break;
        }

        draw_height = pf_height - text_margin;
        if (draw_height < 0)
            draw_height = 0;

        pf_half_height = draw_height / 2;
        pf_lower_half = draw_height - pf_half_height;
        pf_draw_y_shift = text_at_top ? text_margin : 0;
    }

    pf_update_dynamic_colors();

    /* plugin_get_buffer() -- exactly what the original plugin used in its
     * PF_PLAYBACK_CAPABLE branch (pictureflow.c's init(), guarded by
     * PLUGIN_BUFFER_SIZE > 0x10000: true for both of this fork's targets,
     * ipod6g at 3 MiB and ipodvideo at 512 KiB, per firmware/export/config/
     * *.h) -- NOT core_alloc_maximum()/plugin_get_audio_buffer(), which was
     * tried here first and reverted: those hand back the largest free block
     * of the *audio* buflib pool, which requires the audio system to free
     * its own buffer first, stopping playback (and, with it, the dynamic
     * colour scheme that depends on a current track).
     *
     * pluginbuf[] (apps/plugin.c) is a plain static array -- a fixed,
     * always-resident region completely separate from the audio buffer
     * pool, reserved for whichever plugin is currently loaded, or, when
     * none is (current_plugin_handle is NULL -- true here, since this is
     * core-linked code, never a loaded plugin), returned to the caller in
     * full. It costs nothing new: this RAM was already permanently
     * reserved for plugin use, sitting idle whenever no plugin is loaded;
     * Album covers is simply borrowing it back the same way the original
     * plugin did. Not a buflib handle, so nothing to free in cleanup(). */
    buf = plugin_get_buffer(&buf_size);
    if (!buf)
    {
        error_wait("Not enough memory");
        return false;
    }

    /* store buffer pointers and sizes */
    pf_idx.buf = buf;
    pf_idx.buf_sz = buf_size;

    lcd_setfont(screens[SCREEN_MAIN].getuifont());

    /* Album-name font: the shared bold UI font. settings.c loads it once and
     * owns its lifecycle (so we must not unload it), and it already falls back
     * to the real configured UI font when the theme has no bold font -- so this
     * can be used unconditionally. Using the shared id (not the FONT_UI
     * constant) also sidesteps FONT_UI's fallback slot-search, which could
     * otherwise resolve to some other theme font in a higher slot. */
    pf_bold_font = font_get_ui_bold();

    if (!dir_exists(CACHE_PREFIX))
    {
        if (mkdir(CACHE_PREFIX) < 0)
        {
            error_wait("Could not create directory " CACHE_PREFIX);
            return false;
        }
    }

    mutex_init(&buf_ctx_mutex);

    init_scroll_lines();

    ret = model->build_index();

    if (ret == ERROR_BUFFER_FULL)
    {
        error_wait("Not enough memory for album names");
        return false;
    }
    else if (ret == ERROR_NO_ALBUMS)
    {
        error_wait("No albums found. Please enable database");
        return false;
    }
    else if (ret == ERROR_USER_ABORT)
        return false;

    number_of_slides = model->count();

    /* Phase 3 v2: Cover Flow no longer generates its own thumbnails -- the
     * background album-art cache (albumart_cache.c) does. Mark inspection
     * complete so the idle-loop generator never runs and the navigation
     * "wait for cache" splashes don't appear; slides come from the shared
     * .aat cache (with the old pfraw / empty slide as fallback). */
    aa_cache.inspected = model->count();

    /* Reserve the album-art scratch buffer. Both models need it: create_empty_slide()
     * builds the placeholder into aa_cache.buf, and (album only) the pfraw
     * generator caches thumbnails there. */
    size_t aa_min = DISPLAY_WIDTH * DISPLAY_HEIGHT * sizeof(pix_t);
    size_t aa_bufsz = ALIGN_DOWN(MAX(aa_min * 3, pf_idx.buf_sz / 8),
                                 sizeof(long));
    if (aa_bufsz < aa_min)
    {
        error_wait("Not enough memory for album art cache");
        return false;
    }

    ALIGN_BUFFER(pf_idx.buf, pf_idx.buf_sz, sizeof(long));
    aa_cache.buf = (char*) pf_idx.buf;
    aa_cache.buf_sz = aa_bufsz;

    pf_idx.buf += aa_bufsz;
    pf_idx.buf_sz -= aa_bufsz;

    pf_cover_size_idx = albumart_cache_size_index("coverflow");

    buflib_init(&buf_ctx, (void *)pf_idx.buf, pf_idx.buf_sz);
    initialize_slide_cache();

    if (!create_empty_slide(model->has_pfraw_cache &&
                            pf_cfg.cache_version != CACHE_VERSION))
    {
        if (model->has_pfraw_cache)
        {
            pf_cfg.cache_version = CACHE_REBUILD;
            pf_config_save();
        }
        error_wait("Could not load the empty slide");
        return false;
    }

    /* Model-specific one-off setup after the slide cache is up (album covers
     * generates its pfraw fallback thumbnails here; artist portraits has none). */
    if (model->prepare)
        model->prepare();

    if ((empty_slide_hid = read_pfraw(EMPTY_SLIDE, 0)) < 0)
    {
        error_wait("Unable to load empty slide image");
        return false;
    }

    if (!create_pf_thread())
    {
        error_wait("Cannot create thread!");
        return false;
    }

    buffer = LCD_BUF;
    buffer += (pf_vp_y + pf_draw_y_shift) * BUFFER_WIDTH; /* offset below
        status bar, plus a TOP text caption's reserved margin, if any (see
        pf_draw_y_shift's assignment above) */

    pf_state = pf_idle;

    slide_frame = 0;
    step = 0;
    target = 0;
    fade = 256;

    recalc_offsets();
    reset_slides();

    lcd_set_drawmode(DRMODE_FG);

    return true;
}

static bool reinit(void)
{
    bool is_album = (model == &album_model);
    unsigned int hash_album = 0, hash_artist = 0;

    return_to_idle_state();

    /* reselect() restores the same album across the rebuild by name hash --
     * album-model-only (it reads the album index). Other models just restart
     * at the first slide. */
    if (is_album)
    {
        hash_album = mfnv(get_album_name(center_index));
        hash_artist = mfnv(get_album_artist(center_index));
    }

    cleanup();
    if (init())
    {
        if (is_album)
            reselect(hash_album, hash_artist); /* splash if not found */
        else
            set_current_slide(0);
        return true;
    }
    return false;
}

/* carousel_model.enter for the album model: drill into the selected album's
 * track list. Identifies the album by its own tagcache seek, not by name --
 * tagtree.c filters directly on this, so there's no name/sort matching involved
 * (the earlier, much more fragile design that this replaced). */
static int album_enter(int index)
{
    int album_idx = 0;
    char *album = get_album_name_idx(index, &album_idx);
    long album_seek = pf_idx.album_index[index].seek;

    pf_cfg.last_album = index;
    pf_resume_album_index = index;
    pf_resume_last_album = true;
    tagtree_enter_album_tracks_on_next_load(album_seek, album);
    return GO_TO_ALBUM_COVERS_TRACKS;
}

/* carousel_model.on_menu: the Album Covers in-screen settings menu. Returns a
 * GO_TO_* to exit the carousel, or a CAROUSEL_MENU_* sentinel to stay (the
 * engine loop restores the carousel's display for the sentinel cases). */
static int album_on_menu(void)
{
    int ret;

    FOR_NB_SCREENS(i)
        viewportmanager_theme_enable(i, true, NULL);
    ret = main_menu();
    FOR_NB_SCREENS(i)
        viewportmanager_theme_undo(i, false);

    if (ret == -3)
        return reinit() ? CAROUSEL_MENU_RELOADED : GO_TO_PREVIOUS;
    if (ret == -2)
        return GO_TO_WPS;
    if (ret == -1)
        return GO_TO_PREVIOUS;
    if (ret == MENU_ATTACHED_USB)
        return GO_TO_ROOT;
    return CAROUSEL_MENU_STAY;
}

/* carousel_model.idle: opportunistic pfraw thumbnail generation. Only does work
 * while the pfraw cache is still incomplete (aa_cache.inspected < album_ct);
 * normally inspected == album_ct after init, so this is a cheap no-op. */
static void album_idle(void)
{
    if (aa_cache.inspected < pf_idx.album_ct)
    {
        buf_ctx_lock();
        incremental_albumart_cache(false);
        buf_ctx_unlock();
    }
}

/* carousel_model.prepare: generate the pfraw fallback thumbnails after init,
 * when the on-disk cache version has changed. */
static void album_prepare(void)
{
    if ((pf_cfg.cache_version != CACHE_VERSION) && !create_albumart_cache())
    {
        pf_cfg.cache_version = CACHE_REBUILD;
        pf_config_save();
        error_wait("Could not create album art cache");
    }

    if (pf_cfg.cache_version != CACHE_VERSION)
    {
        pf_cfg.cache_version = CACHE_VERSION;
        pf_config_save();
    }
}

static int album_covers_loop(void)
{
    int ret;
    int button;

    while (true) {
        /* Get input first. The SBS renders during get_custom_action() and
         * writes into the framebuffer (including decorative viewports that
         * overlap our area). Inhibit the SBS's lcd_update() so it doesn't
         * push that content to the display -- our own lcd_update() after
         * rendering will push both the SBS status bar and our content
         * atomically, avoiding one-frame flicker of theme artifacts. */

        /* Idle-frame-rate backoff. This loop fully re-renders the coverflow
         * and flushes the shared framebuffer (status bar included) on every
         * wake-up, so individual frames can't be skipped -- the SBS stamps
         * overlapping viewports into our area and relies on our lcd_update()
         * to reach the LCD. What we can do is wake up less often when nothing
         * is changing, which avoids the expensive per-frame redraw (a full
         * viewport clear + recomposite of every slide) while the screen sits
         * idle. Only back off when it is genuinely static: not animating a
         * scroll, no caption text scrolling, cover cache fully built, no
         * dynamic-colour fade in progress, and playback stopped (while
         * playing, the status bar's time updates every second and a track
         * change can start a colour fade, both of which want the normal
         * rate). A button press interrupts the wait immediately, so
         * responsiveness is unaffected. */
        bool caption_scrolling = scroll_lines[PF_SCROLL_TRACK].step
                              || scroll_lines[PF_SCROLL_ALBUM].step
                              || scroll_lines[PF_SCROLL_ARTIST].step;
        bool quiescent = pf_state != pf_scrolling
                      && !caption_scrolling
                      && aa_cache.inspected >= pf_idx.album_ct
                      && !dynamic_colors_fading()
                      && !(audio_status() & AUDIO_STATUS_PLAY);
        int timeout = (pf_state == pf_scrolling) ? 0
                    : (quiescent ? HZ/2 : HZ/16);

        skin_render_inhibit_flush(true);
        button = get_custom_action(CONTEXT_PLUGIN, timeout, get_context_map);
        skin_render_inhibit_flush(false);

        /* SBS rendering in get_custom_action resets the viewport to default,
         * and also leaves the global draw mode at whatever the skin's own
         * text/menu elements last used (typically DRMODE_SOLID, an opaque
         * background block) -- restore both, or draw_album_text()'s
         * lcd_putsxy() below inherits that solid mode and paints an opaque
         * block behind the album/artist text instead of drawing
         * transparently over the coverflow's own background. */
        lcd_set_viewport(&pf_vp);
        lcd_set_drawmode(DRMODE_FG);

        pf_update_dynamic_colors();
        update_scroll_lines();

        if (pf_state == pf_scrolling)
            update_scroll_animation();
        render_all_slides();
        model->draw_text();

        if (model->idle)
            model->idle();

        /* Copy offscreen buffer to LCD and give time to other threads */
        lcd_update();
        yield();

        switch (button) {
        case PF_QUIT:
            return GO_TO_PREVIOUS;
        case PF_WPS:
            return GO_TO_WPS;
        case PF_BACK:
            /* Album covers is a first-class main-menu screen now, not just
             * a WPS browsing mode (the old plugin's assumption, which is
             * why this unconditionally went to the WPS) -- BACK should
             * return to whichever screen it was actually opened from, same
             * as everywhere else in the firmware. */
            return GO_TO_PREVIOUS;
        case PF_MENU:
            /* The in-screen menu is model-specific (album covers has its settings
             * menu; artist portraits has none -> on_menu is NULL). It returns a
             * GO_TO_* to exit, or a CAROUSEL_MENU_* sentinel to stay. */
            if (!model->on_menu)
                break;
            ret = model->on_menu();
            if (ret >= 0)
                return ret;   /* a GO_TO_* screen code */
            /* Handled in place -- restore the carousel's own status bar/viewport
             * after the menu overlay, and (on a rebuild) its colours. */
            sb_set_persistent_title(model->title, Icon_NOICON, SCREEN_MAIN);
            lcd_set_viewport(&pf_vp);
            if (ret == CAROUSEL_MENU_RELOADED)
            {
                lcd_set_background(pf_bg_color);
                lcd_set_foreground(pf_fg_color);
            }
            lcd_set_drawmode(DRMODE_FG);
            break;

        case PF_NEXT:
        case PF_NEXT_REPEAT:
            show_next_slide();
            break;

        case PF_PREV:
        case PF_PREV_REPEAT:
            show_previous_slide();
            break;

        case PF_SORTING_NEXT:
            model->sort_next();
            break;
        case PF_SORTING_PREV:
            model->sort_prev();
            break;
        case PF_JMP:
        {
            int new_idx = model->jump_next();
            if (new_idx != center_index)
            {
                pf_state = pf_idle;
                set_current_slide(new_idx);
            }
            break;
        }
        case PF_JMP_PREV:
        {
            int new_idx = model->jump_prev();
            if (new_idx != center_index)
            {
                pf_state = pf_idle;
                set_current_slide(new_idx);
            }
            break;
        }
        case PF_TRACKLIST:
        case PF_SELECT:
        {
            /* Settle the animation onto the slide the user is looking at, so
             * center_index is the one they picked, then let the model drill in. */
            if (pf_state == pf_scrolling)
                set_current_slide(target);
            return model->enter(center_index);
        }
        default:
            if (default_event_handler(button) == SYS_USB_CONNECTED)
                return GO_TO_ROOT;
            break;
        }
    }
}

/* Run the coverflow carousel over the given model. Both public entry points
 * (album_covers / artist_portraits) are thin wrappers that select the model. */
int carousel_run(const struct carousel_model *m, const char *selected_file)
{
    int ret;

    model = m;

    /* Self-managed (rather than left to root_menu.c's load_screen(), which
     * only wraps the main-menu dispatch path) since this is also reachable
     * directly from apps/gui/wps.c (the "coverflow" WPS select-action and
     * the custom STOP-opens-coverflow behavior), bypassing that dispatcher
     * entirely. This is what makes %cs report ACTIVITY_ALBUMCOVERS
     * correctly regardless of which entry path was used. */
    push_current_activity(ACTIVITY_ALBUMCOVERS);

    if (!check_database())
    {
        pop_current_activity();
        return GO_TO_PREVIOUS;
    }

    /* render_slide() writes pixels directly into lcd_fb, and pf_framebuffer
     * (assigned to pf_vp.buffer in init()) needs the real frame_buffer_t
     * contents -- both dropped silently during the plugin-to-core port,
     * leaving every direct pixel write going to a NULL pointer (lcd_fb's
     * BSS default) instead of the real framebuffer. Matches plugin_start()'s
     * original setup exactly. */
    {
        struct viewport *vp_main = lcd_set_viewport(NULL);
        lcd_fb = vp_main->buffer->fb_ptr;
        pf_framebuffer = *vp_main->buffer;
    }

    if (!init())
    {
        cleanup();
        pop_current_activity();
        return GO_TO_PREVIOUS;
    }

    /* Was previously only set after returning from the in-screen menu (see
     * the PF_MENU case in album_covers_loop()) -- meaning the status bar
     * showed whatever title the previous screen left behind for the entire
     * time between opening the carousel and the first MENU press. */
    sb_set_persistent_title(model->title, Icon_NOICON, SCREEN_MAIN);

    /* Jump to selected_file's album if one was passed (e.g. onplay.c's
     * "Album covers" context-menu item on a specific track), otherwise the
     * currently-playing track's album, otherwise wherever was last viewed --
     * matching the old plugin's plugin_start() behavior exactly. */
    model->set_initial(selected_file);

    ret = album_covers_loop();

    cleanup();

    if (ret == GO_TO_WPS || ret == GO_TO_PREVIOUS_MUSIC || ret == GO_TO_PLUGIN)
        pop_current_activity_without_refresh();
    else
        pop_current_activity();

    return ret;
}

int album_covers(const char *selected_file)
{
    return carousel_run(&album_model, selected_file);
}
