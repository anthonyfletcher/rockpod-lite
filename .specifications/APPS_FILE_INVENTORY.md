# `apps/` file inventory and placement review

Every file in `apps/` now carries a description of what it does, written by
reading the code rather than inferring from its name or location. This document
collects those descriptions and records where the current placement looks wrong.

Generated from the file headers themselves — regenerate with the extractor in
`.specifications/` rather than editing this by hand, or it will drift.

**How to read this:** the placement rule (`apps/README`) is

    puts pixels where it is told, no input loop      -> draw/
    called by someone, hands control back            -> widgets/
    navigated to, owns the screen until you leave    -> screens/
    also owns a file format and document model       -> viewers/

everything else by subject matter. The findings below are cases where a file
fails its own directory's test.

---

## Findings: files that fail their directory's test

Grouped by the *kind* of mismatch, because they want deciding as groups.

### 1. RESOLVED -- in screens/, but serving screens rather than being screens

| File | What it actually is | Suggested |
|---|---|---|
| `filetypes.c/h` | A registry: extension → attribute, icon, colour, viewer. Also loads the colour and viewer theme files. Its lookups are called from all over the UI. | `system/` or its own `files/` |
| `fileop.c/h` | Copy, move, delete, rename, create directory — operations with progress and confirmation UI, invoked by the context menu. Not a destination. | `files/`, beside the browser backend |
| `browser_files.c/h` | The browser's filesystem backend (`ft_load`, `ft_enter`, `ft_exit`). Correctly beside `browser.c`, but it is a data layer — the same relationship `tagtree.c` has to the database, and that lives in `database/`. | `files/`, or leave and accept the exception |

The strongest option is probably a `files/` directory holding `browser_files`,
`fileop` and `filetypes`, leaving `screens/browser.c` as the screen alone.

### 2. RESOLVED -- in widgets/, but not UI control flow at all

| File | What it actually is | Suggested |
|---|---|---|
| `peakmeter.c/h` | Samples PCM peaks, converts to dB, draws a bar with peak-hold. Nothing calls it and waits for a result. | `audio/` (it analyses PCM) or `skin/` (its only consumer) |
| `spectrum_meter.c/h` | Goertzel filter bank over recent PCM, exposing per-bar levels to the skin engine. Same shape. | as above |

Both are *sampled*, not *called*. They fail the widget test outright.

### 3. Singletons -- one fixed, three dismissed on the evidence

| File | Verdict |
|---|---|
| `system/app_util.c` | **FIXED.** Was 437 lines / 15 functions. Colour parsing (`hex_to_rgb`, `parse_color`, `hex2dec`) went to `draw/color.c`; system sounds and keyclick went to `audio/sound_feedback.c` — input triggers them, audio produces them, and they were only in `system/` because `misc.c` was. Now 273 lines / 9 functions: a genuine small residue rather than a dumping ground. |
| `draw/viewport.c` | **LEAVE — the suggested split does not work.** The claim above was that `viewport_*` are primitives and `viewportmanager_*` are the theme manager. False: `viewport_set_defaults`, the most-called function in the file, depends on `is_theme_enabled` and `sb_skin_get_info_vp`, because the default drawing area *is* whatever the status bar leaves. It is inherently theme-aware. The file is one coherent thing — the viewport manager — and splitting it would strand its most-used function away from every caller. |
| `widgets/menu.c` | **LEAVE — it passes the widget test.** `do_menu()` is called and returns a value to its caller. Running a loop internally does not make it a scene, any more than `gui_syncyesno_run()` is one. |
| `draw/screen_access.c` | **LEAVE.** It builds the `screens[]` display vtable that everything draws through — `draw/`'s foundation rather than a misplacement. |

Three of the four were me over-applying the taxonomy rather than real problems.
Recorded with the reasoning, so the same suspicions are not re-raised later
without the evidence that settled them.

### 4. Forced placements — correct, and must not be "tidied"

| File | Why it sits where it does |
|---|---|
| `apps/fracmul.h` | `lib/rbcodec/codecs/spc.c` includes `"../fracmul.h"`, which resolves against `-I` directories rather than by name. No file inside `api/` can satisfy it. |
| `apps/tagnavi.config` | `tools/buildzip.pl` reads that exact path, and buildzip.pl is kept byte-identical to upstream. |
| `apps/plugins/plugin.lds` | Still the live codec linker script (`lib/rbcodec/codecs/codecs.make`), despite living in a directory that contains no plugin. |

---

## Full inventory

### (root)  (4 files)
  fracmul.h                  Boundary shim. lib/rbcodec/codecs/spc.c includes "../fracmul.h", which resolves against the include path rather than by name, so this must sit at the apps/ root. The real header is system/fracmul.h.
  main.c                     Boot: brings up the hardware, filesystem, settings, playback and voice in order, shows the logo, then hands control to the root menu and never returns.
  root_menu.c                The top-level menu and the navigation loop behind it: dispatches to each screen, remembers where the user came from, and owns the start-up screen setting.
  root_menu.h                Interface to root_menu.c and the GO_TO_* screen identifiers every screen returns.

### api  (9 files)
  action.h                   Boundary shim. firmware/backlight.c includes "action.h" by bare name. This keeps that name resolvable from the include path no matter where the real header lives inside apps/ -- currently input/action.h -- so apps/ can be reorganised without editing files outside it. Update the path below when the real header moves.
  buffering.h                Boundary shim. lib/rbcodec/metadata/metadata.c includes "buffering.h" by bare name; the real header is audio/buffering.h. Update the path below when it moves.
  fracmul.h                  Boundary shim. lib/rbcodec/dsp includes "fracmul.h" by bare name; the real header is system/fracmul.h. Note that spc.c does NOT come through here -- it uses "../fracmul.h", which resolves against the include path rather than by name, and lands on the copy at the apps/ root.
  misc.h                     Boundary UMBRELLA over the six files misc.h was split into, deliberately wide rather than a narrow forward. Only firmware/powermgmt.c uses symbols from here (read_line, settings_parseline, both now in system/strutil.h), but firmware/scroll_engine.c and firmware/usb.c also include "misc.h" while using no symbol from it, so they may depend on what it pulled in transitively; including all six keeps their view identical to before the split. If firmware/ ever becomes editable: point powermgmt.c at system/strutil.h, delete the misc.h includes in scroll_engine.c and usb.c, then delete this file.
  plugin.h                   Vestigial boundary stub. lib/rbcodec/metadata/hes.c has a leftover include of "plugin.h" from when this fork had a plugin system; it needs nothing from the header, and the build succeeds with it empty. Unlike the other stubs here this one forwards to nothing, because apps/plugin.h was deleted -- no file inside apps/ included it. To finish the job: delete the include at lib/rbcodec/metadata/hes.c:12, then delete this file.
  rbcodecconfig.h            Boundary shim. lib/rbcodec includes "rbcodecconfig.h" by bare name; the real header is system/rbcodecconfig.h. Update the path below when it moves.
  rbcodecplatform.h          Boundary shim. lib/rbcodec/platform.h includes "rbcodecplatform.h" by bare name; the real header is system/rbcodecplatform.h. Update the path below when it moves.
  settings.h                 Boundary shim. firmware/ and lib/rbcodec/dsp include "settings.h" by bare name; the real header is settings/settings.h. Update the path below when it moves.
  splash.h                   Boundary shim. firmware/powermgmt.c includes "splash.h" by bare name. It used to resolve because tools/configure put apps/gui on the include path; that is no longer so, which makes this stub load-bearing rather than documentary. The real header is widgets/splash.h.

### api/gui  (1 files)
  yesno.h                    Boundary shim for a SLASHED path: firmware/usb.c:48 includes "gui/yesno.h", mirroring the pre-reorganisation layout. The real header is widgets/yesno.h.

### api/gui/skin_engine  (1 files)
  skin_engine.h              Boundary shim for a SLASHED path: firmware/usb.c:51 includes "gui/skin_engine/skin_engine.h", mirroring the pre-reorganisation layout. The real header is skin/skin_engine.h.

### audio  (26 files)
  ab_repeat.c                A-B repeat: stores the two markers and loops playback between them.
  ab_repeat.h                Interface to abrepeat.c.
  audio_path.c               Selects the audio input source. Vestigial on these targets, which have no recording or line-in.
  audio_thread.c             The audio thread's event loop. Receives queued playback commands and dispatches them into playback.c; owns the audio status.
  audio_thread.h             Interface to audio_thread.c, plus audio_init()/playback_init().
  beep.c                     Software beep generation for keyclicks and edge tones, mixed into the PCM stream. Only built where there is no hardware beeper.
  buffering.c                The buffering layer: manages the big audio buffer as handles, reads files into it ahead of playback, and hands data to the codec. Also buffers album art and cuesheets.
  buffering.h                Interface to buffering.c: the buf* handle API and the data_type values.
  codec_thread.c             The codec thread: loads a codec, runs its decode loop, and handles the callbacks it makes back into the core.
  codec_thread.h             Interface to codec_thread.c.
  codecs.c                   Loading and linking of .codec modules: locates the file for a format and binds it to the codec API struct.
  pcmbuf.c                   The PCM output ring buffer between the codec and the DAC. Owns crossfade mixing, the watermark that drives refills, and elapsed-time reporting.
  pcmbuf.h                   Interface to pcmbuf.c.
  peak_meter.c               Audio peak meter. Samples the PCM peaks, converts to dB with a logarithmic scale, and draws the bar with peak-hold and clip indication.
  peak_meter.h               Interface to peakmeter.c.
  play_status.c              The current playback mode (play, pause, ff/rewind) as the status bar and skin engine read it.
  play_status.h              The playmode enum and its accessors. The ordering is fixed -- the status bar icons and the skin %mp tag depend on it.
  playback.c                 The playback engine: the track lifecycle from buffering through decode to output, gapless handoff, seeking, and the album-art slots the skin draws from.
  playback.h                 Interface to playback.c.
  rbcodec_helpers.c          Glue the shared rbcodec library expects from the application: buffer allocation for the time-stretch DSP.
  sound_feedback.c           The sounds the UI makes: the standard system sounds and the keyclick, both produced through the beep generator. Input triggers them, audio produces them, which is why they live here rather than with the widgets.
  sound_feedback.h           Interface to sound_feedback.c.
  spectrum_meter.c           Spectrum analyser bars for the skin. Runs a Goertzel filter bank over recent PCM samples and exposes per-bar levels.
  spectrum_meter.h           Interface to spectrum_meter.c.
  voice_thread.c             The voice thread: decodes and mixes spoken clips over the music, at its own priority so speech stays responsive.
  voice_thread.h             Interface to voice_thread.c.

### database  (2 files)
  tagcache.c                 The tag database itself: builds and loads the on-disk index, and answers searches over it. The largest file in apps/.
  tagcache.h                 Interface to tagcache.c: the search context, the tag enum and the query API.

### draw  (22 files)
  bmp.c                      Reads Windows BMP files into a rockbox bitmap. Handles 1/4/8/16/24-bit input, RLE, palettes and dithering, and can scale on load via resize.c.
  bmp.h                      Interface to bmp.c, plus the shared image types (uint8_rgb, dim, rowset) and IMG_* load flags used by every image path.
  color.c                    Parses colours from text: "#rrggbb" hex into a native pixel value, and the named/hex forms skins and theme files use.
  color.h                    Interface to color.c.
  icon.c                     Draws themable icons and the list cursor onto a screen. Owns the active icon set, whether built-in or a user-supplied bitmap strip.
  icon.h                     Interface to icon.c and the themable_icons enum -- the canonical list of icon slots the UI and skins refer to.
  icon_bitmaps.c             The built-in monochrome icon bitmaps (5x8 and 7x8 tables, the disk icon) and the core bitmap registry.
  icon_bitmaps.h             Declares the built-in icon bitmap tables and the core bitmap registry entry type.
  jpeg_common.h              Shared JPEG tables and helpers used by jpeg_load.c and the ARM IDCT.
  jpeg_idct_arm.S            ARM assembler inverse DCT for the JPEG decoder. Pulls its struct offsets from the generated apps/system/core_asmdefs.h.
  jpeg_load.c                Baseline JPEG decoder used for album art: Huffman decode, IDCT and YUV-to-native conversion straight into a rockbox bitmap.
  jpeg_load.h                Interface to jpeg_load.c.
  line.c                     Renders one themed text line -- icon, text, scrolling, style -- into a viewport. The primitive the list renderer is built on.
  line.h                     Interface to line.c: the line_desc style descriptor and put_line()/vput_line().
  resize.c                   Image scaler. Area-average and linear scaling between bitmaps, plus recalc_dimension() for fitting an image to a target box.
  resize.h                   Interface to resize.c.
  screen_access.c            Builds the screens[] array: one struct screen per physical display, wiring each to its LCD driver's drawing functions. The abstraction everything else draws through.
  screen_access.h            Defines struct screen -- the per-display drawing vtable and metrics -- and declares screens[].
  scrollbar.c                Draws vertical and horizontal scrollbars, the bitmap scrollbar variant, and the indeterminate busy slider.
  scrollbar.h                Interface to scrollbar.c.
  viewport.c                 Viewport manager. Owns theme enable/undo per screen, the viewport stack, and the screen area left over once the status bar is reserved.
  viewport.h                 Interface to viewport.c and the viewport helpers used to set up drawing areas.

### files  (4 files)
  file_ops.c                 File operations with progress and confirmation: copy, move, delete, rename, create directory. Used by the context menu.
  file_ops.h                 Interface to fileop.c and its result codes.
  filetypes.c                The filetype registry: maps extensions to attributes, icons, colours and the core viewer that opens them. Loads the colour and viewer theme files.
  filetypes.h                Interface to filetypes.c and the FILE_ATTR_* attribute constants.

### iap  (8 files)
  iap-core.c                 The Apple accessory protocol core: framing, checksums, authentication and the device state shared by the lingo handlers.
  iap-core.h                 Interface to iap-core.c and the shared iAP state types.
  iap-lingo.h                Declares the per-lingo packet handlers.
  iap-lingo0.c               iAP lingo 0 (General): identification, capabilities and the handshake every accessory starts with.
  iap-lingo1.c               iAP lingo 1 (Microphone). Not reachable on these targets, which have no recording.
  iap-lingo2.c               iAP lingo 2 (Simple Remote): basic transport commands from a remote control.
  iap-lingo3.c               iAP lingo 3 (Display Remote): track metadata and playback state for accessories with a screen.
  iap-lingo4.c               iAP lingo 4 (Extended Interface): database browsing and playback control, the richest of the lingos and the one dock accessories use.

### input  (7 files)
  action.c                   Turns raw button events into context-sensitive actions: applies the keymap for the current context, handles repeat and long-press, and drives the backlight and keyclick.
  action.h                   The action enum -- every action the UI can receive -- plus the contexts and get_action().
  core_keymap.c              Loads a user keymap file (.kmf) from disk and overrides the built-in mapping. The header ID embeds the action count so a stale file is rejected rather than mis-bound.
  core_keymap.h              Interface to core_keymap.c and the keymap file header format.
  keymap_ipod.c              The iPod keymap: which button sequence produces which action in each context. The only target keymap this fork builds.
  usb_keymaps.c              Maps buttons to USB HID keycodes when acting as a keyboard, mouse or remote for a host.
  usb_keymaps.h              Interface to usb_keymaps.c.

### metadata  (9 files)
  albumart.c                 Finds album art for a track: searches the conventional filenames and directories, and reports the best match for a requested size.
  albumart.h                 Interface to albumart.c.
  art_cache.c                Disk cache for cover art -- BOTH album art and artist art. Pre-scales each image to the sizes skins ask for and stores it, so browsing does not re-decode on every track. Album art comes from the album folder, artist art from its parent; each has its own placeholder for when nothing is found.
  art_cache.h                Interface to art_cache.c.
  art_sizes.h                The album-art sizes this build caches, derived from the skins in use.
  cuesheet.c                 Cuesheet support: finds and parses a .cue beside a track, then reports which indexed sub-track is playing so the UI can show it as a separate song.
  cuesheet.h                 Interface to cuesheet.c and the cuesheet types.
  mul_id3.c                  Aggregates tags across many files: walks a directory collecting counts, sizes and durations for the properties screen.
  mul_id3.h                  Interface to mul_id3.c and the dir_stats type.

### playlist  (7 files)
  catalog.c                  The playlist catalogue: the directory of saved .m3u files, and adding tracks or directories to them.
  catalog.h                  Interface to catalog.c.
  playlist.c                 The playlist engine: the in-memory index of tracks, shuffle and repeat, insert and move, and the on-disk .m3u representation including the dynamic playlist.
  playlist.h                 Interface to playlist.c and the playlist_info type every playlist caller passes around.
  save_screen.h              Declares save_playlist_screen(), the "save current playlist" screen. Named for what it declares: it is a screen, not a menu, and is unrelated to the playlist settings screen that used to share its name.
  viewer.c                   The playlist viewer screen: lists the current or a saved playlist, and allows moving, removing and searching entries.
  viewer.h                   Interface to viewer.c.

### screens  (8 files)
  bookmark.c                 Bookmarks: creating, listing, loading and auto-bookmarking a playback position, including the most-recent list and the on-disk bookmark file format.
  bookmark.h                 Interface to bookmark.c.
  context_menu.c             The long-press context menu. Builds the item list appropriate to what was selected (file, directory, playlist entry) and runs the chosen operation.
  context_menu.h             Interface to context_menu.c (context_menu_show) and its custom-action values.
  main_menu.c                The main menu: system info, version, running time, credits, licences, and the manage-settings submenu.
  main_menu_config.c         Lets the user reorder and hide main menu entries, persisting the arrangement.
  shortcuts.c                The shortcuts menu: a user-editable list of jumps to files, directories, settings or screens, persisted to disk.
  shortcuts.h                Interface to shortcuts.c.

### screens/browse  (6 files)
  browser.c                  The file browser screen. Owns the browser_context (current directory, selection, filter), the browse loop, and dispatch into the database browser or a viewer.
  browser.h                  Interface to browser.c: browser_context, the browse_context entry point (rockbox_browse) and the dirfilter values.
  browser_db.c               Turns database queries into a browsable tree, driven by tagnavi.config. The database equivalent of the filesystem browser's backend.
  browser_db.h               Interface to browser_db.c.
  browser_disk.c             The browser's filesystem backend: reads a directory into the browser cache, decides what entering a file does, and builds playlists from a directory.
  browser_disk.h             Interface to browser_files.c (the ft_* entry points).

### screens/covers  (5 files)
  album_covers.c             Cover-flow album browser built on carousel.c. Supplies the slide model -- album list, art loading and its disk cache, sort order -- and what happens on select.
  album_covers.h             Interface to album_covers.c and the album-name display setting values.
  artist_portraits.c         Cover-flow artist browser. A second carousel.c model, showing artist photos instead of album art.
  carousel.c                 The cover-flow engine shared by album_covers.c and artist_portraits.c: slide cache, 3D projection, scrolling, input loop and worker thread. Each screen supplies a model.
  carousel.h                 The carousel model vtable and shared render state -- the interface between the engine and its two screens.

### screens/playback  (8 files)
  pitch_screen.c             Pitch and speed adjustment screen. Now a thin stub -- the interactive UI was a plugin and is gone; this resets pitch to normal.
  pitch_screen.h             Interface to pitchscreen.c.
  quick_screen.c             The quick screen: four settings bound to the directional buttons, edited in place without leaving playback.
  quick_screen.h             Interface to quickscreen.c.
  track_info.c               The Track Info screen: lists an mp3entry's tags, speaks them on request, and opens any single field full-screen via the text box.
  track_info.h               Interface to track_info.c (browse_id3).
  wps.c                      The While Playing Screen: the playback UI loop. Handles all playback input and drives the skin engine to repaint; the layout itself lives in skin/.
  wps.h                      Interface to wps.c, including the wps_do_action() verbs other code uses to control playback.

### screens/settings  (14 files)
  album_covers_settings.c    Settings menu for the Album Covers screen.
  common_settings.c          Helpers shared by the menu definitions, chiefly the "do you want to apply now" prompts.
  common_settings.h          Interface to menu_common.c.
  display_settings.c         Display settings menu: backlight, LCD, scrolling, status bar and related options.
  eq_settings.c              Equaliser settings menu, including the per-band editor and preset load/save.
  eq_settings.h              Interface to eq_menu.c.
  exported_settings.h        Declares the menu roots that other menus embed as submenus.
  general_settings.c         The general settings menu: filesystem, database, language, voice, hotkey and startup options.
  playback_settings.c        Playback settings menu: shuffle, repeat, crossfade, replaygain, resume and buffering options.
  playlist_settings.c        Playlist settings and the playlist catalogue menu entries.
  sound_settings.c           Sound settings menu: volume, bass, treble, balance and the channel/stereo options.
  text_viewer_settings.c     Settings menu for the core text viewer.
  theme_settings.c           Theme settings menu: skin selection, fonts, colours and backdrop.
  time_settings.c            Time and date menu: sets the clock and, where fitted, the alarm.

### screens/system  (12 files)
  alarm.c                    The RTC wake-up alarm screen. Sets the alarm time via the shared time picker and arms the hardware RTC.
  alarm.h                    Interface to alarm.c.
  debug_menu.c               The debug menu and its screens: hardware state, buffers, threads, disk, battery and view-log entries. Development aid, not user-facing.
  debug_menu.h               Interface to debug_menu.c.
  log_viewer.c               On-device viewer and dumper for the circular logf() buffer. Only built when ROCKBOX_HAS_LOGF is set.
  log_viewer.h               Interface to log_viewer.c.
  runtime_info.c             Running-time and top-time statistics screen, with the option to reset either counter.
  runtime_info.h             Interface to runtime_info.c.
  time_set.c                 The interactive date and time picker, with voiced feedback. Shared by the time menu and the alarm screen.
  time_set.h                 Interface to time_set.c.
  usb_screen.c               The USB connection screen shown while the device is mounted, including the logo or skinned variant and HID keypad handling.
  usb_screen.h               Interface to usb_screen.c.

### settings  (4 files)
  settings.c                 Loads, saves and applies settings: the config file format, reading and writing config.cfg, and pushing changed values into the running system.
  settings.h                 The global_settings and global_status structures -- the entire persisted state of the device -- plus the settings API.
  settings_list.c            The settings table: one entry per setting giving its name, type, range, default, config-file representation and voice clip. The single source of truth for what a setting is.
  settings_list.h            The settings_list entry type and the macros each setting is declared with.

### skin  (18 files)
  backdrop.c                 Loads a backdrop bitmap from disk and pushes it to the LCD. The plain, non-skin backdrop path.
  backdrop.h                 Interface to backdrop.c.
  custom_tags.c              Registry of this fork's non-upstream skin tags, kept separate so the additions are visible in one place.
  skin_albumart_color.c      Extracts a dominant colour palette from the current album art and exposes it to skins as dynamic colours, with fading between tracks.
  skin_albumart_color.h      Interface to skin_albumart_color.c.
  skin_backdrops.c           Manages backdrop bitmaps for skins: loading, reference counting and sharing them between screens.
  skin_display.c             Drawing helpers the renderer calls: progress bars, the embedded playlist viewer, album art placement and A-B markers.
  skin_display.h             Interface to skin_display.c.
  skin_engine.c              Owns the loaded skins: which skin file is active per screen, loading and freeing them, and the update entry point the rest of the UI calls.
  skin_engine.h              The skin engine's public interface: skin_update(), the skinnable_screens enum, and the render-inhibit control.
  skin_parser.c              Parses a .wps/.sbs skin file into the internal element tree: tags, viewports, conditionals, images and their buflib allocations.
  skin_render.c              Walks the parsed element tree and draws it: viewports, conditionals, alternators and progress bars, resolving each tag as it goes.
  skin_tokens.c              Resolves each skin tag to a value -- track title, elapsed time, battery, and the rest. The bridge between the skin language and the running system.
  statusbar.c                The built-in (non-skinned) status bar: battery, volume, playback mode, time and disk activity drawn directly.
  statusbar.h                Interface to statusbar.c and the status bar state types.
  statusbar_skinned.c        The skinned status bar (.sbs): drives a skin file as the status bar, including the title text and icon other screens set.
  statusbar_skinned.h        Interface to statusbar_skinned.c.
  wps_internals.h            Internal types shared across the skin engine: wps_data, the element and viewport structures, and the token enum. Not for use outside skin/.

### speech  (4 files)
  language.c                 Loads a translated .lng file over the built-in strings, and reports text direction for right-to-left languages.
  language.h                 Interface to language.c.
  talk.c                     Voice output: loads the .voice clip file, queues and mixes clips, and provides the talk_* vocabulary (numbers, dates, spelling) the UI speaks with.
  talk.h                     Interface to talk.c and the talk_* vocabulary.

### system  (20 files)
  activity.c                 The activity stack: tracks which screen is in front so the skin engine and status bar can react to context changes.
  activity.h                 The current_activity enum and the push/pop API.
  app_buffer.c               Hands out the linker-reserved scratch buffer to whichever screen needs a large temporary allocation, guarding against two owners at once.
  app_buffer.h               Interface to app_buffer.c.
  app_util.c                 Cross-cutting UI helpers that belong to no single screen: colour parsing, bitmap loading into buflib, keyclick and system sounds, dynamic value formatting, and small confirmation prompts.
  app_util.h                 Interface to app_util.c.
  appevents.h                The event ids other subsystems subscribe to -- track change, playback state, settings changed.
  applimits.h                Compile-time limits: maximum path length, filename length and similar caps.
  core_asmdefs.c             Emits struct offsets as assembler constants, generating core_asmdefs.h for the ARM assembly routines to include.
  format_time.c              Formats durations and clock values for display and speech, including the auto-ranging formatter and the sleep timer's text.
  format_time.h              Interface to format_time.c and the unit index flags it takes.
  fracmul.h                  Fixed-point fractional multiply helpers used by the DSP and scalers.
  rbcodecconfig.h            Configuration the shared rbcodec library expects from the application.
  rbcodecplatform.h          Platform hooks the shared rbcodec library expects from the application.
  shutdown.c                 Orderly shutdown and system events: flushing state to disk, the poweroff path, the default event handler, and the car-adapter and headphone-unplug behaviours.
  shutdown.h                 Interface to shutdown.c: the default event handlers and car adapter init.
  strutil.c                  String, path and line-parsing helpers: reading lines from a file, splitting settings lines, path fixups, and the UTF-8 aware open helpers.
  strutil.h                  Interface to strutil.c, plus the byte-order-mark constants.
  volume.c                   Volume control: the perceptual and direct adjustment modes, the normalised-volume mapping, replaygain mode selection and sound-value formatting.
  volume.h                   Interface to volume.c.

### viewers  (6 files)
  credits.c                  The credits screen: scrolls the contributor list generated into credits.raw from docs/CREDITS at build time.
  credits.h                  Interface to credits.c.
  playing_time.c             Playing-time statistics for the current playlist: elapsed, remaining and total time, track counts and sizes.
  playing_time.h             Interface to playing_time.c.
  properties.c               File and directory properties: size, dates and, for audio, the full tag set. Aggregates whole directories via metadata/mul_id3.c.
  properties.h               Interface to properties.c.

### viewers/image_viewer  (8 files)
  image_decoder.c            The decoder registry: maps a file's type to the right decoder and exposes a uniform load interface to image_viewer.c.
  image_decoder.h            The image_decoder vtable each format back end implements.
  image_viewer.c             The image viewer UI: loads a picture through the decoder registry, then handles zoom, pan, slideshow and next/previous within a directory.
  image_viewer.h             Shared interface between the image viewer and its decoders: the decoder vtable, the shared buffer, and the progress callback.
  image_viewer_button.h      Button mapping for the image viewer, kept separate so the keypad bindings are all in one place.
  image_viewer_pub.h         The image viewer's public entry point, for callers outside viewers/.
  resize.c                   Smooth (area-average) bitmap scaler used when fitting a decoded image to the screen. Separate from draw/resize.c, which serves album art.
  xlcd.c                     Hardware-assisted LCD block scrolling, used to pan a large image without redrawing it.

### viewers/image_viewer/decoders  (22 files)
  bmp.c                      BMP back end for the image viewer, wrapping the core BMP reader in the decoder vtable.
  crc32.c                    CRC-32 used to validate PNG chunks and zlib streams.
  gif.c                      GIF back end for the image viewer.
  gif_decoder.c              GIF decoding on top of vendored giflib: drives the frame loop and converts palette output to native pixels.
  gif_decoder.h              Interface to gif_decoder.c.
  gif_glue.h                 Adapts vendored giflib to this build: memory via the TLSF pool, and the file access it expects.
  jpeg.c                     Baseline JPEG back end for the image viewer, separate from the album-art decoder in draw/.
  jpeg_decoder.c             Baseline JPEG decoding for the viewer: Huffman, dequantisation and IDCT into a native bitmap.
  jpeg_decoder.h             Interface to jpeg_decoder.c.
  jpegp.c                    Progressive JPEG back end, using the vendored jpeg81 coefficient decoder.
  jpegp_glue.h               Adapts the vendored jpeg81 decoder to this build.
  png.c                      PNG back end for the image viewer.
  png_decoder.c              PNG parser: chunk walking, filter reversal and interlace handling, inflating through tinflate.c.
  png_decoder.h              Interface to png_decoder.c.
  ppm.c                      PPM/PGM back end for the image viewer.
  ppm_decoder.c              Netpbm parser: reads the ASCII or binary PPM/PGM header and pixel data.
  ppm_decoder.h              Interface to ppm_decoder.c.
  tinf.h                     Interface to the tinf decompressor.
  tinflate.c                 DEFLATE decompressor (tinf, by Joergen Ibsen) as adapted by Rockbox. Used by the PNG decoder.
  tinfzlib.c                 zlib-wrapper entry point around tinflate.c.
  yuv2rgb.c                  Converts decoded YUV planes to native-format pixels.
  yuv2rgb.h                  Interface to yuv2rgb.c.

### viewers/text_viewer  (16 files)
  text_viewer.c              The text reader UI: paging, scrolling, bookmarking and settings, drawing text pulled from the ts_* extraction engine.
  text_viewer.h              Interface to text_viewer.c.
  ts_charset.c               Detects the input encoding (BOM, UTF-8 validity, codepage heuristics) and converts it to UTF-8.
  ts_core.c                  The engine core: arena allocation, the file stream, format probing, and the public entry points from txt_source.h.
  ts_inflate.c               Streaming DEFLATE and zlib decompressor, used by the zip-backed document formats.
  ts_internal.h              Shared internals of the ts_* engine -- stream types and per-stage restart hooks. Not part of the public API.
  ts_io_core.c               Implements the engine's ts_io abstraction over the core file API. The only file in the engine that knows about Rockbox.
  ts_io_core.h               Interface to ts_io_core.c.
  ts_markup.c                Back end for loose markup: HTML and XHTML read directly from the file.
  ts_pdf.c                   Text extraction from PDF: object parsing, stream filters, content-stream tokenising and text positioning.
  ts_pdffont.c               PDF font handling: /ToUnicode CMaps and the page resources that name them, so extracted glyphs map back to characters.
  ts_tags.c                  Turns tagged markup into flowing text: strips tags, resolves entities, and applies block and whitespace rules.
  ts_text.c                  Back ends for the formats that need no container: plain text, markdown and RTF.
  ts_zip.c                   Just enough ZIP to walk an EPUB or DOCX container: central directory, entry lookup, and stream-per-entry.
  ts_zipdoc.c                The zip-backed documents: EPUB, DOCX and .fb2.zip. Finds the content parts inside the container and feeds them through the markup path.
  txt_source.h               Public API of the ts_* text-extraction engine: open a document, read UTF-8 out of it, close it. The only header a caller needs.

### widgets  (24 files)
  color_picker.c             Full-screen RGB colour chooser. Shows a live swatch and per-channel sliders; set_color() returns true if the user accepted.
  color_picker.h             Interface to color_picker.c.
  dialog.c                   Modal box primitive shared by the popup, yes/no and text-input dialogs. Draws a centred bordered box and returns its content viewport; owns the dialog style defaults.
  dialog.h                   Interface to dialog.c: the dialog_style descriptor and the frame/inset helpers.
  folder_select.c            Tri-state folder picker used by the database scan list and autoresume. Walks the filesystem into a collapsible tree and serialises the minimal include/exclude path set back into a setting string.
  folder_select.h            Interface to folder_select.c.
  keyboard.c                 On-screen keyboard. kbd_input() edits a string via a character grid or the scroll wheel; dialog_input() is the boxed single-line variant.
  keyboard.h                 Interface to keyboard.c.
  list.c                     The scrollable list widget: state, selection, scrolling, voicing, and the standard input loop (list_do_action, simplelist_show_list). Delegates painting to list_render.c or list_skinned.c.
  list.h                     Interface to the list widget -- gui_synclist and the callback types every list screen implements.
  list_render.c              Paints a list using the core renderer: title, per-item icon and text via draw/line.c, scrollbar. Used when the theme is off or the skin defines no list.
  list_skinned.c             Paints a list using a skin-defined %Vl viewport instead of the core renderer, including album-art rows. Configured per screen by the skin engine.
  mask_select.c              Generic checkbox tree over a bitmask. Presents nested categories and returns the edited mask; used for selective-backlight style settings.
  mask_select.h              Interface to mask_select.c.
  menu.c                     The menu engine. Walks a static menu_item_ex tree, renders it as a list, dispatches callbacks, and hands settings items to the option/value screens.
  menu.h                     The menu item type and the MENUITEM_* macros every menu is declared with, plus do_menu().
  option_select.c            Edits one setting's value: renders it as text or a chooser list, speaks it, and applies the result. The bridge between settings_list.c and the menu.
  option_select.h            Interface to option_select.c.
  splash.c                   Transient centred message overlay -- splash(), splashf() and the progress variant. Saves and restores whatever it covers.
  splash.h                   Interface to splash.c.
  text_box.c                 Full-screen scrollable display for a string already in memory: word wraps, paginates and scrolls. Distinct from viewers/text_viewer, which streams documents from a file.
  text_box.h                 Interface to text_box.c (view_text).
  yesno.c                    Yes/no confirmation dialog with optional timeout and voiced prompts. Returns YESNO_YES, _NO, _TMO or _USB.
  yesno.h                    Interface to yesno.c: the text_message type and the yesno result enum.
