#include "configfile.h"

#include "mpegplayer.h"
#include "mpeg_settings.h"

struct mpeg_settings vv_settings;

#define THUMB_DELAY (75*HZ/100)

/* button definitions */
#if (CONFIG_KEYPAD == IRIVER_H100_PAD) || \
    (CONFIG_KEYPAD == IRIVER_H300_PAD)
#define MPEG_START_TIME_SELECT      BUTTON_ON
#define MPEG_START_TIME_LEFT        BUTTON_LEFT
#define MPEG_START_TIME_RIGHT       BUTTON_RIGHT
#define MPEG_START_TIME_UP          BUTTON_UP
#define MPEG_START_TIME_DOWN        BUTTON_DOWN
#define MPEG_START_TIME_EXIT        BUTTON_OFF

#elif (CONFIG_KEYPAD == IAUDIO_X5M5_PAD)
#define MPEG_START_TIME_SELECT      BUTTON_PLAY
#define MPEG_START_TIME_LEFT        BUTTON_LEFT
#define MPEG_START_TIME_RIGHT       BUTTON_RIGHT
#define MPEG_START_TIME_UP          BUTTON_UP
#define MPEG_START_TIME_DOWN        BUTTON_DOWN
#define MPEG_START_TIME_EXIT        BUTTON_POWER

#elif (CONFIG_KEYPAD == IPOD_4G_PAD) || \
      (CONFIG_KEYPAD == IPOD_3G_PAD) || \
      (CONFIG_KEYPAD == IPOD_1G2G_PAD)
#define MPEG_START_TIME_SELECT      BUTTON_SELECT
#define MPEG_START_TIME_LEFT        BUTTON_LEFT
#define MPEG_START_TIME_RIGHT       BUTTON_RIGHT
#define MPEG_START_TIME_UP          BUTTON_SCROLL_FWD
#define MPEG_START_TIME_DOWN        BUTTON_SCROLL_BACK
#define MPEG_START_TIME_EXIT        BUTTON_MENU

#elif CONFIG_KEYPAD == GIGABEAT_PAD
#define MPEG_START_TIME_SELECT      BUTTON_SELECT
#define MPEG_START_TIME_LEFT        BUTTON_LEFT
#define MPEG_START_TIME_RIGHT       BUTTON_RIGHT
#define MPEG_START_TIME_UP          BUTTON_UP
#define MPEG_START_TIME_DOWN        BUTTON_DOWN
#define MPEG_START_TIME_LEFT2       BUTTON_VOL_UP
#define MPEG_START_TIME_RIGHT2      BUTTON_VOL_DOWN
#define MPEG_START_TIME_EXIT        BUTTON_POWER

#define MPEG_START_TIME_RC_SELECT   (BUTTON_RC_PLAY | BUTTON_REL)
#define MPEG_START_TIME_RC_LEFT     BUTTON_RC_REW
#define MPEG_START_TIME_RC_RIGHT    BUTTON_RC_FF
#define MPEG_START_TIME_RC_UP       BUTTON_RC_VOL_UP
#define MPEG_START_TIME_RC_DOWN     BUTTON_RC_VOL_DOWN
#define MPEG_START_TIME_RC_EXIT     (BUTTON_RC_PLAY | BUTTON_REPEAT)

#elif CONFIG_KEYPAD == GIGABEAT_S_PAD
#define MPEG_START_TIME_SELECT      BUTTON_SELECT
#define MPEG_START_TIME_LEFT        BUTTON_LEFT
#define MPEG_START_TIME_RIGHT       BUTTON_RIGHT
#define MPEG_START_TIME_UP          BUTTON_UP
#define MPEG_START_TIME_DOWN        BUTTON_DOWN
#define MPEG_START_TIME_LEFT2       BUTTON_VOL_UP
#define MPEG_START_TIME_RIGHT2      BUTTON_VOL_DOWN
#define MPEG_START_TIME_EXIT        BUTTON_POWER

#define MPEG_START_TIME_RC_SELECT   (BUTTON_RC_PLAY | BUTTON_REL)
#define MPEG_START_TIME_RC_LEFT     BUTTON_RC_REW
#define MPEG_START_TIME_RC_RIGHT    BUTTON_RC_FF
#define MPEG_START_TIME_RC_UP       BUTTON_RC_VOL_UP
#define MPEG_START_TIME_RC_DOWN     BUTTON_RC_VOL_DOWN
#define MPEG_START_TIME_RC_EXIT     (BUTTON_RC_PLAY | BUTTON_REPEAT)

#elif CONFIG_KEYPAD == IRIVER_H10_PAD
#define MPEG_START_TIME_SELECT      BUTTON_PLAY
#define MPEG_START_TIME_LEFT        BUTTON_LEFT
#define MPEG_START_TIME_RIGHT       BUTTON_RIGHT
#define MPEG_START_TIME_UP          BUTTON_SCROLL_UP
#define MPEG_START_TIME_DOWN        BUTTON_SCROLL_DOWN
#define MPEG_START_TIME_EXIT        BUTTON_POWER

#elif (CONFIG_KEYPAD == SANSA_E200_PAD)
#define MPEG_START_TIME_SELECT      BUTTON_SELECT
#define MPEG_START_TIME_LEFT        BUTTON_LEFT
#define MPEG_START_TIME_RIGHT       BUTTON_RIGHT
#define MPEG_START_TIME_UP          BUTTON_UP
#define MPEG_START_TIME_DOWN        BUTTON_DOWN
#define MPEG_START_TIME_LEFT2       BUTTON_SCROLL_BACK
#define MPEG_START_TIME_RIGHT2      BUTTON_SCROLL_FWD
#define MPEG_START_TIME_EXIT        BUTTON_POWER

#elif (CONFIG_KEYPAD == SANSA_FUZE_PAD)
#define MPEG_START_TIME_SELECT      BUTTON_SELECT
#define MPEG_START_TIME_LEFT        BUTTON_LEFT
#define MPEG_START_TIME_RIGHT       BUTTON_RIGHT
#define MPEG_START_TIME_UP          BUTTON_UP
#define MPEG_START_TIME_DOWN        BUTTON_DOWN
#define MPEG_START_TIME_LEFT2       BUTTON_SCROLL_BACK
#define MPEG_START_TIME_RIGHT2      BUTTON_SCROLL_FWD
#define MPEG_START_TIME_EXIT        (BUTTON_HOME|BUTTON_REPEAT)

#elif (CONFIG_KEYPAD == SANSA_C200_PAD) || \
(CONFIG_KEYPAD == SANSA_CLIP_PAD) || \
(CONFIG_KEYPAD == SANSA_M200_PAD)
#define MPEG_START_TIME_SELECT      BUTTON_SELECT
#define MPEG_START_TIME_LEFT        BUTTON_LEFT
#define MPEG_START_TIME_RIGHT       BUTTON_RIGHT
#define MPEG_START_TIME_UP          BUTTON_UP
#define MPEG_START_TIME_DOWN        BUTTON_DOWN
#define MPEG_START_TIME_LEFT2       BUTTON_VOL_UP
#define MPEG_START_TIME_RIGHT2      BUTTON_VOL_DOWN
#define MPEG_START_TIME_EXIT        BUTTON_POWER

#elif CONFIG_KEYPAD == MROBE500_PAD
#define MPEG_START_TIME_SELECT      BUTTON_RC_HEART
#define MPEG_START_TIME_LEFT        BUTTON_LEFT
#define MPEG_START_TIME_RIGHT       BUTTON_RIGHT
#define MPEG_START_TIME_UP          BUTTON_RC_PLAY
#define MPEG_START_TIME_DOWN        BUTTON_RC_DOWN
#define MPEG_START_TIME_LEFT2       BUTTON_RC_VOL_UP
#define MPEG_START_TIME_RIGHT2      BUTTON_RC_VOL_DOWN
#define MPEG_START_TIME_EXIT        BUTTON_POWER

#elif CONFIG_KEYPAD == MROBE100_PAD
#define MPEG_START_TIME_SELECT      BUTTON_SELECT
#define MPEG_START_TIME_LEFT        BUTTON_LEFT
#define MPEG_START_TIME_RIGHT       BUTTON_RIGHT
#define MPEG_START_TIME_UP          BUTTON_UP
#define MPEG_START_TIME_DOWN        BUTTON_DOWN
#define MPEG_START_TIME_LEFT2       BUTTON_PLAY
#define MPEG_START_TIME_RIGHT2      BUTTON_MENU
#define MPEG_START_TIME_EXIT        BUTTON_POWER

#elif CONFIG_KEYPAD == IAUDIO_M3_PAD
#define MPEG_START_TIME_SELECT      BUTTON_RC_PLAY
#define MPEG_START_TIME_LEFT        BUTTON_RC_REW
#define MPEG_START_TIME_RIGHT       BUTTON_RC_FF
#define MPEG_START_TIME_UP          BUTTON_RC_VOL_UP
#define MPEG_START_TIME_DOWN        BUTTON_RC_VOL_DOWN
#define MPEG_START_TIME_EXIT        BUTTON_RC_REC

#elif CONFIG_KEYPAD == COWON_D2_PAD
#define MPEG_START_TIME_EXIT        BUTTON_POWER

#elif (CONFIG_KEYPAD == CREATIVE_ZENXFI3_PAD)
#define MPEG_START_TIME_SELECT      (BUTTON_PLAY|BUTTON_REL)
#define MPEG_START_TIME_LEFT        BUTTON_BACK
#define MPEG_START_TIME_RIGHT       BUTTON_MENU
#define MPEG_START_TIME_UP          BUTTON_UP
#define MPEG_START_TIME_DOWN        BUTTON_DOWN
#define MPEG_START_TIME_EXIT        (BUTTON_PLAY|BUTTON_REPEAT)

#elif CONFIG_KEYPAD == PHILIPS_HDD1630_PAD
#define MPEG_START_TIME_SELECT      BUTTON_SELECT
#define MPEG_START_TIME_LEFT        BUTTON_LEFT
#define MPEG_START_TIME_RIGHT       BUTTON_RIGHT
#define MPEG_START_TIME_UP          BUTTON_UP
#define MPEG_START_TIME_DOWN        BUTTON_DOWN
#define MPEG_START_TIME_LEFT2       BUTTON_VOL_UP
#define MPEG_START_TIME_RIGHT2      BUTTON_VOL_DOWN
#define MPEG_START_TIME_EXIT        BUTTON_POWER

#elif CONFIG_KEYPAD == PHILIPS_HDD6330_PAD
#define MPEG_START_TIME_SELECT      BUTTON_PLAY
#define MPEG_START_TIME_LEFT        BUTTON_LEFT
#define MPEG_START_TIME_RIGHT       BUTTON_RIGHT
#define MPEG_START_TIME_UP          BUTTON_UP
#define MPEG_START_TIME_DOWN        BUTTON_DOWN
#define MPEG_START_TIME_LEFT2       BUTTON_VOL_UP
#define MPEG_START_TIME_RIGHT2      BUTTON_VOL_DOWN
#define MPEG_START_TIME_EXIT        BUTTON_POWER

#elif CONFIG_KEYPAD == PHILIPS_SA9200_PAD
#define MPEG_START_TIME_SELECT      BUTTON_PLAY
#define MPEG_START_TIME_LEFT        BUTTON_PREV
#define MPEG_START_TIME_RIGHT       BUTTON_NEXT
#define MPEG_START_TIME_UP          BUTTON_UP
#define MPEG_START_TIME_DOWN        BUTTON_DOWN
#define MPEG_START_TIME_LEFT2       BUTTON_VOL_UP
#define MPEG_START_TIME_RIGHT2      BUTTON_VOL_DOWN
#define MPEG_START_TIME_EXIT        BUTTON_POWER

#elif CONFIG_KEYPAD == ONDAVX747_PAD
#define MPEG_START_TIME_EXIT        BUTTON_POWER

#elif CONFIG_KEYPAD == ONDAVX777_PAD
#define MPEG_START_TIME_EXIT        BUTTON_POWER

#elif (CONFIG_KEYPAD == SAMSUNG_YH820_PAD) || \
      (CONFIG_KEYPAD == SAMSUNG_YH92X_PAD)
#define MPEG_START_TIME_SELECT      BUTTON_PLAY
#define MPEG_START_TIME_LEFT        BUTTON_LEFT
#define MPEG_START_TIME_RIGHT       BUTTON_RIGHT
#define MPEG_START_TIME_UP          BUTTON_UP
#define MPEG_START_TIME_DOWN        BUTTON_DOWN
#define MPEG_START_TIME_EXIT        BUTTON_REW

#elif CONFIG_KEYPAD == PBELL_VIBE500_PAD
#define MPEG_START_TIME_SELECT      BUTTON_PLAY
#define MPEG_START_TIME_LEFT        BUTTON_PREV
#define MPEG_START_TIME_RIGHT       BUTTON_NEXT
#define MPEG_START_TIME_UP          BUTTON_UP
#define MPEG_START_TIME_DOWN        BUTTON_DOWN
#define MPEG_START_TIME_LEFT2       BUTTON_OK
#define MPEG_START_TIME_RIGHT2      BUTTON_CANCEL
#define MPEG_START_TIME_EXIT        BUTTON_REC

#elif CONFIG_KEYPAD == MPIO_HD200_PAD
#define MPEG_START_TIME_SELECT      BUTTON_FUNC
#define MPEG_START_TIME_LEFT        BUTTON_REW
#define MPEG_START_TIME_RIGHT       BUTTON_FF
#define MPEG_START_TIME_UP          BUTTON_VOL_UP
#define MPEG_START_TIME_DOWN        BUTTON_VOL_DOWN
#define MPEG_START_TIME_EXIT        BUTTON_REC

#elif CONFIG_KEYPAD == MPIO_HD300_PAD
#define MPEG_START_TIME_SELECT      BUTTON_ENTER
#define MPEG_START_TIME_LEFT        BUTTON_REW
#define MPEG_START_TIME_RIGHT       BUTTON_FF
#define MPEG_START_TIME_UP          BUTTON_UP
#define MPEG_START_TIME_DOWN        BUTTON_DOWN
#define MPEG_START_TIME_EXIT        BUTTON_REC

#elif CONFIG_KEYPAD == SANSA_FUZEPLUS_PAD
#define MPEG_START_TIME_SELECT      BUTTON_SELECT
#define MPEG_START_TIME_LEFT        BUTTON_LEFT
#define MPEG_START_TIME_RIGHT       BUTTON_RIGHT
#define MPEG_START_TIME_UP          BUTTON_UP
#define MPEG_START_TIME_DOWN        BUTTON_DOWN
#define MPEG_START_TIME_EXIT        BUTTON_POWER

#elif CONFIG_KEYPAD == SANSA_CONNECT_PAD
#define MPEG_START_TIME_SELECT      BUTTON_SELECT
#define MPEG_START_TIME_LEFT        BUTTON_LEFT
#define MPEG_START_TIME_RIGHT       BUTTON_RIGHT
#define MPEG_START_TIME_UP          BUTTON_UP
#define MPEG_START_TIME_DOWN        BUTTON_DOWN
#define MPEG_START_TIME_EXIT        BUTTON_POWER

#elif CONFIG_KEYPAD == SAMSUNG_YPR0_PAD
#define MPEG_START_TIME_SELECT      BUTTON_SELECT
#define MPEG_START_TIME_LEFT        BUTTON_LEFT
#define MPEG_START_TIME_RIGHT       BUTTON_RIGHT
#define MPEG_START_TIME_UP          BUTTON_UP
#define MPEG_START_TIME_DOWN        BUTTON_DOWN
#define MPEG_START_TIME_EXIT        BUTTON_BACK

#elif (CONFIG_KEYPAD == HM60X_PAD) || (CONFIG_KEYPAD == HM801_PAD)
#define MPEG_START_TIME_SELECT      BUTTON_SELECT
#define MPEG_START_TIME_LEFT        BUTTON_LEFT
#define MPEG_START_TIME_RIGHT       BUTTON_RIGHT
#define MPEG_START_TIME_UP          BUTTON_UP
#define MPEG_START_TIME_DOWN        BUTTON_DOWN
#define MPEG_START_TIME_EXIT        BUTTON_POWER

#elif CONFIG_KEYPAD == SONY_NWZ_PAD
#define MPEG_START_TIME_SELECT      BUTTON_PLAY
#define MPEG_START_TIME_LEFT        BUTTON_LEFT
#define MPEG_START_TIME_RIGHT       BUTTON_RIGHT
#define MPEG_START_TIME_UP          BUTTON_UP
#define MPEG_START_TIME_DOWN        BUTTON_DOWN
#define MPEG_START_TIME_EXIT        BUTTON_BACK

#elif CONFIG_KEYPAD == CREATIVE_ZEN_PAD
#define MPEG_START_TIME_SELECT      BUTTON_SELECT
#define MPEG_START_TIME_LEFT        BUTTON_LEFT
#define MPEG_START_TIME_RIGHT       BUTTON_RIGHT
#define MPEG_START_TIME_UP          BUTTON_UP
#define MPEG_START_TIME_DOWN        BUTTON_DOWN
#define MPEG_START_TIME_EXIT        BUTTON_BACK

#elif CONFIG_KEYPAD == DX50_PAD
#define MPEG_START_TIME_EXIT        BUTTON_POWER
#define MPEG_START_TIME_SELECT      BUTTON_PLAY
#define MPEG_START_TIME_LEFT        BUTTON_LEFT
#define MPEG_START_TIME_RIGHT       BUTTON_RIGHT
#define MPEG_START_TIME_UP          BUTTON_VOL_UP
#define MPEG_START_TIME_DOWN        BUTTON_VOL_DOWN

#elif CONFIG_KEYPAD == CREATIVE_ZENXFI2_PAD
#define MPEG_START_TIME_EXIT        BUTTON_POWER

#elif CONFIG_KEYPAD == AGPTEK_ROCKER_PAD
#define MPEG_START_TIME_SELECT      BUTTON_SELECT
#define MPEG_START_TIME_LEFT        BUTTON_LEFT
#define MPEG_START_TIME_RIGHT       BUTTON_RIGHT
#define MPEG_START_TIME_UP          BUTTON_UP
#define MPEG_START_TIME_DOWN        BUTTON_DOWN
#define MPEG_START_TIME_EXIT        BUTTON_POWER

#elif CONFIG_KEYPAD == XDUOO_X3_PAD
#define MPEG_START_TIME_SELECT      BUTTON_PLAY
#define MPEG_START_TIME_LEFT        BUTTON_PREV
#define MPEG_START_TIME_RIGHT       BUTTON_NEXT
#define MPEG_START_TIME_UP          BUTTON_HOME
#define MPEG_START_TIME_DOWN        BUTTON_OPTION
#define MPEG_START_TIME_LEFT2       BUTTON_VOL_UP
#define MPEG_START_TIME_RIGHT2      BUTTON_VOL_DOWN
#define MPEG_START_TIME_EXIT        BUTTON_POWER

#elif CONFIG_KEYPAD == XDUOO_X3II_PAD || CONFIG_KEYPAD == XDUOO_X20_PAD
#define MPEG_START_TIME_SELECT      BUTTON_PLAY
#define MPEG_START_TIME_LEFT        BUTTON_PREV
#define MPEG_START_TIME_RIGHT       BUTTON_NEXT
#define MPEG_START_TIME_UP          BUTTON_HOME
#define MPEG_START_TIME_DOWN        BUTTON_OPTION
#define MPEG_START_TIME_LEFT2       BUTTON_VOL_UP
#define MPEG_START_TIME_RIGHT2      BUTTON_VOL_DOWN
#define MPEG_START_TIME_EXIT        BUTTON_POWER

#elif CONFIG_KEYPAD == FIIO_M3K_LINUX_PAD
#define MPEG_START_TIME_SELECT      BUTTON_PLAY
#define MPEG_START_TIME_LEFT        BUTTON_PREV
#define MPEG_START_TIME_RIGHT       BUTTON_NEXT
#define MPEG_START_TIME_UP          BUTTON_HOME
#define MPEG_START_TIME_DOWN        BUTTON_OPTION
#define MPEG_START_TIME_LEFT2       BUTTON_VOL_UP
#define MPEG_START_TIME_RIGHT2      BUTTON_VOL_DOWN
#define MPEG_START_TIME_EXIT        BUTTON_POWER

#elif CONFIG_KEYPAD == IHIFI_770_PAD || CONFIG_KEYPAD == IHIFI_800_PAD
#define MPEG_START_TIME_SELECT      BUTTON_PLAY
#define MPEG_START_TIME_LEFT        BUTTON_HOME
#define MPEG_START_TIME_RIGHT       BUTTON_VOL_DOWN
#define MPEG_START_TIME_UP          BUTTON_PREV
#define MPEG_START_TIME_DOWN        BUTTON_NEXT
#define MPEG_START_TIME_LEFT2       (BUTTON_POWER + BUTTON_HOME)
#define MPEG_START_TIME_RIGHT2      (BUTTON_POWER + BUTTON_VOL_DOWN)
#define MPEG_START_TIME_EXIT        BUTTON_POWER

#elif CONFIG_KEYPAD == EROSQ_PAD
#define MPEG_START_TIME_SELECT      BUTTON_PLAY
#define MPEG_START_TIME_LEFT        BUTTON_SCROLL_BACK
#define MPEG_START_TIME_RIGHT       BUTTON_SCROLL_FWD
#define MPEG_START_TIME_UP          BUTTON_PREV
#define MPEG_START_TIME_DOWN        BUTTON_NEXT
#define MPEG_START_TIME_EXIT        BUTTON_POWER

#elif CONFIG_KEYPAD == FIIO_M3K_PAD
#define MPEG_START_TIME_SELECT      BUTTON_SELECT
#define MPEG_START_TIME_LEFT        BUTTON_LEFT
#define MPEG_START_TIME_RIGHT       BUTTON_RIGHT
#define MPEG_START_TIME_UP          BUTTON_UP
#define MPEG_START_TIME_DOWN        BUTTON_DOWN
#define MPEG_START_TIME_EXIT        BUTTON_POWER

#elif CONFIG_KEYPAD == MA_PAD
#define MPEG_START_TIME_SELECT      BUTTON_PLAY
#define MPEG_START_TIME_LEFT        BUTTON_LEFT
#define MPEG_START_TIME_RIGHT       BUTTON_RIGHT
#define MPEG_START_TIME_UP          BUTTON_UP
#define MPEG_START_TIME_DOWN        BUTTON_DOWN
#define MPEG_START_TIME_EXIT        BUTTON_BACK

#elif CONFIG_KEYPAD == SHANLING_Q1_PAD || CONFIG_KEYPAD == HIBY_R3PROII_PAD
#define MPEG_START_TIME_EXIT        BUTTON_POWER

#elif CONFIG_KEYPAD == RG_NANO_PAD
#define MPEG_START_TIME_SELECT      BUTTON_A
#define MPEG_START_TIME_LEFT        BUTTON_LEFT
#define MPEG_START_TIME_RIGHT       BUTTON_RIGHT
#define MPEG_START_TIME_LEFT2       BUTTON_L
#define MPEG_START_TIME_RIGHT2      BUTTON_R
#define MPEG_START_TIME_UP          BUTTON_UP
#define MPEG_START_TIME_DOWN        BUTTON_DOWN
#define MPEG_START_TIME_EXIT        BUTTON_START

#elif CONFIG_KEYPAD == CTRU_PAD
#define MPEG_START_TIME_SELECT      BUTTON_SELECT
#define MPEG_START_TIME_LEFT        BUTTON_LEFT
#define MPEG_START_TIME_RIGHT       BUTTON_RIGHT
#define MPEG_START_TIME_UP          BUTTON_UP
#define MPEG_START_TIME_DOWN        BUTTON_DOWN
#define MPEG_START_TIME_EXIT        BUTTON_BACK

#else
#error No keymap defined!
#endif

#ifdef HAVE_TOUCHSCREEN
#ifndef MPEG_START_TIME_SELECT
#define MPEG_START_TIME_SELECT      BUTTON_CENTER
#endif
#ifndef MPEG_START_TIME_LEFT
#define MPEG_START_TIME_LEFT        BUTTON_MIDLEFT
#endif
#ifndef MPEG_START_TIME_RIGHT
#define MPEG_START_TIME_RIGHT       BUTTON_MIDRIGHT
#endif
#ifndef MPEG_START_TIME_UP
#define MPEG_START_TIME_UP          BUTTON_TOPMIDDLE
#endif
#ifndef MPEG_START_TIME_DOWN
#define MPEG_START_TIME_DOWN        BUTTON_BOTTOMMIDDLE
#endif
#ifndef MPEG_START_TIME_LEFT2
#define MPEG_START_TIME_LEFT2       BUTTON_TOPRIGHT
#endif
#ifndef MPEG_START_TIME_RIGHT2
#define MPEG_START_TIME_RIGHT2      BUTTON_TOPLEFT
#endif
#ifndef MPEG_START_TIME_EXIT
#define MPEG_START_TIME_EXIT        BUTTON_TOPLEFT
#endif
#endif

static struct configdata config[] =
{
    {TYPE_INT, 0, 2, { .int_p = &vv_settings.showfps }, "Show FPS", NULL},
    {TYPE_INT, 0, 2, { .int_p = &vv_settings.limitfps }, "Limit FPS", NULL},
    {TYPE_INT, 0, 2, { .int_p = &vv_settings.skipframes }, "Skip frames", NULL},
    {TYPE_INT, 0, INT_MAX, { .int_p = &vv_settings.resume_count }, "Resume count",
     NULL},
    {TYPE_INT, 0, MPEG_RESUME_NUM_OPTIONS,
     { .int_p = &vv_settings.resume_options }, "Resume options", NULL},
#if MPEG_OPTION_DITHERING_ENABLED
    {TYPE_INT, 0, INT_MAX, { .int_p = &vv_settings.displayoptions },
     "Display options", NULL},
#endif
    {TYPE_INT, 0, 2, { .int_p = &vv_settings.tone_controls }, "Tone controls",
     NULL},
    {TYPE_INT, 0, 2, { .int_p = &vv_settings.channel_modes }, "Channel modes",
     NULL},
    {TYPE_INT, 0, 2, { .int_p = &vv_settings.crossfeed }, "Crossfeed", NULL},
    {TYPE_INT, 0, 2, { .int_p = &vv_settings.equalizer }, "Equalizer", NULL},
    {TYPE_INT, 0, 2, { .int_p = &vv_settings.dithering }, "Dithering", NULL},
    {TYPE_INT, 0, 2, { .int_p = &vv_settings.play_mode }, "Play mode", NULL},
#ifdef HAVE_BACKLIGHT_BRIGHTNESS
    {TYPE_INT, -1, INT_MAX, { .int_p = &vv_settings.backlight_brightness },
     "Backlight brightness", NULL},
#endif
};

static const struct opt_items noyes[2] = {
    { STR(LANG_SET_BOOL_NO) },
    { STR(LANG_SET_BOOL_YES) },
};

static const struct opt_items singleall[2] = {
    { STR(LANG_SINGLE) },
    { STR(LANG_ALL) },
};

static const struct opt_items globaloff[2] = {
    { STR(LANG_OFF) },
    { STR(LANG_USE_SOUND_SETTING) },
};

static void mpeg_settings(void);
static bool mpeg_set_option(const char* string,
                            void* variable,
                            enum optiontype type,
                            const struct opt_items* options,
                            int numoptions,
                            void (*function)(int))
{
    mpeg_sysevent_clear();

    /* This eats SYS_POWEROFF - :\ */
    bool usb = set_option(string, variable, type, options, numoptions,
                              function);

    if (usb)
        mpeg_sysevent_set();

    return usb;
}

#ifdef HAVE_BACKLIGHT_BRIGHTNESS /* Only used for this atm */
static bool mpeg_set_int(const char *string, const char *unit,
                         int voice_unit, const int *variable,
                         void (*function)(int), int step,
                         int min,
                         int max,
                         const char* (*formatter)(char*, size_t, int, const char*),
                         int32_t (*get_talk_id)(int, int))
{
    mpeg_sysevent_clear();

    bool usb = set_int_ex(string, unit, voice_unit, variable, function,
                           step, min, max, formatter, get_talk_id);

    if (usb)
        mpeg_sysevent_set();

    return usb;
}

static int32_t backlight_brightness_getlang(int value, int unit)
{
    if (value < 0)
        return LANG_USE_COMMON_SETTING;

    return TALK_ID(value + MIN_BRIGHTNESS_SETTING, unit);
}

void mpeg_backlight_update_brightness(int value)
{
    if (value >= 0)
    {
        value += MIN_BRIGHTNESS_SETTING;
        backlight_set_brightness(value);
    }
    else
    {
        backlight_set_brightness(global_settings.brightness);
    }
}

static void backlight_brightness_function(int value)
{
    mpeg_backlight_update_brightness(value);
}

static const char* backlight_brightness_formatter(char *buf, size_t length,
                                                  int value, const char *input)
{
    (void)input;

    if (value < 0)
        return str(LANG_USE_COMMON_SETTING);
    else
        snprintf(buf, length, "%d", value + MIN_BRIGHTNESS_SETTING);
    return buf;
}
#endif /* HAVE_BACKLIGHT_BRIGHTNESS */

/* Sync a particular audio setting to global or mpegplayer forced off */
static void sync_audio_setting(int setting, bool global)
{
    switch (setting)
    {
    case MPEG_AUDIO_TONE_CONTROLS:
    #ifdef AUDIOHW_HAVE_BASS
        sound_set(SOUND_BASS, (global || vv_settings.tone_controls)
            ? global_settings.bass
            : sound_default(SOUND_BASS));
    #endif
    #ifdef AUDIOHW_HAVE_TREBLE
        sound_set(SOUND_TREBLE, (global || vv_settings.tone_controls)
            ? global_settings.treble
            : sound_default(SOUND_TREBLE));
    #endif

    #ifdef AUDIOHW_HAVE_EQ
        for (int band = 0;; band++)
        {
            int setting = sound_enum_hw_eq_band_setting(band, AUDIOHW_EQ_GAIN);

            if (setting == -1)
                break;

            sound_set(setting, (global || vv_settings.tone_controls)
                    ? global_settings.hw_eq_bands[band].gain
                    : sound_default(setting));
        }
    #endif /* AUDIOHW_HAVE_EQ */
        break;

    case MPEG_AUDIO_CHANNEL_MODES:
        sound_set(SOUND_CHANNELS, (global || vv_settings.channel_modes)
                ? global_settings.channel_config
                : SOUND_CHAN_STEREO);
        break;

    case MPEG_AUDIO_CROSSFEED:
        dsp_set_crossfeed_type((global || vv_settings.crossfeed) ?
                                   global_settings.crossfeed :
                                   CROSSFEED_TYPE_NONE);
        break;

    case MPEG_AUDIO_EQUALIZER:
        dsp_eq_enable((global || vv_settings.equalizer) ?
                          global_settings.eq_enabled : false);
        break;

    case MPEG_AUDIO_DITHERING:
        dsp_dither_enable((global || vv_settings.dithering) ?
                              global_settings.dithering_enabled : false);
       break;
    }
}

/* Sync all audio vv_settings to global or mpegplayer forced off */
static void sync_audio_settings(bool global)
{
    static const int setting_index[] =
    {
        MPEG_AUDIO_TONE_CONTROLS,
        MPEG_AUDIO_CHANNEL_MODES,
        MPEG_AUDIO_CROSSFEED,
        MPEG_AUDIO_EQUALIZER,
        MPEG_AUDIO_DITHERING,
    };
    unsigned i;

    for (i = 0; i < ARRAYLEN(setting_index); i++)
    {
        sync_audio_setting(setting_index[i], global);
    }
}

static void show_loading(struct vo_rect *rc)
{
    int oldmode = lcd_get_drawmode();
    lcd_set_drawmode(DRMODE_SOLID | DRMODE_INVERSEVID);
    lcd_fillrect(rc->l-1, rc->t-1, rc->r - rc->l + 2, rc->b - rc->t + 2);
    lcd_set_drawmode(oldmode);
    splash(0, "Loading...");
}

static void draw_slider(uint32_t range, uint32_t pos, struct vo_rect *rc)
{
    #define SLIDER_WIDTH   (LCD_WIDTH-SLIDER_LMARGIN-SLIDER_RMARGIN)
    #define SLIDER_X       SLIDER_LMARGIN
    #define SLIDER_Y       (LCD_HEIGHT-SLIDER_HEIGHT-SLIDER_BMARGIN)
    #define SLIDER_HEIGHT  8
    #define SLIDER_TEXTMARGIN 1
    #define SLIDER_LMARGIN 1
    #define SLIDER_RMARGIN 1
    #define SLIDER_TMARGIN 1
    #define SLIDER_BMARGIN 1
    #define SCREEN_MARGIN  1

    struct hms hms;
    char str[32];
    int text_w, text_h, text_y;

    /* Put positition on left */
    ts_to_hms(pos, &hms);
    hms_format(str, sizeof(str), &hms);
    lcd_getstringsize(str, NULL, &text_h);
    text_y = SLIDER_Y - SLIDER_TEXTMARGIN - text_h;

    if (rc == NULL)
    {
        int oldmode = lcd_get_drawmode();
        lcd_set_drawmode(DRMODE_BG | DRMODE_INVERSEVID);
        lcd_fillrect(SLIDER_X, text_y, SLIDER_WIDTH,
                       LCD_HEIGHT - SLIDER_BMARGIN - text_y
                       - SLIDER_TMARGIN);
        lcd_set_drawmode(oldmode);

        lcd_putsxy(SLIDER_X, text_y, str);

        /* Put duration on right */
        ts_to_hms(range, &hms);
        hms_format(str, sizeof(str), &hms);
        lcd_getstringsize(str, &text_w, NULL);

        lcd_putsxy(SLIDER_X + SLIDER_WIDTH - text_w, text_y, str);

        /* Draw slider */
        lcd_drawrect(SLIDER_X, SLIDER_Y, SLIDER_WIDTH, SLIDER_HEIGHT);
        lcd_fillrect(SLIDER_X, SLIDER_Y,
                      muldiv_uint32(pos, SLIDER_WIDTH, range),
                      SLIDER_HEIGHT);

        /* Update screen */
        lcd_update_rect(SLIDER_X, text_y - SLIDER_TMARGIN, SLIDER_WIDTH,
                         LCD_HEIGHT - SLIDER_BMARGIN - text_y + SLIDER_TEXTMARGIN);
    }
    else
    {
        /* Just return slider rectangle */
        rc->l = SLIDER_X;
        rc->t = text_y - SLIDER_TMARGIN;
        rc->r = rc->l + SLIDER_WIDTH;
        rc->b = rc->t + LCD_HEIGHT - SLIDER_BMARGIN - text_y;
    }
}

static bool display_thumb_image(const struct vo_rect *rc)
{
    bool retval = true;
    unsigned ltgray = LCD_LIGHTGRAY;
    unsigned dkgray = LCD_DARKGRAY;

    int oldcolor = lcd_get_foreground();

    if (!stream_display_thumb(rc))
    {
        /* Display "No Frame" and erase any border */
        const char * const str = "No Frame";
        int x, y, w, h;

        lcd_getstringsize(str, &w, &h);
        x = (rc->r + rc->l - w) / 2;
        y = (rc->b + rc->t - h) / 2;
        lcd_putsxy(x, y, str);

        lcd_update_rect(x, y, w, h);

        ltgray = dkgray = lcd_get_background();
        retval = false;
    }

    /* Draw a raised border around the frame (or erase if no frame) */

    lcd_set_foreground(ltgray);

    lcd_hline(rc->l-1, rc->r-1, rc->t-1);
    lcd_vline(rc->l-1, rc->t, rc->b-1);

    lcd_set_foreground(dkgray);

    lcd_hline(rc->l-1, rc->r, rc->b);
    lcd_vline(rc->r, rc->t-1, rc->b);

    lcd_set_foreground(oldcolor);

    lcd_update_rect(rc->l-1, rc->t-1, rc->r - rc->l + 2, 1);
    lcd_update_rect(rc->l-1, rc->t, 1, rc->b - rc->t);
    lcd_update_rect(rc->l-1, rc->b, rc->r - rc->l + 2, 1);
    lcd_update_rect(rc->r, rc->t, 1, rc->b - rc->t);

    return retval;
}

/* Add an amount to the specified time - with saturation */
static uint32_t increment_time(uint32_t val, int32_t amount, uint32_t range)
{
    if (amount < 0)
    {
        uint32_t off = -amount;
        if (range > off && val >= off)
            val -= off;
        else
            val = 0;
    }
    else if (amount > 0)
    {
        uint32_t off = amount;
        if (range > off && val <= range - off)
            val += off;
        else
            val = range;
    }

    return val;
}

#if defined(HAVE_LCD_ENABLE) || defined(HAVE_LCD_SLEEP)
static void get_start_time_lcd_enable_hook(unsigned short id, void *param)
{
    (void)id;
    (void)param;
    button_queue_post(LCD_ENABLE_EVENT_0, 0);
}
#endif /* HAVE_LCD_ENABLE */

static int get_start_time(uint32_t duration)
{
    int button = 0;
    int tmo = TIMEOUT_NOBLOCK;
    uint32_t resume_time = vv_settings.resume_time;
    struct vo_rect rc_vid, rc_bound;
    uint32_t aspect_vid, aspect_bound;
    bool sliding = false;

    enum state_enum slider_state = STATE0;

    lcd_clear_display();
    lcd_update();

#if defined(HAVE_LCD_ENABLE) || defined(HAVE_LCD_SLEEP)
    add_event(LCD_EVENT_ACTIVATION, get_start_time_lcd_enable_hook);
#endif

    draw_slider(0, 100, &rc_bound);
    rc_bound.b = rc_bound.t - SLIDER_TMARGIN;
    rc_bound.t = SCREEN_MARGIN;

    DEBUGF("rc_bound: %d, %d, %d, %d\n", rc_bound.l, rc_bound.t,
           rc_bound.r, rc_bound.b);

    rc_vid.l = rc_vid.t = 0;
    if (!stream_vo_get_size((struct vo_ext *)&rc_vid.r))
    {
        /* Can't get size - fill whole thing */
        rc_vid.r = rc_bound.r - rc_bound.l;
        rc_vid.b = rc_bound.b - rc_bound.t;
    }

    /* Get aspect ratio of bounding rectangle and video in u16.16 */
    aspect_bound = ((rc_bound.r - rc_bound.l) << 16) /
                    (rc_bound.b - rc_bound.t);

    DEBUGF("aspect_bound: %u.%02u\n", (unsigned)(aspect_bound >> 16),
           (unsigned)(100*(aspect_bound & 0xffff) >> 16));

    aspect_vid = (rc_vid.r << 16) / rc_vid.b;

    DEBUGF("aspect_vid: %u.%02u\n", (unsigned)(aspect_vid >> 16),
           (unsigned)(100*(aspect_vid & 0xffff) >> 16));

    if (aspect_vid >= aspect_bound)
    {
        /* Video proportionally wider than or same as bounding rectangle */
        if (rc_vid.r > rc_bound.r - rc_bound.l)
        {
            rc_vid.r = rc_bound.r - rc_bound.l;
            rc_vid.b = (rc_vid.r << 16) / aspect_vid;
        }
        /* else already fits */
    }
    else
    {
        /* Video proportionally narrower than bounding rectangle */
        if (rc_vid.b > rc_bound.b - rc_bound.t)
        {
            rc_vid.b = rc_bound.b - rc_bound.t;
            rc_vid.r = (aspect_vid * rc_vid.b) >> 16;
        }
        /* else already fits */
    }

    /* Even width and height >= 2 */
    rc_vid.r = (rc_vid.r < 2) ? 2 : (rc_vid.r & ~1);
    rc_vid.b = (rc_vid.b < 2) ? 2 : (rc_vid.b & ~1);

    /* Center display in bounding rectangle */
    rc_vid.l = ((rc_bound.l + rc_bound.r) - rc_vid.r) / 2;
    rc_vid.r += rc_vid.l;

    rc_vid.t = ((rc_bound.t + rc_bound.b) - rc_vid.b) / 2;
    rc_vid.b += rc_vid.t;

    DEBUGF("rc_vid: %d, %d, %d, %d\n", rc_vid.l, rc_vid.t,
           rc_vid.r, rc_vid.b);

#ifndef HAVE_LCD_COLOR
    stream_gray_show(true);
#endif

    while (slider_state < STATE9)
    {
        button = mpeg_button_get(tmo);

        switch (button)
        {
        case BUTTON_NONE:
            break;

        /* Coarse (1 minute) control */
        case MPEG_START_TIME_DOWN:
        case MPEG_START_TIME_DOWN | BUTTON_REPEAT:
#ifdef MPEG_START_TIME_RC_DOWN
        case MPEG_START_TIME_RC_DOWN:
        case MPEG_START_TIME_RC_DOWN | BUTTON_REPEAT:
#endif
            resume_time = increment_time(resume_time, -60*TS_SECOND, duration);
            slider_state = STATE0;
            break;

        case MPEG_START_TIME_UP:
        case MPEG_START_TIME_UP | BUTTON_REPEAT:
#ifdef MPEG_START_TIME_RC_UP
        case MPEG_START_TIME_RC_UP:
        case MPEG_START_TIME_RC_UP | BUTTON_REPEAT:
#endif
            resume_time = increment_time(resume_time, 60*TS_SECOND, duration);
            slider_state = STATE0;
            break;

        /* Fine (1 second) control */
        case MPEG_START_TIME_LEFT:
        case MPEG_START_TIME_LEFT | BUTTON_REPEAT:
#ifdef MPEG_START_TIME_RC_LEFT
        case MPEG_START_TIME_RC_LEFT:
        case MPEG_START_TIME_RC_LEFT | BUTTON_REPEAT:
#endif
#ifdef MPEG_START_TIME_LEFT2
        case MPEG_START_TIME_LEFT2:
        case MPEG_START_TIME_LEFT2 | BUTTON_REPEAT:
#endif
            resume_time = increment_time(resume_time, -TS_SECOND, duration);
            slider_state = STATE0;
            break;

        case MPEG_START_TIME_RIGHT:
        case MPEG_START_TIME_RIGHT | BUTTON_REPEAT:
#ifdef MPEG_START_TIME_RC_RIGHT
        case MPEG_START_TIME_RC_RIGHT:
        case MPEG_START_TIME_RC_RIGHT | BUTTON_REPEAT:
#endif
#ifdef MPEG_START_TIME_RIGHT2
        case MPEG_START_TIME_RIGHT2:
        case MPEG_START_TIME_RIGHT2 | BUTTON_REPEAT:
#endif
            resume_time = increment_time(resume_time, TS_SECOND, duration);
            slider_state = STATE0;
            break;

        case MPEG_START_TIME_SELECT:
#ifdef MPEG_START_TIME_RC_SELECT
        case MPEG_START_TIME_RC_SELECT:
#endif
            vv_settings.resume_time = resume_time;
            button = MPEG_START_SEEK;
            slider_state = STATE9;
            break;

        case MPEG_START_TIME_EXIT:
#ifdef MPEG_START_TIME_RC_EXIT
        case MPEG_START_TIME_RC_EXIT:
#endif
            button = MPEG_START_EXIT;
            slider_state = STATE9;
            break;

        case ACTION_STD_CANCEL:
            button = MPEG_START_QUIT;
            slider_state = STATE9;
            break;

#ifdef HAVE_LCD_ENABLE
        case LCD_ENABLE_EVENT_0:
            if (slider_state == STATE2)
                display_thumb_image(&rc_vid);
            continue;
#endif

        default:
            default_event_handler(button);
            yield();
            continue;
        }

        switch (slider_state)
        {
        case STATE0:
            if (!sliding)
            {
                trigger_cpu_boost();
                sliding = true;
            }
            stream_seek(resume_time, SEEK_SET);
            show_loading(&rc_bound);
            draw_slider(duration, resume_time, NULL);
            slider_state = STATE1;
            tmo = THUMB_DELAY;
            break;
        case STATE1:
            display_thumb_image(&rc_vid);
            slider_state = STATE2;
            tmo = TIMEOUT_BLOCK;
            if (sliding)
            {
                cancel_cpu_boost();
                if (global_settings.talk_menu)
                {
                    talk_value(resume_time / TS_SECOND, UNIT_TIME, false);
                    talk_value(resume_time * 100 / duration, UNIT_PERCENT, true);
                }
                sliding = false;
            }
        default:
            break;
        }

        yield();
    }

#if defined(HAVE_LCD_ENABLE) || defined(HAVE_LCD_SLEEP)
    remove_event(LCD_EVENT_ACTIVATION, get_start_time_lcd_enable_hook);
#endif
#ifndef HAVE_LCD_COLOR
    stream_gray_show(false);
    grey_clear_display();
    grey_update();
#endif

    cancel_cpu_boost();

    return button;
}

static int show_start_menu(uint32_t duration)
{
    int selected = 0;
    int result = 0;
    bool menu_quit = false;

    MENUITEM_STRINGLIST(menu, "MPEG Player", mpeg_sysevent_callback,
                        ID2P(LANG_RESTART_PLAYBACK),
                        ID2P(LANG_RESUME_PLAYBACK),
                        ID2P(LANG_SET_RESUME_TIME),
                        ID2P(LANG_SETTINGS),
                        ID2P(LANG_MENU_QUIT));

    button_clear_queue();

    while (!menu_quit)
    {
        mpeg_sysevent_clear();
        result = do_menu(&menu, &selected, NULL, false);

        switch (result)
        {
        case MPEG_START_RESTART:
            vv_settings.resume_time = 0;
            menu_quit = true;
            break;

        case MPEG_START_RESUME:
            menu_quit = true;
            break;

        case MPEG_START_SEEK:
            if (!stream_can_seek())
            {
                splash(HZ, ID2P(LANG_UNAVAILABLE));
                break;
            }

            result = get_start_time(duration);

            if (result != MPEG_START_EXIT)
                menu_quit = true;
            break;

        case MPEG_START_SETTINGS:
            mpeg_settings();
            break;

        default:
            result = MPEG_START_QUIT;
            menu_quit = true;
            break;
        }

        if (mpeg_sysevent() != 0)
        {
            result = MPEG_START_QUIT;
            menu_quit = true;
        }
    }

    return result;
}

/* Return the desired resume action */
int mpeg_start_menu(uint32_t duration)
{
    mpeg_sysevent_clear();

    switch (vv_settings.resume_options)
    {
    case MPEG_RESUME_MENU_IF_INCOMPLETE:
        if (!stream_can_seek() || vv_settings.resume_time == 0)
        {
    case MPEG_RESUME_RESTART:
            vv_settings.resume_time = 0;
            return MPEG_START_RESTART;
        }
    default:
    case MPEG_RESUME_MENU_ALWAYS:
        return show_start_menu(duration);
    case MPEG_RESUME_ALWAYS:
        return MPEG_START_SEEK;
    }
}

int mpeg_menu(void)
{
    int result;

    MENUITEM_STRINGLIST(menu, "MPEG Player", mpeg_sysevent_callback,
                        ID2P(LANG_SETTINGS),
                        ID2P(LANG_RESUME_PLAYBACK),
                        ID2P(LANG_MENU_QUIT));

    button_clear_queue();

    mpeg_sysevent_clear();

    result = do_menu(&menu, NULL, NULL, false);

    switch (result)
    {
    case MPEG_MENU_SETTINGS:
        mpeg_settings();
        break;

    case MPEG_MENU_RESUME:
        break;

    case MPEG_MENU_QUIT:
        break;

    default:
        break;
    }

    if (mpeg_sysevent() != 0)
        result = MPEG_MENU_QUIT;

    return result;
}

static void display_options(void)
{
    int selected = 0;
    int result;
    bool menu_quit = false;

    MENUITEM_STRINGLIST(menu, "Display Options", mpeg_sysevent_callback,
#if MPEG_OPTION_DITHERING_ENABLED
                        ID2P(LANG_DITHERING),
#endif
                        ID2P(LANG_DISPLAY_FPS),
                        ID2P(LANG_LIMIT_FPS),
                        ID2P(LANG_SKIP_FRAMES),
#ifdef HAVE_BACKLIGHT_BRIGHTNESS
                        ID2P(LANG_BACKLIGHT_BRIGHTNESS),
#endif
                        );

    button_clear_queue();

    while (!menu_quit)
    {
        mpeg_sysevent_clear();
        result = do_menu(&menu, &selected, NULL, false);

        switch (result)
        {
#if MPEG_OPTION_DITHERING_ENABLED
        case MPEG_OPTION_DITHERING:
            result = (vv_settings.displayoptions & LCD_YUV_DITHER) ? 1 : 0;
            mpeg_set_option(str(LANG_DITHERING), &result, RB_INT, noyes, 2, NULL);
            vv_settings.displayoptions =
                (vv_settings.displayoptions & ~LCD_YUV_DITHER)
                      | ((result != 0) ? LCD_YUV_DITHER : 0);
            lcd_yuv_set_options(vv_settings.displayoptions);
            break;
#endif /* MPEG_OPTION_DITHERING_ENABLED */

        case MPEG_OPTION_DISPLAY_FPS:
            mpeg_set_option(str(LANG_DISPLAY_FPS), &vv_settings.showfps, RB_INT,
                            noyes, 2, NULL);
            break;

        case MPEG_OPTION_LIMIT_FPS:
            mpeg_set_option(str(LANG_LIMIT_FPS), &vv_settings.limitfps, RB_INT,
                            noyes, 2, NULL);
            break;

        case MPEG_OPTION_SKIP_FRAMES:
            mpeg_set_option(str(LANG_SKIP_FRAMES), &vv_settings.skipframes, RB_INT,
                            noyes, 2, NULL);
            break;

#ifdef HAVE_BACKLIGHT_BRIGHTNESS
        case MPEG_OPTION_BACKLIGHT_BRIGHTNESS:
            result = vv_settings.backlight_brightness;
            mpeg_backlight_update_brightness(result);
            mpeg_set_int(str(LANG_BACKLIGHT_BRIGHTNESS), NULL, UNIT_INT, &result,
                         backlight_brightness_function, 1, -1,
                         MAX_BRIGHTNESS_SETTING - MIN_BRIGHTNESS_SETTING,
                         backlight_brightness_formatter,
                         backlight_brightness_getlang);
            vv_settings.backlight_brightness = result;
            mpeg_backlight_update_brightness(-1);
            break;
#endif /* HAVE_BACKLIGHT_BRIGHTNESS */

        default:
            menu_quit = true;
            break;
        }

        if (mpeg_sysevent() != 0)
            menu_quit = true;
    }
}

static void audio_options(void)
{
    int selected = 0;
    int result;
    bool menu_quit = false;

    MENUITEM_STRINGLIST(menu, "Audio Options", mpeg_sysevent_callback,
                        ID2P(LANG_TONE_CONTROLS),
                        ID2P(LANG_CHANNEL_CONFIGURATION),
                        ID2P(LANG_CROSSFEED),
                        ID2P(LANG_EQUALIZER),
                        ID2P(LANG_DITHERING));

    button_clear_queue();

    while (!menu_quit)
    {
        mpeg_sysevent_clear();
        result = do_menu(&menu, &selected, NULL, false);

        switch (result)
        {
        case MPEG_AUDIO_TONE_CONTROLS:
            mpeg_set_option(str(LANG_TONE_CONTROLS), &vv_settings.tone_controls, RB_INT,
                            globaloff, 2, NULL);
            sync_audio_setting(result, false);
            break;

        case MPEG_AUDIO_CHANNEL_MODES:
            mpeg_set_option(str(LANG_CHANNEL_CONFIGURATION), &vv_settings.channel_modes,
                            RB_INT, globaloff, 2, NULL);
            sync_audio_setting(result, false);
            break;

        case MPEG_AUDIO_CROSSFEED:
            mpeg_set_option(str(LANG_CROSSFEED), &vv_settings.crossfeed, RB_INT,
                            globaloff, 2, NULL);
            sync_audio_setting(result, false);
            break;

        case MPEG_AUDIO_EQUALIZER:
            mpeg_set_option(str(LANG_EQUALIZER), &vv_settings.equalizer, RB_INT,
                            globaloff, 2, NULL);
            sync_audio_setting(result, false);
            break;

        case MPEG_AUDIO_DITHERING:
            mpeg_set_option(str(LANG_DITHERING), &vv_settings.dithering, RB_INT,
                            globaloff, 2, NULL);
            sync_audio_setting(result, false);
            break;

        default:
            menu_quit = true;
            break;
        }

        if (mpeg_sysevent() != 0)
            menu_quit = true;
    }
}

static void resume_options(void)
{
    static const struct opt_items items[MPEG_RESUME_NUM_OPTIONS] = {
        [MPEG_RESUME_MENU_ALWAYS] =
            { STR(LANG_FORCE_START_MENU) },
        [MPEG_RESUME_MENU_IF_INCOMPLETE] =
            { STR(LANG_CONDITIONAL_START_MENU) },
        [MPEG_RESUME_ALWAYS] =
            { STR(LANG_AUTO_RESUME) },
        [MPEG_RESUME_RESTART] =
            { STR(LANG_RESTART_PLAYBACK) },
    };

    mpeg_set_option(str(LANG_MENU_RESUME_OPTIONS), &vv_settings.resume_options,
                    RB_INT, items, MPEG_RESUME_NUM_OPTIONS, NULL);
}

static void clear_resume_count(void)
{
    vv_settings.resume_count = 0;
    configfile_save(SETTINGS_FILENAME, config, ARRAYLEN(config),
                    SETTINGS_VERSION);
}

static void mpeg_settings(void)
{
    int selected = 0;
    int result;
    bool menu_quit = false;

    MENUITEM_STRINGLIST(menu, "Settings", mpeg_sysevent_callback,
                        ID2P(LANG_MENU_DISPLAY_OPTIONS),
                        ID2P(LANG_MENU_AUDIO_OPTIONS),
                        ID2P(LANG_MENU_RESUME_OPTIONS),
                        ID2P(LANG_MENU_PLAY_MODE),
                        ID2P(LANG_CLEAR_ALL_RESUMES));

    button_clear_queue();

    while (!menu_quit)
    {
        mpeg_sysevent_clear();

        result = do_menu(&menu, &selected, NULL, false);

        switch (result)
        {
        case MPEG_SETTING_DISPLAY_SETTINGS:
            display_options();
            break;

        case MPEG_SETTING_AUDIO_SETTINGS:
            audio_options();
            break;

        case MPEG_SETTING_ENABLE_START_MENU:
            resume_options();
            break;

        case MPEG_SETTING_PLAY_MODE:
            mpeg_set_option(str(LANG_MENU_PLAY_MODE), &vv_settings.play_mode,
                            RB_INT, singleall, 2, NULL);
            break;

        case MPEG_SETTING_CLEAR_RESUMES:
            clear_resume_count();
            break;

        default:
            menu_quit = true;
            break;
        }

        if (mpeg_sysevent() != 0)
            menu_quit = true;
    }
}

void init_settings(const char* filename)
{
    /* Set the default vv_settings */
    vv_settings.showfps = 0;     /* Do not show FPS */
    vv_settings.limitfps = 1;    /* Limit FPS */
    vv_settings.skipframes = 1;  /* Skip frames */
    vv_settings.play_mode = 0;   /* Play single video */
    vv_settings.resume_options = MPEG_RESUME_MENU_ALWAYS; /* Enable start menu */
    vv_settings.resume_count = 0;
#ifdef HAVE_BACKLIGHT_BRIGHTNESS
    vv_settings.backlight_brightness = -1; /* Use default setting */
#endif
#if MPEG_OPTION_DITHERING_ENABLED
    vv_settings.displayoptions = 0; /* No visual effects */
#endif
    vv_settings.tone_controls = false;
    vv_settings.channel_modes = false;
    vv_settings.crossfeed = false;
    vv_settings.equalizer = false;
    vv_settings.dithering = false;

    if (configfile_load(SETTINGS_FILENAME, config, ARRAYLEN(config),
                        SETTINGS_MIN_VERSION) < 0)
    {
        /* Generate a new config file with default values */
        configfile_save(SETTINGS_FILENAME, config, ARRAYLEN(config),
                        SETTINGS_VERSION);
    }

    strlcpy(vv_settings.resume_filename, filename, MAX_PATH);

    /* get the resume time for the current mpeg if it exists */
    if ((vv_settings.resume_time = configfile_get_value
         (SETTINGS_FILENAME, filename)) < 0)
    {
        vv_settings.resume_time = 0;
    }

#if MPEG_OPTION_DITHERING_ENABLED
    lcd_yuv_set_options(vv_settings.displayoptions);
#endif

    /* Set our audio options */
    sync_audio_settings(false);
}

void save_settings(void)
{
    unsigned i;
    for (i = 0; i < ARRAYLEN(config); i++)
    {
        configfile_update_entry(SETTINGS_FILENAME, config[i].name,
                                *(config[i].int_p));
    }

    /* If this was a new resume entry then update the total resume count */
    if (configfile_update_entry(SETTINGS_FILENAME, vv_settings.resume_filename,
                                vv_settings.resume_time) == 0)
    {
        configfile_update_entry(SETTINGS_FILENAME, "Resume count",
                                ++vv_settings.resume_count);
    }

    /* Restore audio options */
    sync_audio_settings(true);
}
