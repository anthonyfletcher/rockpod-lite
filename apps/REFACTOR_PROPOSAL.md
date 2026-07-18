# `apps/` Refactor — Analysis & Proposal

**Target hardware:** iPod Video 5G/5.5G (`ipodvideo`, PP5022) and iPod Classic 6G/7G (`ipod6g`, S5L8702). Same 320×240 LCD, same app layer.
**Scope:** the `apps/` tree only. Everything else (`firmware/`, `lib/`, `bootloader/`) stays where it is so it can keep tracking upstream. `apps/` is treated as fully independent and eventually re-forkable as a standalone folder.

---

## 1. Executive summary

`apps/` is structurally flat and altitude-blind: ~95 loose files at the top level mix the audio engine, UI infrastructure, settings, and individual feature screens with no grouping, and the few subdirectories that exist (`gui/`, `recorder/`, `menus/`) each mix several different altitudes. Nothing tells a reader whether a given file is a reusable primitive, a shared widget, a full-screen feature, or a background service.

This proposal does three things:

1. **Introduces an explicit layered taxonomy** — `primitives → components → scenes → apps`, plus `engine`, `services`, `platform`, and a thin `core` — and maps every current file into it.
2. **Plans the complete removal of the plugin framework.** All six remaining plugins plus the image viewer and a re-added video viewer become core code, after which `plugin.c`, `open_plugin.c`, the 937-line plugin API struct, the overlay loader, `plugin.lds`/`plugin_crt0.c`, `plugins/lib/`, and the `viewers.config`/`CATEGORIES` machinery are deleted outright.
3. **Fixes naming and flags cleanup opportunities** found along the way.

The work is sequenced into stages that each leave the tree building and bootable, so it can be done incrementally rather than as one big-bang commit.

---

## 2. Guiding principles

- **`apps/` is independent; the rest is not.** Move and rename freely inside `apps/`. Avoid changing file locations under `firmware/`, `lib/`, etc. Where an `apps/` file is `#include`d by a path from outside `apps/`, note it and keep a shim or update the one caller — don't let a rename ripple outside the folder.
- **One altitude per directory.** A directory's name should tell you what *kind* of thing lives there, not just what feature it serves.
- **A file's name should describe its function.** Retire misnomers (`recorder/`) and opaque prefixes where they hurt more than they help.
- **Every deletion must be call-site-verified.** The current `plugins/SOURCES` already documents which plugins are load-bearing; that discipline continues — nothing gets removed until its core replacement exists and its last `plugin_load()` caller is gone.
- **Each stage builds and boots.** No stage leaves the firmware in a non-compiling or non-running state.

---

## 3. Current-state assessment

### 3.1 The flat top level (~95 files)

The `apps/` root holds, with no separation:

| Concern | Representative files |
|---|---|
| Audio engine | `playback.c` (137 KB), `pcmbuf.c`, `buffering.c` (55 KB), `codec_thread.c`, `codecs.c`, `audio_thread.c`, `audio_path.c`, `voice_thread.c`, `beep.c`, `abrepeat.c` |
| Database | `tagcache.c` (154 KB — the single largest file), `tagtree.c` (96 KB), `tagnavi.config` |
| Playlists | `playlist.c` (127 KB), `playlist_catalog.c`, `playlist_viewer.c` |
| Settings | `settings.c`, `settings_list.c` (83 KB) |
| UI infrastructure / navigation | `menu.c`, `action.c`, `root_menu.c` (57 KB), `tree.c`, `filetree.c`, `filetypes.c`, `fileop.c`, `onplay.c`, `shortcuts.c`, `screens.c`, `status.c`, `screen_access.c`, `gesture.c` |
| Voice / i18n | `talk.c`, `language.c` |
| Platform glue | `main.c`, `misc.c` (54 KB), `usb_keymaps.c`, `core_keymap.c`, `core_asmdefs.c` |
| Debug | `debug_menu.c` (71 KB), `logfdisp.c` |
| Plugin framework | `plugin.c`, `open_plugin.c` |
| Feature screens | `bookmark.c`, `alarm_menu.c`, `cuesheet.c`, `albumart_cache.c` |

There is no way to look at this list and see the shape of the system.

### 3.2 Misnamed / mixed-altitude directories

- **`recorder/`** — the worst offender. The iPods don't record. This directory actually holds general-purpose rendering and input helpers: `bmp.c`, `icons.c`, `keyboard.c`, `jpeg_load.c` + `jpeg_idct_arm.S`, `resize.c`, `albumart.c`, `peakmeter.c`, `spectrum_meter.c`. The name is a pure historical artifact.
- **`gui/`** — mixes four altitudes in one flat directory:
  - *primitives:* `viewport.c`, `line.c`, `scrollbar.c`, `icon.c`, `list.c`, `bitmap/list.c`, `bitmap/list-skinned.c`
  - *components (reusable widgets):* `dialog.c`, `yesno.c`, `splash.c`, `option_select.c`, `color_picker.c`, `folder_select.c`, `mask_select.c`, `statusbar.c`, `statusbar-skinned.c`
  - *scenes (full screens):* `wps.c`, `quickscreen.c`, `pitchscreen.c`, `usb_screen.c`
  - *subsystems:* `skin_engine/` (a whole skin interpreter), `backdrop.c`
  - *features:* `album_covers.c`, `artist_portraits.c`
  - *test scaffolding:* `dialog_test.c`
- **`menus/`** — this one is actually fine (it's cohesive: one file per settings-menu area), but it currently sits as a sibling of `gui/` rather than being understood as the "settings scenes" layer.

### 3.3 Plugin framework still present for a handful of callers

Plugins have already been trimmed hard (the rationale is well documented in `plugins/SOURCES` and `plugins/SUBDIRS`). What remains:

- **Loose plugins:** `credits.c`, `db_folder_select.c`, `lastfm_scrobbler.c`, `playing_time.c`, `properties.c`, `view_text.c`
- **Subdir plugins:** `text_viewer/` (already superseded by the new core `text_viewer/` — see §5.3), `imageviewer/`
- **Framework kept alive only to serve those:** `plugin.c` (1034 lines), `open_plugin.c` (604 lines), `plugin.h` (937 lines — the versioned API struct), `plugins/plugin_crt0.c`, `plugins/plugin.lds` (11.7 KB linker script), `plugins/lib/` (41 `.c` files), `plugins/bitmaps/`, `plugins/viewers.config`, `plugins/CATEGORIES`, `plugins/rockbox-fonts.config`, the `.make` build fragments.

That's a large, complex dynamic-loading subsystem (overlay linking, API versioning, a relocatable-code loader) kept alive for **eight** call sites. Removing it is the single biggest simplification available.

### 3.4 Dead / near-dead remnants

- **Radio:** already removed as a module. Only cosmetic remnants remain — a comment in `root_menu.c:85` and an enum-group comment `/* radio */` in `action.h:207`. Both configs have `CONFIG_TUNER` commented out. Trivial to sweep.
- **`gui/pitchscreen.c:26`** references a `pitch_screen.rock` that no longer exists as a shipped plugin — a stale fallback path to audit.
- **`gui/dialog_test.c`** — test scaffolding shipped in the core build (`gui/dialog_test.c` is in `apps/SOURCES`). Candidate for removal or a debug-only guard.

---

## 4. Proposed taxonomy

Eight layers. The rule for each is a one-liner you can apply to any file without ambiguity.

```
apps/
├── core/         Startup, the main loop, and cross-cutting glue that everything
│                 depends on and that depends on everything. main.c, misc.c.
│
├── primitives/   Stateless drawing/geometry/input building blocks. No feature
│                 knowledge. viewport, line, scrollbar, icon, list, bmp, resize.
│
├── components/   Reusable, embeddable UI widgets driven by a caller. Return a
│                 result; don't own the screen. dialog, yesno, splash,
│                 option_select, color_picker, folder_select, keyboard, statusbar.
│
├── scenes/       Full-screen, self-contained interaction loops the user navigates
│                 to. wps, quickscreen, pitchscreen, usb_screen, browser(tree),
│                 settings menus, debug menu, playlist viewer, bookmarks.
│
├── apps/         Standalone "applications" launched from a menu, each owning its
│                 own model + view + input. text_viewer, image_viewer,
│                 video_viewer, credits, properties, playing_time, scrobbler.
│
├── engine/       Non-UI subsystems that run the device: the audio pipeline, the
│                 database, playlists, the skin interpreter, voice.
│
├── services/     Cross-cutting core services used by many scenes/apps: settings,
│                 the action/keymap system, talk, i18n, album-art cache, filetypes.
│
└── platform/     Target/host glue: iAP accessory protocol, USB keymaps, iPod
                  keymap, asmdefs. (Kept apps-local; does not reach into firmware/.)
```

The distinction that does the most work here is **components vs. scenes vs. apps**:
- A **component** is called by someone else and hands control back (`gui_syncyesno_run()` returns yes/no).
- A **scene** is navigated *to* and runs its own loop until the user leaves (the WPS, the quickscreen).
- An **app** is a scene with its own persistent model and file/format handling that could almost be a standalone program (the text viewer, the image viewer).

### 4.1 Target tree (concrete)

```
apps/
├── core/
│   ├── main.c
│   └── misc.c
│
├── primitives/
│   ├── viewport.c            (from gui/)
│   ├── line.c                (from gui/)
│   ├── scrollbar.c           (from gui/)
│   ├── icon.c                (from gui/)
│   ├── list.c  list_bitmap.c list_skinned.c   (from gui/ and gui/bitmap/)
│   ├── bmp.c                 (from recorder/)
│   ├── resize.c              (from recorder/)
│   ├── jpeg_load.c  jpeg_idct_arm.S            (from recorder/)
│   └── icons.c               (from recorder/)
│
├── components/
│   ├── dialog.c  yesno.c  splash.c            (from gui/)
│   ├── option_select.c  color_picker.c  mask_select.c   (from gui/)
│   ├── folder_select.c                       (from gui/; see §5.4)
│   ├── keyboard.c                            (from recorder/)
│   ├── statusbar.c  statusbar_skinned.c      (from gui/)
│   └── peakmeter.c  spectrum_meter.c         (from recorder/)
│
├── scenes/
│   ├── wps.c                                 (from gui/)
│   ├── quickscreen.c  pitchscreen.c          (from gui/)
│   ├── usb_screen.c                          (from gui/)
│   ├── browser/       tree.c filetree.c fileop.c filetypes.c onplay.c
│   ├── playlist_viewer.c  playlist_catalog.c
│   ├── bookmark.c  cuesheet.c  shortcuts.c
│   ├── alarm.c                               (from alarm_menu.c)
│   ├── menus/         (the existing settings menus, moved intact)
│   └── debug/         debug_menu.c  logfdisp.c
│
├── apps/
│   ├── text_viewer/   (already core — the ts_* engine, see §5.3)
│   ├── image_viewer/  (ported from plugins/imageviewer — §5.2)
│   ├── video_viewer/  (re-added — §5.5)
│   ├── credits/       (rebuilt in core — §5.6)
│   ├── properties.c   (from plugins/properties.c — §5.4)
│   ├── playing_time.c (from plugins/playing_time.c — §5.4)
│   └── scrobbler.c    (from plugins/lastfm_scrobbler.c — §5.4)
│
├── engine/
│   ├── audio/    playback.c pcmbuf.c buffering.c codec_thread.c codecs.c
│   │             audio_thread.c audio_path.c beep.c abrepeat.c
│   │             voice_thread.c
│   ├── database/ tagcache.c tagtree.c tagnavi.config
│   ├── playlist/ playlist.c
│   ├── skin/     (from gui/skin_engine/ + backdrop.c)
│   └── albumart/ albumart.c  albumart_cache.c   (from recorder/ + top level)
│
├── services/
│   ├── settings/ settings.c settings_list.c
│   ├── action.c  gesture.c  screen_access.c  screens.c  status.c
│   ├── menu.c  root_menu.c  open list (see §5.1 for open_plugin's fate)
│   ├── talk.c  language.c
│   └── albumart_covers.c  artist_portraits.c   (from gui/)
│
└── platform/
    ├── iap/                  (moved intact)
    ├── usb_keymaps.c
    ├── keymap_ipod.c         (from keymaps/keymap-ipod.c)
    ├── core_keymap.c
    └── core_asmdefs.c
```

*(File placements marked above are the recommendation; the handful of genuinely arguable ones are called out in §8 Open questions rather than decided unilaterally.)*

---

## 5. Plugin framework removal

This is the largest and highest-value part of the work. The end state has **no dynamic plugin loading at all**.

### 5.1 Call-site inventory (what currently reaches into plugins)

Every core→plugin entry point, and where it goes:

| Call site | Plugin loaded | Destination |
|---|---|---|
| `menus/main_menu.c:120` | `lastfm_scrobbler.rock` (startup TSR) | `apps/scrobbler.c` core service, started from `main.c` |
| `menus/main_menu.c:133` | `credits.rock` | `apps/credits/` core (already has an info-screen fallback) |
| `menus/main_menu.c:140,142` | `text_viewer.rock` (COPYING/LICENSES) | core `text_viewer/` (§5.3) |
| `menus/settings_menu.c:131,591` | `db_folder_select.rock` | core `folder_select` component (§5.4) |
| `onplay.c:215` | `playing_time.rock` | `apps/playing_time.c` core |
| `onplay.c:221` | `imageviewer.rock` (View Album Art) | core `image_viewer/` (§5.2) |
| `screens.c:823` | `view_text.rock` (Track Info long field) | core `text_viewer/` (§5.3) |
| `filetree.c:675,702,734,753` / `filetypes.c:710,738` | generic viewer dispatch via `viewers.config` | core filetype→app dispatch table (§5.7) |
| `gui/pitchscreen.c:26` | `pitch_screen.rock` (stale) | delete the dead fallback |
| `open_plugin.c`, `root_menu.c:1164-1172`, `main.c:167` | `open_plugin` / start-screen / autostart | remove the open-plugin subsystem (§5.7) |

### 5.2 Image viewer → core app *(you're converting this)*

`plugins/imageviewer/` is a container plugin plus per-format decoder sub-plugins: `bmp`, `gif` (giflib), `jpeg` + `jpegp` (progressive), `png` (tinf/zlib), `ppm`. The core already contains a JPEG path in `recorder/jpeg_load.c` used for album art, so the JPEG decode is partly duplicated.

Recommended core shape: `apps/image_viewer/` with `image_viewer.c` (the scene/loop) and a `decoders/` set. Reuse the core JPEG loader where possible instead of shipping `imageviewer/jpeg` twice. GIF/PNG/PPM decoders move in as-is but lose the plugin-API indirection (`rb->` calls become direct core calls). This is the biggest single port; the decoder count (5 formats) makes it worth its own stage.

### 5.3 Text viewer — retire the plugin, it's already core

A core `text_viewer/` already exists (the `ts_*` extraction engine — `ts_core`, `ts_text`, `ts_markup`, `ts_pdf`, `ts_zip`, `ts_charset`, etc., built through Stage 3 per the git history). Two call sites still load the *old* `text_viewer.rock`/`view_text.rock`:

- **Legal screens** (`main_menu.c:140,142`) → point at the core viewer.
- **Track Info long-field view** (`screens.c:823`, currently `view_text.rock` via `simple_viewer.c`) → point at the core viewer.

Once both are switched, `plugins/text_viewer/`, `plugins/view_text.c`, and `plugins/lib/simple_viewer.c` all go away.

### 5.4 The three "what does this even do" plugins *(answering your uncertainties)*

- **`playing_time.c`** — backs the **"Playing Time"** item on the current-playlist context menu (`onplay.c:215`). It computes and displays elapsed/remaining/total time and track-count stats for the running playlist. Small; becomes a plain core function `apps/playing_time.c`.
- **`db_folder_select.c`** — backs two things in `settings_menu.c`: the database **"Directories to Scan"** setting (`:131`) and the **custom autoresume-next-track folder picker** (`:591`). It's a checkbox tree folder selector. **The core already ships `gui/folder_select.c`** doing essentially this job — the likely outcome is *delete `db_folder_select` and re-point both call sites at the core `folder_select` component*, rather than porting it. Worth a focused look to confirm the core widget covers both cases (multi-select + persistence format).
- **`properties.c`** — File Properties / Track Info (`onplay.c` menu items + the `HOTKEY_PROPERTIES` hotkey, dispatched through `viewers.config`'s `*,properties,-` mapping). Uses `plugins/lib/mul_id3.c` (multi-file ID3 aggregation) and `simple_viewer.c`. Becomes `apps/properties.c`; `mul_id3` moves to `engine/database/` or `services/` as a plain helper.

### 5.5 Video viewer → re-add in core *(you flagged this)*

The old mpegplayer is not present in this branch's history, so this is effectively a fresh core addition rather than a revert. It'll need: the MPEG demux/decode path, an audio-sync loop against the existing PCM buffer, and a scene UI. This is the **largest unknown** in the whole plan — recommend scoping it as its own separate project *after* the reorg and the other plugin conversions land, so it's built directly into the new `apps/video_viewer/` structure rather than ported through the plugin API and then un-ported. (One remaining `mpegplayer` string reference exists in `iap/iap-lingo4.c` — an accessory-protocol capability flag, unrelated to the viewer itself.)

### 5.6 Credits → rebuild in core *(you're doing this)*

`credits.c` already degrades to a plain info screen when the `.rock` is missing (`main_menu.c:133`), so the fallback path is the seed of the core version. The scrolling-names presentation and the generated `credits.raw`/`credits.pl` name list move into `apps/credits/`.

### 5.7 Framework teardown (last stage, after everything above)

Once no call site loads a `.rock`, delete in one sweep:

- `plugin.c`, `plugin.h` (the 937-line API struct), `open_plugin.c`, `open_plugin.h`
- `plugins/plugin_crt0.c`, `plugins/plugin.lds`, `plugins/plugins.make`, `plugins/SOURCES*`, `plugins/SUBDIRS*`, `plugins/CATEGORIES`, `plugins/viewers.config`, `plugins/rockbox-fonts.config`
- `plugins/lib/` (all 41 `.c` files) and `plugins/bitmaps/`
- The generic viewer dispatch in `filetree.c`/`filetypes.c` — replaced by a small static filetype→core-app table (text/image/video/properties), since the set of "viewers" is now fixed and known at compile time.
- The `open_plugin` shortcut/start-screen/autostart machinery (`root_menu.c`, `main.c:167`), plus the `ACTIVITY_UNKNOWN`/`plugin_load` guards scattered around (`playlist_viewer.c:641`, `root_menu.c:1434`).
- Build-system references: the `plugins` subdir in the top-level make, `PLUGIN_DIR`/`VIEWERS_DIR` constants, plugin-API version bumps.

This removes on the order of **2,500+ lines of framework** plus the entire `plugins/lib` tree, in exchange for ~7 direct core call sites.

---

## 6. Naming improvements

- `recorder/` → dissolved into `primitives/` and `components/` (see §4.1). The name goes away entirely.
- `keymaps/keymap-ipod.c` → `platform/keymap_ipod.c` (underscore, matches the all-lowercase-with-underscores house style; the hyphen is inconsistent with the rest of the tree).
- `alarm_menu.c` → `scenes/alarm.c` (it's the alarm scene, not just a menu).
- `statusbar-skinned.c`, `list-skinned.c`, `main_menu_config.c` — hyphens → underscores for consistency.
- **`text_viewer/ts_*` prefix:** the `ts_` (text source) prefix is actually *good* — it namespaces the extraction engine cleanly. Recommend keeping it. Flagging only because it's the one place that already did namespacing right, and the rest of the tree should move toward that pattern, not away from it.
- `albumart_cache.c` (top level) gets renamed to `art_cache.c` + `recorder/albumart.c` → co-locate under `engine/art/` so the two halves of one feature stop living in different directories.

---

## 7. Opportunities found (beyond the stated goals)

1. **Duplicate JPEG decode.** `recorder/jpeg_load.c` (core, for album art) and `plugins/imageviewer/jpeg` + `jpegp` decode the same format via different code. The image-viewer port is the moment to unify on one core JPEG path.
2. **`gui/dialog_test.c` ships in the release build.** It's in `apps/SOURCES` unconditionally. Drop it and remove it from the settings menu.
3. **Stale plugin fallbacks.** `gui/pitchscreen.c:26`'s `pitch_screen.rock` load and the radio remnants (§3.4) are dead paths that can be swept as part of the framework teardown.
4. **`simple_viewer.c` vs. the core text viewer.** Two "show some text in a scrollable view" implementations coexist (`plugins/lib/simple_viewer.c` and the new `text_viewer/`). Consolidating on the core viewer removes the lib entirely.
5. **`mul_id3` placement.** Multi-file ID3 aggregation is a database/metadata helper wearing a plugin-lib costume; moving it to `engine/database/` makes its real role visible.

---

## 8. Risks & considerations

- **SOURCES churn.** Every move touches `apps/SOURCES` (paths are relative). This is mechanical but large; a single well-reviewed SOURCES rewrite per stage is safer than dribbling changes.
- **Includes from outside `apps/`.** A few `apps/` headers may be `#include`d by `firmware/` or `lib/` (e.g. via `-I apps`). Before each move, grep the *whole* tree for the header name; where an external include exists, keep the include path stable (a one-line forwarding header) rather than editing outside `apps/`. This protects your "keep pulling upstream for other folders" goal.
- **The video viewer is the wildcard.** MPEG decode + A/V sync is a real project. Keeping it *out* of the reorg's critical path (build it last, natively) is the main risk-control lever.
- **Plugin API version coupling.** While plugins still exist mid-migration, the `plugin.h` struct is versioned; converting plugins one at a time means the struct keeps changing. Converting them in a tight cluster (one stage) minimizes the version-bump thrash.
- **`folder_select` equivalence.** The plan assumes core `gui/folder_select.c` can replace `db_folder_select`. If it can't cover both call sites, that plugin needs a real port instead of a delete — a small scope risk to resolve early.

---

## 9. Staged execution plan

Each stage is independently committable and leaves the firmware building and booting.

1. **Sweep dead remnants.** Radio comments, `pitch_screen.rock` fallback, decide `dialog_test.c`. Zero behavior change. *(verify: builds; boots)*
2. **Introduce the taxonomy directories, move the unambiguous files.** `primitives/`, `components/`, `engine/`, `services/`, `platform/` with the files whose placement isn't debatable (viewport, scrollbar, playback, tagcache, iap, keymaps…). Rewrite `apps/SOURCES` once. No renames of *content*, only locations. *(verify: builds; boots; WPS + browser + playback smoke test)*
3. **Dissolve `recorder/`.** Split into `primitives/` and `components/`. *(verify: album art still renders; keyboard entry works)*
4. **Regroup `gui/` into `primitives`/`components`/`scenes`/`engine(skin)`.** *(verify: all listed screens open)*
5. **Convert the small plugins to core:** `playing_time`, `properties`, `credits`; re-point `db_folder_select` at core `folder_select`. *(verify: each menu item opens)*
6. **Switch text-viewer call sites** (legal + track-info) to the core `text_viewer/`; delete `plugins/text_viewer/`, `view_text.c`, `simple_viewer.c`. *(verify: legal screens + long track-info field)*
7. **Port the image viewer to `apps/image_viewer/`;** unify JPEG. Delete `plugins/imageviewer/`. *(verify: View Album Art + opening an image from the browser)*
8. **Move the scrobbler to a core service** started from `main.c`. *(verify: scrobble log written after playback)*
9. **Tear down the framework** (§5.7): delete `plugin.c/.h`, `open_plugin`, `plugins/lib`, `plugin.lds`, configs; replace viewer dispatch with a static table. *(verify: full smoke test; confirm no `.rock` is referenced anywhere)*
10. **(Separate project) Build the video viewer** natively into `apps/video_viewer/`.

Stages 1–4 are pure reorganization (reversible, low risk). Stages 5–9 are the plugin removal. Stage 10 is a distinct effort.

---

## 10. Open questions for you

1. **`db_folder_select` — delete or port?** My read is that core `gui/folder_select.c` replaces it and it can be deleted. Want me to verify the two call sites are fully covered before assuming that, or are you already confident?
2. **Video viewer scope.** Agreed to defer it to a separate post-reorg project (Stage 10)? Or do you want it planned in detail now?
3. **`services/` vs. `engine/` boundary.** A few files are arguable — e.g. `screens.c`, `status.c`, `menu.c`, `root_menu.c` I've placed in `services/`. If you have a strong intuition about where navigation/root-menu code belongs, tell me and I'll align the taxonomy to it.
4. **Naming house-style.** OK to standardize hyphens→underscores in filenames (`statusbar-skinned.c` → `statusbar_skinned.c`, `keymap-ipod.c` → `keymap_ipod.c`)? It's consistent with the codebase's stated convention but does rename a few long-standing files.
```
