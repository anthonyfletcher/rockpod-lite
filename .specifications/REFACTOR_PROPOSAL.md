# `apps/` Refactor — Analysis & Proposal (rev. 2)

**Target hardware:** iPod Video 5G/5.5G (`ipodvideo`, PP5022) and iPod Classic 6G/7G (`ipod6g`, S5L8702). Same 320×240 LCD, same app layer.

**Scope:** the `apps/` tree. `apps/` never pulls from the fork base, so moves and renames inside it are unconstrained by upstream. The only files outside `apps/` this work may touch are `firmware/export/config/ipodvideo.h` and `firmware/export/config/ipod6g.h`. Everything else outside `apps/` — `firmware/`, `lib/`, `tools/`, `bootloader/` — is off limits and must be worked *around*, not edited. §3 enumerates every place that constraint bites and the workaround for each.

**Supersedes:** rev. 1 (2025-07-17). That revision planned the plugin removal as its centrepiece; that work is now finished, and rev. 1's taxonomy is replaced. See §11 for what changed and why.

---

## 1. Executive summary

The plugin system is gone. What's left is the problem rev. 1 identified but never fixed: **`apps/` is flat and its names don't say what the code does.** Ninety-odd loose files at the top level mix the audio engine, the tag database, UI infrastructure, settings, and individual feature screens with nothing to distinguish them. The three subdirectories that exist are each wrong in a different way — `recorder/` is named for hardware these iPods don't have, `gui/` holds five different altitudes in one flat list, and `plugins/` no longer contains a plugin.

Layered on that, a set of file names actively mislead: `misc.c` is 1780 lines spanning eight unrelated subsystems, `screens.c` is three unrelated screens, `tree.c` is the file browser, `onplay.c` is the context menu, `status.c` is three functions about playback state, and `keyboard.h` sits at the root while `keyboard.c` sits in `recorder/`.

This proposal:

1. **Reorganises `apps/` into 16 domain-named directories** (§4), so a directory name states the subject matter and only `main.c` and `root_menu.c` stay loose.
2. **Renames or splits the files whose names don't describe their function** (§5), including breaking `misc.c` and `screens.c` into named parts.
3. **Isolates the five ways the outside world reaches into `apps/`** into one visible, documented surface (§3) instead of leaving them as accidents of the include path.
4. **Records provenance on every moved file** (§2.2) so the reorganisation stays traceable without `git log --follow`.
5. **Sweeps the residue** the plugin removal left behind (§6).

Work is staged (§9) so each stage builds and boots on both targets.

---

## 2. Rules

### 2.1 Scope rules

- **Inside `apps/`, move and rename freely.** No upstream compatibility to preserve.
- **Never edit outside `apps/`,** except `firmware/export/config/ipodvideo.h` and `firmware/export/config/ipod6g.h`. Where an outside file reaches into `apps/`, satisfy it from inside `apps/` (§3).
- **One subject per directory.** The directory name states what the code is *about*.
- **A file name states what the file does.** If you have to open it to find out, rename it.
- **Every stage builds and boots on both targets.** Verified on the build server (§10).

### 2.2 Provenance headers (new rule)

Every file that moves or is renamed gets a provenance line as its **first line**, before the GPL banner:

```c
/* was: apps/gui/statusbar-skinned.c */
#include "config.h"
/***************************************************************************
 *             __________               __   ___.
 ...
```

For a file split into several, each new file records the source:

```c
/* was: apps/misc.c (volume and replaygain section) */
```

Rationale for first-line placement: `head -1` on any file answers "where did this come from", and `grep -rn '^/\* was:'` produces the complete move manifest for review. It is one line and it never needs updating.

These lines are **permanent** (§8.4) — there is no stage that strips them.

Files that stay put do not get one.

---

## 3. What outside `apps/` reaches in — and the workaround for each

This is the binding constraint on the whole plan, so it comes before the design. Five mechanisms; all five are solvable from inside `apps/`.

### 3.1 `tools/root.make` hard-includes five paths

```
tools/root.make:70    $(ROOTDIR)/apps/bitmaps/bitmaps.make
tools/root.make:129   $(APPSDIR)/lang/lang.make
tools/root.make:130   $(APPSDIR)/apps.make
tools/root.make:134   $(APPSDIR)/plugins/bitmaps/pluginbitmaps.make
tools/root.make:135   $(APPSDIR)/plugins/plugins.make
```

A missing `include` (not `-include`) is a fatal make error, so these five paths are **pinned exactly**. `apps/bitmaps/`, `apps/lang/`, and `apps/apps.make` stay where they are — they're correctly named anyway. The two under `apps/plugins/` are the awkward ones (§3.4).

### 3.2 `tools/buildzip.pl` pins four more paths

```
tools/buildzip.pl:497   apps/plugins/viewers.config       -- opened through the preprocessor, dies on failure
tools/buildzip.pl:550   apps/plugins/CATEGORIES           -- open ... or die
tools/buildzip.pl:578   apps/tagnavi.config               -- copied to the zip root
tools/buildzip.pl:590   apps/plugins/rockbox-fonts.config -- copied into rocks/viewers/
```

`buildzip.pl` is deliberately kept byte-identical to upstream (see CLAUDE.md; the fork's packaging lives in `bundle-theme.sh` instead), so all four paths are pinned.

The first two are fatal if missing, and both are already inert — `CATEGORIES` is a comment block, and the extension→viewer map that `viewers.config` used to carry was compiled into `filetypes.c` in commit `a86c1ab3fa`. They stay as tombstones.

The last two are non-fatal (`copy()` without `or die`), but they are real content, so they stay put regardless. **`apps/tagnavi.config` must stay at the `apps/` root** — it cannot move into `database/` as its subject matter would suggest. That's the one place the outside world forces a file to sit somewhere illogical, and it needs a comment saying so.

> Incidentally, `buildzip.pl:579` copies `apps/plugins/disktidy.config`, which this fork deleted with the plugins. That copy fails silently today and has for some time — harmless, and confirmation that non-`die` copies are tolerated.

### 3.2a `lib/rbcodec/codecs/codecs.make` needs `plugin.lds`

```
lib/rbcodec/codecs/codecs.make:168   CODEC_LDS := $(APPSDIR)/plugins/plugin.lds
```

**`apps/plugins/plugin.lds` is still live.** Codecs and plugins shared one linker script upstream; the plugins are gone but the codecs are not, so this 11.7 KB file is still linking every `.codec` in the build. It must stay at exactly that path. Rename it in place and you break the codec link, silently, at package time.

This is the strongest argument for the `plugins/README` in §4.1: the directory's name now actively lies about why its contents exist.

### 3.3 `tools/configure` supplies stale include paths

`tools/configure` sets `APPEXTRA="recorder:gui:radio"` for our targets, which `apps/apps.make` expands into `-Iapps/recorder -Iapps/gui -Iapps/radio`. Note `apps/radio/` **has not existed for some time** — the variable is already stale and harmless.

**Workaround: ignore `APPEXTRA` entirely.** `apps/apps.make` is inside `apps/` and owns `INCLUDES`. Replace:

```make
INCLUDES += -I$(APPSDIR) $(patsubst %,-I$(APPSDIR)/%,$(subst :, ,$(APPEXTRA)))
```

with an explicit, self-documenting list under our control:

```make
# Include path is owned here, not by tools/configure's APPEXTRA (which is
# stale -- it still names apps/radio/, removed long ago). Every apps/
# subdirectory that exports headers is listed explicitly.
INCLUDES += -I$(APPSDIR) -I$(APPSDIR)/api
```

This is the single change that unblocks moving anything. See §4.3 for why the list is this short.

### 3.4 `firmware/` and `lib/` include nine `apps/` headers by bare name

Found by cross-referencing every `#include "…h"` under `firmware/`, `lib/`, and `bootloader/` against the `apps/` header set:

| Header | Included by |
|---|---|
| `settings.h` | `firmware/backlight.c`, `firmware/scroll_engine.c`, `firmware/sound.c`, `firmware/usb.c`, `lib/rbcodec/dsp/{afr,pbe,surround,tdspeed}.c` |
| `misc.h` | `firmware/powermgmt.c`, `firmware/scroll_engine.c`, `firmware/usb.c` |
| `action.h` | `firmware/backlight.c` |
| `splash.h` | `firmware/powermgmt.c` |
| `buffering.h` | `lib/rbcodec/metadata/metadata.c` |
| `fracmul.h` | `lib/rbcodec/dsp/*.c` (10 files) |
| `rbcodecconfig.h` | `lib/rbcodec/codecs/codecs.h`, `lib/rbcodec/dsp/*.c`, `lib/rbcodec/platform.h` |
| `rbcodecplatform.h` | `lib/rbcodec/platform.h` |
| `plugin.h` | `lib/rbcodec/metadata/hes.c` — vestigial; the local file is already an empty 46-line stub |

These nine headers are the **entire contract** between `apps/` and the rest of the firmware. Today that contract is invisible: it exists only as a side effect of `-Iapps` and `-Iapps/gui` happening to find the right files.

**Workaround, and an improvement: `apps/api/`.** Create one directory holding exactly these nine headers, put it on the include path, and document at the top of each that it is consumed from outside `apps/` and therefore cannot move or be renamed. Two shapes are possible:

- **Forwarding stubs** — `apps/api/misc.h` contains `#include "../system/shutdown.h"` etc. Keeps the real header next to its `.c`, at the cost of a level of indirection.
- **The real header lives in `api/`** — no indirection, but the header sits away from its implementation.

**Recommendation: forwarding stubs**, because `misc.h` and `settings.h` will be split (§5.2) and a stub can re-export several new headers behind the one name the outside world still asks for. It also makes the boundary explicit at exactly one place per header.

`plugin.h` gets a stub in `api/` too, with its existing note preserved: deleting `lib/rbcodec/metadata/hes.c:12` would let it go, but `lib/` is out of scope.

---

## 4. Proposed structure

Sixteen directories, named for subject matter. Only `main.c` (startup) and `root_menu.c` (top-level navigation) stay loose, because they are the two files that genuinely belong to no single domain and everything else hangs off.

```
apps/
├── main.c  root_menu.c        the two files that are about the whole app
│
│   ── the device ──
├── audio/       the playback pipeline
├── database/    the tag database
├── playlist/    playlists, in memory and on disk
├── metadata/    per-track data: album art, cuesheets, ID3 aggregation
│
│   ── the interface, by altitude ──
├── draw/        stateless drawing. Knows pixels, knows no features.
├── widgets/     reusable controls. Called by someone; hand control back.
├── screens/     full screens. Navigated to; run their own loop until exit.
├── viewers/     file-format applications. Own model + view + input.
├── skin/        the skin interpreter and skinned chrome
│
│   ── services ──
├── settings/    the settings model and its table
├── input/       buttons, actions, keymaps
├── speech/      voice output and translation
├── system/      shutdown, events, activity stack, paths, formatting
│
│   ── boundaries ──
├── iap/         Apple accessory protocol (unchanged)
├── api/         the nine headers firmware/ and lib/ include (§3.4)
│
│   ── pinned by tools/, not code ──
├── bitmaps/     pinned: tools/root.make:70
├── lang/        pinned: tools/root.make:129, tools/configure:1424
└── plugins/     pinned: tools/root.make:134-135, tools/buildzip.pl:497,550
```

The three UI altitudes carry the design weight, so they get a hard test:

- **`draw/`** — give it a viewport and it puts pixels in it. No input loop, no settings, no knowledge of what it's drawing for.
- **`widgets/`** — `gui_syncyesno_run()` asks a question and returns yes or no. The caller owns the screen; the widget borrows it.
- **`screens/`** — the WPS is navigated *to* and runs until the user leaves. It owns the screen.
- **`viewers/`** — a screen that also owns a file format and a document model. The text viewer is the clearest case: an extraction engine plus a reader UI.

### 4.1 Full file mapping

Every current file, with its destination. Renames are marked; §5 justifies them.

#### `audio/` — the playback pipeline

| From | To |
|---|---|
| `playback.c/.h` | `audio/playback.c/.h` |
| `pcmbuf.c/.h` | `audio/pcmbuf.c/.h` |
| `buffering.c/.h` | `audio/buffering.c/.h` |
| `codec_thread.c/.h` | `audio/codec_thread.c/.h` |
| `codecs.c` | `audio/codecs.c` |
| `audio_thread.c/.h` | `audio/audio_thread.c/.h` |
| `audio_path.c` | `audio/audio_path.c` |
| `voice_thread.c/.h` | `audio/voice_thread.c/.h` |
| `beep.c` | `audio/beep.c` |
| `abrepeat.c/.h` | `audio/abrepeat.c/.h` |
| `rbcodec_helpers.c` | `audio/rbcodec_helpers.c` |
| `status.c/.h` | `audio/play_status.c/.h` **(rename)** |

#### `database/` — the tag database

| From | To |
|---|---|
| `tagcache.c/.h` | `database/tagcache.c/.h` |
| `tagtree.c/.h` | `database/tagtree.c/.h` |
| `tagnavi.config` | **stays at `apps/tagnavi.config`** — pinned by `tools/buildzip.pl:578` (§3.2). Add a comment saying why it isn't in `database/`. |

#### `playlist/`

| From | To |
|---|---|
| `playlist.c/.h` | `playlist/playlist.c/.h` |
| `playlist_catalog.c/.h` | `playlist/catalog.c/.h` **(rename)** |
| `playlist_viewer.c/.h` | `playlist/viewer.c/.h` **(rename)** |
| `playlist_menu.h` | `playlist/playlist_menu.h` |

#### `metadata/` — per-track data

| From | To |
|---|---|
| `recorder/albumart.c/.h` | `metadata/albumart.c/.h` |
| `albumart_cache.c/.h` | `metadata/albumart_cache.c/.h` |
| `albumart_sizes.h` | `metadata/albumart_sizes.h` |
| `cuesheet.c/.h` | `metadata/cuesheet.c/.h` |
| `mul_id3.c/.h` | `metadata/mul_id3.c/.h` |

#### `draw/` — stateless drawing

| From | To |
|---|---|
| `gui/viewport.c/.h` | `draw/viewport.c/.h` |
| `gui/line.c/.h` | `draw/line.c/.h` |
| `gui/scrollbar.c/.h` | `draw/scrollbar.c/.h` |
| `gui/icon.c/.h` | `draw/icon.c/.h` |
| `recorder/icons.c/.h` | `draw/icon_bitmaps.c/.h` **(rename — resolves the `icon.c`/`icons.c` collision)** |
| `recorder/bmp.c/.h` | `draw/bmp.c/.h` |
| `recorder/resize.c/.h` | `draw/resize.c/.h` |
| `recorder/jpeg_load.c/.h` | `draw/jpeg_load.c/.h` |
| `recorder/jpeg_common.h` | `draw/jpeg_common.h` |
| `recorder/jpeg_idct_arm.S` | `draw/jpeg_idct_arm.S` |
| `screen_access.c/.h` | `draw/screen_access.c/.h` |

#### `widgets/` — reusable controls

| From | To |
|---|---|
| `gui/list.c/.h` | `widgets/list.c/.h` |
| `gui/bitmap/list.c` | `widgets/list_render.c` **(rename — it is the renderer, not a second list)** |
| `gui/bitmap/list-skinned.c` | `widgets/list_skinned.c` **(rename)** |
| `gui/dialog.c/.h` | `widgets/dialog.c/.h` |
| `gui/yesno.c/.h` | `widgets/yesno.c/.h` |
| `gui/splash.c/.h` | `widgets/splash.c/.h` |
| `gui/option_select.c/.h` | `widgets/option_select.c/.h` |
| `gui/color_picker.c/.h` | `widgets/color_picker.c/.h` |
| `gui/mask_select.c/.h` | `widgets/mask_select.c/.h` |
| `folder_select.c/.h` | `widgets/folder_select.c/.h` |
| `recorder/keyboard.c` + `keyboard.h` | `widgets/keyboard.c/.h` **(reunites the split pair)** |
| `recorder/peakmeter.c/.h` | `widgets/peakmeter.c/.h` |
| `recorder/spectrum_meter.c/.h` | `widgets/spectrum_meter.c/.h` |
| `menu.c/.h` | `widgets/menu.c/.h` |
| `view_text.c` | `widgets/text_box.c/.h` **(rename + reclassify, §8.3)** |
| `gui/dialog_test.c/.h` | **delete** — see §6.2 |

#### `screens/` — full screens

| From | To |
|---|---|
| `gui/wps.c/.h` | `screens/wps.c/.h` |
| `gui/quickscreen.c/.h` | `screens/quickscreen.c/.h` |
| `gui/pitchscreen.c/.h` | `screens/pitchscreen.c/.h` |
| `gui/usb_screen.c/.h` | `screens/usb_screen.c/.h` |
| `tree.c/.h` | `screens/browser.c/.h` **(rename)** |
| `filetree.c/.h` | `screens/browser_files.c/.h` **(rename)** |
| `filetypes.c/.h` | `screens/filetypes.c/.h` |
| `fileop.c/.h` | `screens/fileop.c/.h` |
| `onplay.c/.h` | `screens/context_menu.c/.h` **(rename)** |
| `bookmark.c/.h` | `screens/bookmark.c/.h` |
| `shortcuts.c/.h` | `screens/shortcuts.c/.h` |
| `alarm_menu.c/.h` | `screens/alarm.c/.h` **(rename)** |
| `screens.c/.h` | **split** → `screens/track_info.c/.h`, `screens/time_set.c/.h`, `screens/runtime_info.c/.h` (§5.2) |
| `debug_menu.c/.h` | `screens/debug_menu.c/.h` |
| `logfdisp.c/.h` | `screens/logfdisp.c/.h` |
| `gui/carousel.c/.h` | `screens/carousel.c/.h` (§8.2) |
| `gui/album_covers.c/.h` | `screens/album_covers.c/.h` (§8.2) |
| `gui/artist_portraits.c` | `screens/artist_portraits.c` (§8.2) |
| `menus/*` | `screens/menus/*` (moved intact; `main_menu_config.c` keeps its name) |

#### `viewers/` — file-format applications

| From | To |
|---|---|
| `text_viewer/` | `viewers/text_viewer/` (whole tree, intact — the `ts_` prefix stays, §5.3) |
| `image_viewer/` | `viewers/image_viewer/` (whole tree, intact) |
| `properties.c/.h` | `viewers/properties.c/.h` |
| `playing_time.c/.h` | `viewers/playing_time.c/.h` |
| `credits.c/.h` | `viewers/credits.c/.h` |

`view_text.c` is **not** a viewer — see §8.3. It becomes `widgets/text_box.c/.h`.

#### `skin/` — the skin interpreter

| From | To |
|---|---|
| `gui/skin_engine/*` | `skin/*` (flattened one level; file names unchanged) |
| `gui/backdrop.c/.h` | `skin/backdrop.c/.h` |
| `gui/statusbar.c/.h` | `skin/statusbar.c/.h` |
| `gui/statusbar-skinned.c/.h` | `skin/statusbar_skinned.c/.h` **(rename)** |

#### `settings/`, `input/`, `speech/`

| From | To |
|---|---|
| `settings.c/.h` | `settings/settings.c/.h` |
| `settings_list.c/.h` | `settings/settings_list.c/.h` |
| `sound_menu.h` | `settings/sound_menu.h` |
| `action.c/.h` | `input/action.c/.h` |
| `keymaps/keymap-ipod.c` | `input/keymap_ipod.c` **(rename)** |
| `core_keymap.c/.h` | `input/core_keymap.c/.h` |
| `usb_keymaps.c/.h` | `input/usb_keymaps.c/.h` |
| `talk.c/.h` | `speech/talk.c/.h` |
| `language.c/.h` | `speech/language.c/.h` |

#### `system/` — cross-cutting glue (the `misc.c` split, §5.2)

| From | To |
|---|---|
| `misc.c/.h` | **split** into six files below |
| — shutdown, `default_event_handler`, car adapter, headphone unplug, `check_bootfile` | `system/shutdown.c/.h` |
| — activity stack, `ui_working` | `system/activity.c/.h` |
| — `strrsplt`, `strip_extension`, `skip_whitespace`, `split_string`, `string_option`, `fix_path_part`, `open_pathfmt`, `open_utf8`, `read_line`, `fast_readline`, `settings_parseline` | `system/strings.c/.h` |
| — `format_time*`, `time_split_units`, sleep timer, `talk_timedate` | `system/format_time.c/.h` |
| — `setvol`, normalized volume, `adjust_volume`, `format_sound_value`, replaygain mode | `system/volume.c/.h` |
| — `hex_to_rgb`, `parse_color`, `core_load_bmp`, `clear_screen_buffer`, `output_dyn_value`, `confirm_delete_yesno`, `warn_on_pl_erase`, `show_search_progress`, `system_sound_play`, `keyclick_*`, `clamp_value_wrap` | `system/app_util.c/.h` |
| `app_buffer.c/.h` | `system/app_buffer.c/.h` (§8.1) |
| `appevents.h` | `system/appevents.h` |
| `applimits.h` | `system/applimits.h` |
| `core_asmdefs.c` | `system/core_asmdefs.c` |
| `fracmul.h` | `system/fracmul.h` (stub in `api/`) |
| `rbcodecconfig.h`, `rbcodecplatform.h` | `system/` (stubs in `api/`) |

#### Unchanged

`iap/`, `bitmaps/`, `lang/`, `README`, `SOURCES`, `apps.make`, `features.txt`.

#### `plugins/` — reduced to tombstones

| File | Fate |
|---|---|
| `plugins.make` | **keep** — pinned by `tools/root.make:135`; reduce to a comment once the `credits.raw` rule moves to `apps.make` (§6.1) |
| `bitmaps/pluginbitmaps.make` | **keep** — pinned by `tools/root.make:134`; already empty |
| `viewers.config` | **keep** — pinned by `tools/buildzip.pl:497` |
| `CATEGORIES` | **keep** — pinned by `tools/buildzip.pl:550` |
| `credits.pl` | **move** → `viewers/credits.pl` (nothing outside `apps/` reads it) |
| `plugin.lds` | **keep — still live.** `lib/rbcodec/codecs/codecs.make:168` links every `.codec` with it (§3.2a). Not a tombstone; load-bearing. |
| `rockbox-fonts.config` | **keep** — copied by `tools/buildzip.pl:590` in a branch that does run for these targets |

Nothing else in `plugins/` survives. Add a `plugins/README` stating that the directory contains no plugin and no plugin code; that `plugin.lds` is the **live codec linker script** and deleting it breaks the codec build; and that the other four files exist solely because `tools/root.make` and `tools/buildzip.pl` name these paths. The whole directory can go the moment `tools/` and `lib/` become editable — and not before.

### 4.2 What this does to the top level

Before: ~95 loose files. After: `main.c`, `root_menu.c`, `SOURCES`, `apps.make`, `features.txt`, `README`, and 16 directories. `tagnavi.config` also stays loose, under protest (§3.2).

### 4.3 Include-path policy

The move is the moment to fix the include style. Today almost every include is a bare basename (`#include "list.h"`, `#include "splash.h"`) resolved by whichever `-I` happens to hit first — which is precisely why the tree is hard to read.

**Policy: all cross-directory includes become path-qualified from the `apps/` root.**

```c
#include "widgets/splash.h"      /* not "splash.h" */
#include "draw/viewport.h"       /* not "viewport.h" */
#include "audio/playback.h"      /* not "playback.h" */
```

Same-directory includes stay bare. This means the include path needs only `-I$(APPSDIR)` and `-I$(APPSDIR)/api` (§3.3), every include states where its target lives, and a moved file's references become impossible to leave stale — the build breaks loudly instead of silently resolving to the wrong header.

1,227 `#include` directives under `apps/` resolve to an `apps/` header. Most need no change: a header at the `apps/` root is *already* root-qualified by its bare name, and same-directory includes stay bare by policy. **The actual stage 2 edit was 259 directives across 95 files** — the ones naming a header in `gui/` or `recorder/`, which only resolved because `APPEXTRA` put those two directories on the search path.

Three classes had to be handled, and the first scan caught only the first:

1. **Bare basenames** (250) — `"splash.h"` → `"gui/splash.h"`.
2. **Slash-bearing paths relative to `gui/`** (9) — `"skin_engine/skin_engine.h"` → `"gui/skin_engine/..."`. Easy to miss because they already contain a `/` and so look qualified. Files directly in `gui/` resolve these relatively and must be left alone; files in `gui/bitmap/` must not.
3. **Angle-bracket includes of project-local headers** (8) — `<icons.h>`, `<bmp.h>`, `<jpeg_load.h>`, and five others. `<>` does not search the including file's directory, so even same-directory cases (`recorder/resize.c` including `<bmp.h>`) broke. All eight were converted to the quoted form; four were breaking the build and four were latent, since an angle include of a local header breaks silently the moment that header moves.

The lesson for stage 3: "qualify the includes" is not one regex over `#include "..."`. Check the angle-bracket form and the already-has-a-slash form too.

---

## 5. Naming

### 5.1 Renames, with justification

| Current | Proposed | Why |
|---|---|---|
| `tree.c` | `screens/browser.c` | It is the file browser. "tree" describes a data structure it doesn't use. |
| `filetree.c` | `screens/browser_files.c` | The browser's filesystem backend, as against `tagtree.c`'s database backend. Pairs the two. |
| `onplay.c` | `screens/context_menu.c` | It is the long-press context menu. "onplay" is a callback name, not a subject. |
| `status.c` | `audio/play_status.c` | Three functions about playback mode. "status" reads as the status bar, which is a different file. |
| `alarm_menu.c` | `screens/alarm.c` | It is the alarm screen, not a menu. |
| `recorder/icons.c` | `draw/icon_bitmaps.c` | Removes the `icon.c` / `icons.c` collision, which currently resolves by include-path luck. |
| `gui/bitmap/list.c` | `widgets/list_render.c` | It renders the list `widgets/list.c` manages. Two files named `list.c` is indefensible. |
| `playlist_catalog.c` | `playlist/catalog.c` | The directory carries the prefix now. |
| `playlist_viewer.c` | `playlist/viewer.c` | Same. |
| `statusbar-skinned.c` | `skin/statusbar_skinned.c` | House style is underscores. |
| `list-skinned.c` | `widgets/list_skinned.c` | Same. |
| `keymaps/keymap-ipod.c` | `input/keymap_ipod.c` | Same. |
| `keyboard.h` + `recorder/keyboard.c` | `widgets/keyboard.{c,h}` | A header and its implementation in different directories is a bug in the layout. |

### 5.2 Splits

**`misc.c` (1780 lines) → six files.** It currently holds eight unrelated subsystems: shutdown and system events, the activity stack, string and path helpers, time formatting, volume and replaygain, colour parsing, keyclick, and a bitmap loader. The mapping is in §4.1. `misc.h` becomes six headers plus one forwarding stub at `api/misc.h` re-exporting the three symbols `firmware/powermgmt.c`, `firmware/scroll_engine.c`, and `firmware/usb.c` actually use.

**`screens.c` (944 lines) → three files.** Three unrelated screens share the file for no reason:
- `browse_id3` / `browse_id3_ex` and their callbacks → `screens/track_info.c`
- `set_time_screen`, `say_time`, `say_number_and_spell` → `screens/time_set.c`
- `view_runtime` and its callbacks → `screens/runtime_info.c`
- `charging_splash` → `system/app_util.c`

**`settings.h`** is split only if the `api/` stub work (§3.4) shows the external consumers need a narrower surface. Investigate at stage 6; do not split speculatively.

**Not split:** `playback.c` (4358), `tagcache.c` (5120), `playlist.c` (4092), `tagtree.c` (3223). These are large but *cohesive* — each is one subsystem, and each is the kind of intricate, stateful code where a split creates more risk than it removes. They move; they don't change. Revisit separately if ever.

### 5.3 Kept deliberately

- **`text_viewer/ts_*`** — the `ts_` (text source) prefix is good namespacing and the one place the tree already got this right. Keep it; it's the pattern the rest should move toward.
- **`image_viewer/decoders/`** — vendored third-party decoders (giflib, tinf, the JPEG code). Vendored code keeps upstream names so it stays diffable against its source.
- **`iap/iap-lingo*.c`** — hyphens retained; these names track Apple's lingo numbering and the set reads better kept uniform than half-converted.

---

## 6. Residue from the plugin removal

The plugin system is gone but left traces worth sweeping in stage 1, before any file moves.

1. **`plugins.make` still owns a non-plugin rule.** It builds `credits.raw` from `docs/CREDITS`, which the *core* `credits.c` `#include`s. Move that rule into `apps.make` (which already carries the `credits.o: credits.raw` dependency), leaving `plugins.make` a pure comment. This also removes the dependency on `ENABLEDPLUGINS=yes` in `tools/root.make:133` — today, if that flag ever went to `no`, `credits.raw` would silently stop being generated.
2. **`gui/dialog_test.c` ships in the release build.** It is unconditionally in `SOURCES` and wired into the main menu at `menus/main_menu.c:414-424`, whose own comment calls it temporary. Delete the file, the menu item, and the include.
3. **`plugin.h`** — already an empty stub kept only for `lib/rbcodec/metadata/hes.c:12`. Becomes `api/plugin.h` with its explanatory comment intact.
4. **`plugin.lds` is *not* residue.** It reads like a leftover and it is not: `lib/rbcodec/codecs/codecs.make:168` still links every codec with it (§3.2a). Left unchecked this is the single most likely thing to get deleted during a cleanup and break the build in a way that surfaces only at package time. Comment it loudly instead.
5. **Radio action enum.** `action.h:184-207` still defines eleven `ACTION_FM_*` constants and `action.c:144-159` still switches on three of them, with `CONFIG_TUNER` commented out in both target configs. Dead, but touching the action enum has ordering implications for keymaps — do it as its own commit with a careful read of `input/keymap_ipod.c`, not folded into a move.

---

## 7. Opportunities noted, not scheduled

Listed for visibility; each is out of scope for this refactor per CLAUDE.md §3, and should be its own decision.

1. **Duplicate JPEG decode.** `recorder/jpeg_load.c` (1812 lines, album art) and `image_viewer/decoders/{jpeg,jpeg_decoder,jpeg81,jpegp,idct}.c` (~3900 lines) decode the same format via different code. The reorganisation puts them in `draw/` and `viewers/image_viewer/` respectively, which makes the duplication visible without acting on it. Unifying is a real project with real risk to album-art rendering.
2. **Vendored TLSF, twice.** `image_viewer/tlsf.c` (869 lines) is a private copy alongside `lib/tlsf/`. Noted previously during the Tier 9 conditional cleanup; still true.
3. **`app_buffer` ownership.** `app_buffer.c` is the ex-plugin scratch buffer. §8 asks where it belongs.
4. **`core_alloc` pinning.** The video-viewer attempt was killed by an unpinned `core_alloc` buffer, and `image_viewer` may carry the same latent bug. Unrelated to layout, but it should not get lost — it is a correctness issue, not a cleanliness one.

---

## 8. Resolved decisions

All four questions from the first draft are settled. Recorded with their consequences.

1. **`app_buffer` → `system/`.** Its role (general-purpose scratch allocation) governs, not where the memory physically sits. `system/app_buffer.c/.h`.
2. **`covers/` folds into `screens/`.** Cover flow is two screens plus the engine they share; it doesn't need its own top level. `screens/carousel.c`, `screens/album_covers.c`, `screens/artist_portraits.c`. Directory count drops from 17 to 16.
3. **`view_text.c` stays — it cannot be deleted, but it is renamed and reclassified.** Investigated: it is *not* a shim onto the core text viewer. It is a self-contained ~300-line word-wrapping scrollable display for a string **already in memory**, with four live call sites — `properties.c:222` directly, plus passed as the `view_text` callback into `browse_id3` from `onplay.c:696`, `playlist_viewer.c:569`, and `properties.c:390-397`.

   `viewers/text_viewer/` cannot take it over: the `ts_*` engine is a *file*-streaming document reader, and its whole IO layer (`ts_io_core.c`) is built on file descriptors. Substituting it would mean either adding an in-memory source to `ts_io`, or writing each tag value out to a temp file to read it back — both cost more than the 300 lines they'd remove, and the second is poor behaviour on a flash device.

   **But** `view_text.c` sitting beside `text_viewer/` is exactly the naming confusion this refactor exists to remove, and it is misfiled: it is called by someone and hands control back, so by §4's own test it is a **widget**, not a viewer. It moves to **`widgets/text_box.c/.h`**.

   The *function* keeps the name `view_text()` for now — it appears in two callback signatures in `screens.h` as well as at the call sites, and renaming it is a separate behaviour-free change better done on its own than smuggled into a move. Follow-up, not scheduled.
4. **Provenance lines are permanent.** No stripping stage. One line, never goes stale, and `grep -rn '^/\* was:'` stays a permanent answer to "where did this come from".

---

## 9. Staged execution plan

Each stage is one or a few commits, builds on both targets, and boots. Stages 1 and 2 carry nearly all the mechanical risk; everything after is small and reviewable.

| # | Stage | Verify |
|---|---|---|
| 1 | **Sweep residue** (§6): move the `credits.raw` rule to `apps.make`, delete `dialog_test`, add the `plugins/README` and the "do not delete" comments on `plugin.lds` and `tagnavi.config`. No moves yet. | builds both; `make zip` still succeeds; codecs link; main menu has no "Dialog tests" |
| 2 | **Own the include path and qualify every include.** Rewrite `apps.make` per §3.3; script the conversion of all cross-directory includes to be path-qualified *against the current layout*; create `api/` with the nine stubs. **No file moves.** This deliberately separates the 1,100-line include churn from the moves, so stage 3's diff is readable. | builds both; byte-compare the resulting binaries against stage 1 — they should be identical |
| 3 | **Move everything**, in one commit per destination directory, adding provenance lines (§2.2) and rewriting `SOURCES` once at the end. Because stage 2 already qualified the includes, each move is a `git mv` plus a mechanical path substitution. | builds both after each directory; boot + WPS + browser + playback smoke test at the end |
| 4 | **Rename the misleading files** (§5.1). Pure rename plus reference update; no content change. | builds both; binaries byte-identical to stage 3 |
| 5 | **Split `screens.c`** (§5.2) into three screens. | track info, time set, and runtime info screens each open |
| 6 | **Split `misc.c`** (§5.2) into six, and narrow the `api/misc.h` stub to what `firmware/` actually needs. | builds both; shutdown, volume, sleep timer, car-adapter mode exercised |
| 7 | **Sweep the radio action enum** (§6.5). Its own commit, keymap read carefully. | builds both; every button in WPS and browser behaves |
| 8 | **Write `apps/README`** describing the sixteen directories and the three-altitude UI rule, so the structure is documented where a reader will find it. | n/a |

Stage 2's byte-identical-binary check is the key safety property: it proves the include rewrite changed nothing semantic before any file moves. Stage 4 gets the same check.

---

## 10. Verification

Build server `ant@192.168.4.159`, confirmed reachable and at `d2687a33c5`:

```bash
ssh ant@192.168.4.159 'cd ~/rockpod-lite/build-cc-6g && make -j$(nproc)'   # iPod Classic 6G/7G
ssh ant@192.168.4.159 'cd ~/rockpod-lite/build-cc-5g && make -j$(nproc)'   # iPod Video 5G/5.5G
```

Per-stage gate: **both targets build clean, with no new warnings.** `SOURCES` is preprocessed, so a file dropped from the build fails silently at link rather than at compile — after each stage, diff the object-file list against the previous stage to confirm nothing vanished.

For the stages claiming byte-identical output (2 and 4), compare the stripped binary, not the zip — the zip embeds a build timestamp.

Boot smoke test per stage 3 and beyond: boot, WPS, file browser, database browser, settings, play a track, album art renders, text viewer, image viewer.

---

## 11. What changed from rev. 1

| rev. 1 | Status now |
|---|---|
| §5 plugin framework removal — the centrepiece, stages 5–9 | **Done.** `plugin.c`, `open_plugin.c`, the 937-line API struct, `plugins/lib/`, and every `.rock` are gone. `plugin.h` is a 46-line stub. |
| §5.2 image viewer → core | **Done.** `apps/image_viewer/`, colour-only, decoders ported. |
| §5.3 text viewer → core | **Done.** `apps/text_viewer/` with the `ts_*` engine. |
| §5.4 `properties`, `playing_time`, `db_folder_select` | **Done.** All three are core files; `folder_select.c` replaced `db_folder_select` as predicted. |
| §5.5 video viewer → core (stage 10) | **Abandoned.** Attempted and removed as too niche; root cause of the failure was an unpinned `core_alloc` buffer. **Dropped from this plan.** |
| §5.6 credits → core | **Done.** `credits.c` + `credits.raw`. |
| §5.7 framework teardown | **Done**, except the files `tools/` and `lib/` pin (§3.2, §3.2a). Note rev. 1 listed `plugin.lds` for deletion; that was wrong — it is the live codec linker script. |
| §4 taxonomy: `primitives`/`components`/`scenes`/`apps`/`engine`/`services`/`platform` | **Replaced.** It nested `apps/apps/`, used jargon that doesn't survive contact with a new reader, and its own §8 admitted the `services`-vs-`engine` boundary was unresolved. Superseded by §4's domain-first layout. |
| §6 naming | **Extended.** Rev. 1 fixed hyphens and two names; this revision also renames the files whose names don't describe them and splits the two grab-bags. |
| — | **New:** provenance headers (§2.2), the external-contract analysis and `api/` (§3), the include-qualification policy (§4.3). |
| — | **New since rev. 1:** `gui/carousel.c` (2683 lines), `gui/album_covers.c`, `gui/artist_portraits.c`, `gui/dialog.c`, `albumart_cache.c` — all unmapped by rev. 1, all placed here. |
