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

#include "version.h"
#include "gcc_extensions.h"
#include "storage.h"
#include "disk.h"
#include "file_internal.h"
#include "lcd.h"
#include "rtc.h"
#include "debug.h"
#include "led.h"
#include "../kernel-internal.h"
#include "button.h"
#include "core_keymap.h"
#include "tree.h"
#include "filetypes.h"
#include "panic.h"
#include "menu.h"
#include "usb.h"
#include "wifi.h"
#include "powermgmt.h"
#include "adc.h"
#include "i2c.h"
#ifndef DEBUG
#include "serial.h"
#endif
#include "audio.h"
#include "settings.h"
#include "backlight.h"
#include "status.h"
#include "debug_menu.h"
#include "font.h"
#include "language.h"
#include "bitmaps/rockpodlogo.h"
#include "wps.h"
#include "playlist.h"
#include "core_alloc.h"
#include "rolo.h"
#include "screens.h"
#include "usb_screen.h"
#include "power.h"
#include "talk.h"
#include "plugin.h"
#include "misc.h"
#include "dircache.h"
#include "tagcache.h"
#include "albumart_cache.h"
#include "tagtree.h"
#include "lang.h"
#include "string.h"
#include "splash.h"
#include "eeprom_settings.h"
#include "icon.h"
#include "viewport.h"
#include "skin_engine/skin_engine.h"
#include "statusbar-skinned.h"
#include "bootchart.h"
#include "logdiskf.h"
#include "bootdata.h"

#include "shortcuts.h"

#ifdef IPOD_ACCESSORY_PROTOCOL
#include "iap.h"
#endif

#include "audio_thread.h"
#include "playback.h"
#include "tdspeed.h"

#ifdef BUTTON_REC
    #define SETTINGS_RESET BUTTON_REC
#endif

#if (CONFIG_STORAGE & STORAGE_MMC)
#include "ata_mmc.h"
#endif

#ifdef HAVE_HARDWARE_CLICK
#include "piezo.h"
#endif

#define MAIN_NORETURN_ATTR NORETURN_ATTR



/*#define AUTOROCK*/ /* define this to check for "autostart.rock" on boot */

static void init(void);
/* main(), and various functions called by main() and init() may be
 * be INIT_ATTR. These functions must not be called after the final call
 * to root_menu() at the end of main()
 * see definition of INIT_ATTR in config.h */
int main(void) INIT_ATTR MAIN_NORETURN_ATTR;
int main(void)
{
    CHART(">init");
    init();
    CHART("<init");
    FOR_NB_SCREENS(i)
    {
        screens[i].clear_display();
        screens[i].update();
    }
    list_init();
    tree_init();
    /* Keep the order of this 3
     * Must be done before any code uses the multi-screen API */
#ifdef HAVE_USBSTACK
    /* All threads should be created and public queues registered by now */
    usb_start_monitoring();
#endif

#if !defined(DISABLE_ACTION_REMAP) && defined(CORE_KEYREMAP_FILE)
    if (file_exists(CORE_KEYREMAP_FILE))
    {
        int mapct = core_load_key_remap(CORE_KEYREMAP_FILE);
        if (mapct <= 0)
            splashf(HZ, "key remap failed: %d,  %s", mapct, CORE_KEYREMAP_FILE);
    }
#endif

    allocate_playback_log();
    if (!file_exists(ROCKBOX_DIR"/playername.txt"))
    {
        int fd = open(ROCKBOX_DIR"/playername.txt", O_CREAT|O_WRONLY|O_TRUNC, 0666);
        if(fd >= 0)
        {
            fdprintf(fd, "%s", MODEL_NAME);
            close(fd);
        }
    }

    global_status.last_volume_change = 0;
    /* no calls INIT_ATTR functions after this point anymore!
     * see definition of INIT_ATTR in config.h */
    CHART(">root_menu");
    root_menu();
}

/* The disk isn't ready at boot, rblogo is stored in bin and erased after boot */
int show_logo_boot( void ) INIT_ATTR;
int show_logo_boot( void )
{
    unsigned char version[32];
    lcd_clear_display();
    lcd_bmp(&bm_rockpodlogo, (LCD_WIDTH - BMPWIDTH_rockpodlogo) / 2,
                             (LCD_HEIGHT - BMPHEIGHT_rockpodlogo) / 2);
    lcd_update();
    return 0;
}

static int INIT_ATTR init_dircache(bool preinit)
{
    if (preinit)
        dircache_init(MAX(global_status.dircache_size, 0));

    if (!global_settings.dircache)
        return -1;

    int result = -1;

    if (!preinit)
    {
        result = dircache_enable();
        if (result != 0)
        {
            if (result > 0)
            {
                /* Print "Scanning disk..." to the display. */
                splash(0, str(LANG_SCANNING_DISK));
                dircache_wait();
                backlight_on();
                show_logo_boot();
            }

            struct dircache_info info;
            dircache_get_info(&info);
            global_status.dircache_size = info.size;
            status_save(true);
        }
        /* else don't wait or already enabled by load */
    }

    return result;
}

static void init_tagcache(void) INIT_ATTR;
static void init_tagcache(void)
{
    bool clear = false;
#if 0
    long talked_tick = 0;
#endif
    tagcache_init();
    albumart_cache_init();

    while (!tagcache_is_initialized())
    {
        int ret = tagcache_get_commit_step();

        if (ret > 0)
        {
#if 0 /* FIXME: Audio isn't even initialized yet! */
            /* hwcodec can't use voice here, as the database commit
             * uses the audio buffer. */
            if(global_settings.talk_menu
               && (talked_tick == 0
                   || TIME_AFTER(current_tick, talked_tick+7*HZ)))
            {
                talked_tick = current_tick;
                talk_id(LANG_TAGCACHE_INIT, false);
                talk_number(ret, true);
                talk_id(VOICE_OF, true);
                talk_number(tagcache_get_max_commit_step(), true);
            }
#endif
            if (lang_is_rtl())
            {
                splash_progress(ret, tagcache_get_max_commit_step(),
                               "[%d/%d] %s", ret, tagcache_get_max_commit_step(),
                               str(LANG_TAGCACHE_INIT));
            }
            else
            {
                splash_progress(ret, tagcache_get_max_commit_step(),
                                "%s [%d/%d]", str(LANG_TAGCACHE_INIT), ret,
                                tagcache_get_max_commit_step());
            }
            clear = true;
        }
        sleep(HZ/4);
    }
    tagtree_init();

    if (clear)
    {
        backlight_on();
        show_logo_boot();
    }
}


#include "errno.h"

static void init(void) INIT_ATTR;
static void init(void)
{
    int rc;
    bool mounted = false;

    system_init();
    core_allocator_init();
    kernel_init();



    /* early early early! */
    filesystem_init();

#ifdef HAVE_ADJUSTABLE_CPU_FREQ
    set_cpu_frequency(CPUFREQ_NORMAL);
    cpu_boost(true);
#endif

    i2c_init();

    power_init();

    enable_irq();
#if defined(CPU_ARM_CLASSIC)
    enable_fiq();
#endif
    /* current_tick should be ticking by now */
    CHART("ticking");

    unicode_init();
    lcd_init();
    FOR_NB_SCREENS(i)
        global_status.font_id[i] = FONT_SYSFIXED;
    font_init();

    settings_reset();

    CHART(">show_logo");
    show_logo_boot();
    CHART("<show_logo");
    lang_init(core_language_builtin, language_strings,
              LANG_LAST_INDEX_IN_ARRAY);

#ifdef DEBUG
    debug_init();
#else
#ifdef HAVE_SERIAL
    serial_setup();
#endif
#endif

#if CONFIG_RTC
    rtc_init();
#endif

    adc_init();

    usb_init();

    backlight_init();

    button_init();

    /* Don't initialize power management here if it could incorrectly
     * measure battery voltage, and it's not needed for charging. */
#if !defined(NEED_ATA_POWER_BATT_MEASURE) || \
    (CONFIG_CHARGING > CHARGING_MONITOR)
    powermgmt_init();
#endif

#ifdef HAVE_HARDWARE_CLICK
    piezo_init();
#endif

    /* Keep the order of this 3 (viewportmanager handles statusbars)
     * Must be done before any code uses the multi-screen API */
    CHART(">gui_syncstatusbar_init");
    gui_syncstatusbar_init(&statusbars);
    CHART("<gui_syncstatusbar_init");
    CHART(">sb_skin_init");
    sb_skin_init();
    CHART("<sb_skin_init");
    CHART(">gui_sync_wps_init");
    gui_sync_skin_init();
    CHART("<gui_sync_wps_init");
    CHART(">viewportmanager_init");
    viewportmanager_init();
    CHART("<viewportmanager_init");

    CHART(">storage_init");
    rc = storage_init();
    CHART("<storage_init");
    if(rc)
    {
        lcd_clear_display();
        lcd_putsf(0, 1, "ATA error: %d", rc);
        lcd_puts(0, 3, "Press button to debug");
        lcd_update();
        while(!(button_get(true) & BUTTON_REL)); /* DO NOT CHANGE TO ACTION SYSTEM */
        dbg_ports();
        panicf("ata: %d", rc);
    }

#if defined(NEED_ATA_POWER_BATT_MEASURE) && \
    (CONFIG_CHARGING <= CHARGING_MONITOR)
    /* After storage_init(), ATA power must be on, so battery voltage
     * can be measured. Initialize power management if it was delayed. */
    powermgmt_init();
#endif

#ifndef HAVE_USBSTACK
    usb_start_monitoring();
    while (usb_detect() == USB_INSERTED)
    {
        /* enter USB mode early, before trying to mount */
        if (button_get_w_tmo(HZ/10) == SYS_USB_CONNECTED)
#if (CONFIG_STORAGE & STORAGE_MMC)
            if (!mmc_touched() ||
                (mmc_remove_request() == SYS_HOTSWAP_EXTRACTED))
#endif
            {
                gui_usb_screen_run(true, button_get_data());
                mounted = true; /* mounting done @ end of USB mode */
            }
#ifdef HAVE_USB_POWER
        /* if there is no host or user requested no USB, skip this */
        if (usb_powered_only())
            break;
#endif
    }
#endif

    if (!mounted)
    {
        CHART(">disk_mount_all");
        rc = disk_mount_all();
        CHART("<disk_mount_all");
        if (rc<=0)
        {
            int line=0;
            lcd_clear_display();
            lcd_putsf(0, line++, "No partition found (%d).", rc);
#ifndef USB_NONE
            lcd_puts(0, line++, "Insert USB cable");
            lcd_puts(0, line++, "and fix it.");
#elif !defined(DEBUG) && !(CONFIG_STORAGE & STORAGE_RAMDISK)
            lcd_puts(0, line++, "Rebooting in 5s");
#endif
            lcd_puts(0, line++, rbversion);

#ifdef STORAGE_GET_INFO
            struct storage_info sinfo;
            storage_get_info(0, &sinfo);
#ifdef MAX_PHYS_SECTOR_SIZE
            lcd_putsf(0, line++, "id: '%s' s:%u*%u", sinfo.product, sinfo.sector_size, sinfo.phys_sector_mult);
#else
            lcd_putsf(0, line++, "id: '%s' s:%u", sinfo.product, sinfo.sector_size);
#endif
#endif
            struct partinfo pinfo;
            for (int i = 0 ; i < NUM_VOLUMES ; i++) {
                disk_partinfo(i, &pinfo);
                if (pinfo.type)
                    lcd_putsf(0, line++, "P%d T%02x S%llx",
                              i, pinfo.type, (unsigned long long)pinfo.size);
            }
            lcd_update();

#if defined(MAX_VIRT_SECTOR_SIZE) && defined(DEFAULT_VIRT_SECTOR_SIZE)
                disk_set_sector_multiplier(IF_MD(i,) DEFAULT_VIRT_SECTOR_SIZE/SECTOR_SIZE);
#endif

#ifndef USB_NONE
            usb_start_monitoring();
            while(button_get(true) != SYS_USB_CONNECTED) {};
            gui_usb_screen_run(true, button_get_data());
#elif !defined(DEBUG) && !(CONFIG_STORAGE & STORAGE_RAMDISK)
            sleep(HZ*5);
#endif

#if !defined(DEBUG) && !(CONFIG_STORAGE & STORAGE_RAMDISK)
            system_reboot();
#else
            rc = disk_mount_all();
            if (rc <= 0) {
                lcd_putsf(0, 4, "Error mounting: %08x", rc);
                lcd_update();
                sleep(HZ*5);
                system_reboot();
            }
#endif
        }
    }

    pcm_init();
    dsp_init();

    CHART(">settings_load");
    settings_load();
    CHART("<settings_load");

#if (CONFIG_KEYPAD == IPOD_4G_PAD)
    if (global_settings.clear_settings_on_hold &&
#ifdef SETTINGS_RESET
    /* Reset settings if holding the reset button. (Rec on Archos,
       A on Gigabeat) */
    ((button_status() & SETTINGS_RESET) == SETTINGS_RESET))
#else
    /* Reset settings if the hold button is turned on */
    (button_hold()))
#endif
    {
        splash(HZ*2, str(LANG_RESET_DONE_CLEAR));
        settings_reset();
    }
#endif
    CHART(">init_battery_tables");
    init_battery_tables();
    CHART("<init_battery_tables");
    CHART(">init_dircache(true)");
    rc = init_dircache(true);
    CHART("<init_dircache(true)");
    if (rc < 0)
        tagcache_remove_statefile();

    CHART(">settings_apply(true)");
    settings_apply(true);
    CHART("<settings_apply(true)");
    CHART(">init_dircache(false)");
    init_dircache(false);
    CHART("<init_dircache(false)");
    CHART(">init_tagcache");
    init_tagcache();
    CHART("<init_tagcache");

    playlist_init();
    tree_mem_init();
    filetype_init();

    shortcuts_init();

    CHART(">audio_init");
    audio_init();
    CHART("<audio_init");
    talk_announce_voice_invalid(); /* notify user w/ voice prompt if voice file invalid */


    /* runtime database has to be initialized after audio_init() */
    cpu_boost(false);

#if CONFIG_CHARGING
    car_adapter_mode_init();
#endif
#ifdef IPOD_ACCESSORY_PROTOCOL
    iap_setup(global_settings.serial_bitrate);
#endif
#ifdef HAVE_ACCESSORY_SUPPLY
    accessory_supply_set(global_settings.accessory_supply);
#endif
#ifdef HAVE_LINEOUT_POWEROFF
    lineout_set(global_settings.lineout_active);
#endif
    CHART("<settings_apply_skins");
    settings_apply_skins();
    CHART(">settings_apply_skins");
}

#ifdef CPU_PP
void cop_main(void) MAIN_NORETURN_ATTR;
void cop_main(void)
{
/* This is the entry point for the coprocessor
   Anyone not running an upgraded bootloader will never reach this point,
   so it should not be assumed that the coprocessor be usable even on
   platforms which support it.

   A kernel thread is initially setup on the coprocessor and immediately
   destroyed for purposes of continuity. The cop sits idle until at least
   one thread exists on it. */

#if NUM_CORES > 1
    system_init();
    kernel_init();
    /* This should never be reached */
#endif
    while(1) {
        sleep_core(COP);
    }
}
#endif /* CPU_PP */

