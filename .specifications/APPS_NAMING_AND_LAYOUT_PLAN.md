# `apps/` naming and layout — plan (rev. 1)

Follows `APPS_FILE_INVENTORY.md`, which described every file and flagged the
placements that looked wrong. This is the plan to act on it, extended to cover
naming as well as location.

The test throughout: **what would someone opening this tree for the first time
expect?** Not "what is defensible", but "what would they guess, and be right".

---

## 1. What is actually wrong

Four distinct problems, evidenced rather than asserted.

### 1.1 The word "menu" means five different things

| Where | What it actually is |
|---|---|
| `widgets/menu.c` | the menu *engine* — walks a tree, runs a loop |
| `root_menu.c` | top-level navigation and screen dispatch |
| `screens/menus/*_menu.c` | declarative settings trees (10 files) |
| `screens/context_menu.c` | a dynamically built menu screen |
| `screens/debug_menu.c` | a screen listing debug screens |

A fresh reader sees a folder called `menus/` and reasonably expects every menu
to be in it. Two aren't. And the ten inside it aren't really "menus" in the
same sense as the other two — they're **settings screens** expressed as
declarative trees.

### 1.2 Two pairs of unrelated files share a name

| File | File | Relationship |
|---|---|---|
| `settings/sound_menu.h` | `screens/menus/sound_menu.c` | **none** — the header declares `recording_menu()`/`rectrigger()`, for recording these targets cannot do. It is dead. |
| `playlist/playlist_menu.h` | `screens/menus/playlist_menu.c` | **none** — the header declares `save_playlist_screen()`, a screen. |

### 1.3 `tagtree.c` is in the wrong place

It is not a data layer. Its interface is
`tagtree_load(struct tree_context*)`, `tagtree_enter()`, `tagtree_exit()` —
**the same shape as `browser_files.c`'s `ft_load`/`ft_enter`/`ft_exit`**, and it
makes 19 UI calls (`gui_synclist`, `get_action`, `splash`, `talk_id`) against
`tagcache.c`'s 1.

`tagcache.c` is the database. `tagtree.c` is the **database browser backend** —
the sibling of the filesystem backend, and they belong beside the browser
screen they both serve.

### 1.4 Names that mislead, and a naming language that is not consistent

- **`albumart_cache.c` caches artist art too.** It has `aa_artist_dir`,
  `albumart_cache_artist_fallback()`, an artist silhouette placeholder, and its
  own comment says it covers "both album folders and their parent (artist)
  folders". The name and my inventory description were both wrong. It is an
  **art** cache. (`metadata/albumart.c` by contrast really is album-specific —
  artist appears only inside the `<artist>-<album>` filename pattern.)
- **A file's name should match its public API prefix.** Where they disagree,
  one is wrong: `peakmeter.c` holds `peak_meter_*`; `quickscreen.c` holds
  `quick_screen_*`; `abrepeat.c` holds `ab_repeat_*`. Conversely `tagcache.c`
  holds `tagcache_*` and is fine — so a blanket "split compound words" rule
  would be wrong.
- **Stage 4 created divergence.** Renaming files without renaming functions
  left `browser.c` full of `tree_*`, `browser_files.c` of `ft_*`,
  `context_menu.c` of `onplay_*`, `play_status.c` of `status_*`. These are now
  the worst offenders, and they were self-inflicted.
- **One concept, three spellings:** `usb_screen.c`, `quickscreen.c`,
  `pitchscreen.c`.

---

## 2. The naming rules

Four rules, in priority order. Each is checkable.

**R1 — A file is named after its public API prefix.**
Where they disagree, change whichever is the worse name. `peak_meter_*` is good,
so `peakmeter.c` → `peak_meter.c`. `browser` is a better name than `tree`, so
the functions change instead.

**R2 — One concept, one word, everywhere.**
The vocabulary is fixed and used consistently:

| Concept | Word | Retired |
|---|---|---|
| the file/database browser | `browser` | `tree`, `filetree`, `ft` |
| cover art of any kind | `art` | `albumart` (when it covers both) |
| a screen that edits settings | `settings` | `menu` (for those ten files) |
| the long-press menu | `context_menu` | `onplay` |

**R3 — A file name must stand alone, and a group must be uniform.**

*(Revised. The first draft said "do not repeat the directory in the file name",
which would have given `screens/settings/display.c`. Overruled: repetition is
acceptable, inconsistency is not. A name that only makes sense once you know
which directory you are in is a worse outcome than a redundant suffix.)*

So: keep the suffix, and apply it to **every** member of a group.

| Group | Suffix | Example |
|---|---|---|
| `screens/settings/` | `_settings` | `display_settings.c`, `sound_settings.c`, `general_settings.c` |
| modal overlay screens | `_screen` | `usb_screen.c`, `quick_screen.c`, `pitch_screen.c` |
| everything else | none — the name is already a distinct noun | `wps.c`, `bookmark.c`, `browser.c`, `track_info.c` |

The uniformity is the point: within `screens/settings/` every file ends
`_settings`, with no exceptions to remember. This also removes the duplicate
basenames the first draft would have created (`screens/settings/playlist.c`
against `playlist/playlist.c`).

**R4 — Compounds are only jammed together when they are English words.**
`viewport`, `scrollbar`, `backdrop`, `bookmark`, `keyboard`, `splash` stay.
`peakmeter`, `quickscreen`, `abrepeat` split. `tagcache`/`tagtree` are exempt
under R1 — but `tagtree` is being renamed anyway under R2.

---

## 3. Target layout

```
apps/
  main.c  root_menu.c/.h          boot and top-level navigation

  audio/          the playback pipeline  (+ peak_meter, spectrum_meter)
  database/       tagcache only -- the index and its queries
  playlist/
  metadata/       art_cache, art_sizes, albumart, cuesheet, mul_id3
  files/          filetypes, file_ops -- filesystem services, not screens

  draw/           stateless drawing
  widgets/        reusable controls
  screens/
    browse/       browser, browser_disk, browser_db
    playback/     wps, track_info, quick_screen, pitch
    covers/       album_covers, artist_portraits, carousel
    settings/     the ten settings screens + common
    system/       debug_menu, log_viewer, usb, runtime_info, time_set, alarm
    bookmark.c  shortcuts.c  context_menu.c  main_menu.c  main_menu_config.c
  viewers/        text_viewer/, image_viewer/, properties, playing_time, credits
  skin/           the skin interpreter and skinned chrome

  settings/       the settings model and table
  input/  speech/  system/  iap/  api/
```

`screens/` goes from 20 loose files to five groups of four to ten, plus five
top-level screens that belong to no group.

---

## 4. The moves and renames

### 4.1 Location changes

| From | To | Why |
|---|---|---|
| `database/tagtree.*` | `screens/browse/browser_db.*` | §1.3 — it is a browser backend, not a data layer |
| `screens/browser.*` | `screens/browse/browser.*` | groups the screen with its backends |
| `screens/browser_files.*` | `screens/browse/browser_disk.*` | parallel name with `browser_db` |
| `screens/filetypes.*` | `files/filetypes.*` | a registry called from everywhere, not a screen |
| `screens/fileop.*` | `files/file_ops.*` | operations, not a destination |
| `widgets/peakmeter.*` | `audio/peak_meter.*` | sampled, not called; analyses PCM |
| `widgets/spectrum_meter.*` | `audio/spectrum_meter.*` | same |
| `screens/{wps,track_info,quickscreen,pitchscreen}` | `screens/playback/` | grouping |
| `screens/{album_covers,artist_portraits,carousel}` | `screens/covers/` | grouping |
| `screens/menus/` | `screens/settings/` | they are settings screens (§1.1) |
| `screens/{debug_menu,logfdisp,usb_screen,runtime_info,time_set,alarm}` | `screens/system/` | grouping |
| `screens/menus/main_menu*.c` | `screens/` | the main menu is not a settings screen |

### 4.2 Renames — files

| From | To | Rule |
|---|---|---|
| `metadata/albumart_cache.*` | `metadata/art_cache.*` | R2 — caches album **and artist** art |
| `metadata/albumart_sizes.h` | `metadata/art_sizes.h` | R2 |
| `widgets/peakmeter.*` | `audio/peak_meter.*` | R1/R4 |
| `screens/quickscreen.*` | `screens/playback/quick_screen.*` | R1/R4 |
| `audio/abrepeat.*` | `audio/ab_repeat.*` | R1/R4 |
| `screens/pitchscreen.*` | `screens/playback/pitch.*` | R3 — `screen` repeats the directory |
| `screens/usb_screen.*` | `screens/system/usb_screen.*` | unchanged name; moves only |
| `screens/logfdisp.*` | `screens/system/log_viewer.*` | opaque both ways; names what it is |
| `screens/fileop.*` | `files/file_ops.*` | R4 — `fileop` is a fabricated abbreviation |
| `screens/menus/display_menu.c` | `screens/settings/display_settings.c` | R3 |
| `screens/menus/sound_menu.c` | `screens/settings/sound_settings.c` | R3 |
| `screens/menus/playback_menu.c` | `screens/settings/playback_settings.c` | R3 |
| `screens/menus/theme_menu.c` | `screens/settings/theme_settings.c` | R3 |
| `screens/menus/time_menu.c` | `screens/settings/time_settings.c` | R3 |
| `screens/menus/eq_menu.*` | `screens/settings/eq_settings.*` | R3 |
| `screens/menus/playlist_menu.c` | `screens/settings/playlist_settings.c` | R3 |
| `screens/menus/text_viewer_menu.c` | `screens/settings/text_viewer_settings.c` | R3 |
| `screens/menus/album_covers_menu.c` | `screens/settings/album_covers_settings.c` | R3 |
| `screens/menus/settings_menu.c` | `screens/settings/general.c` | R3 — "settings_menu" in `settings/` is doubly redundant |
| `screens/menus/menu_common.*` | `screens/settings/common_settings.*` | R3 |
| `screens/menus/exported_menus.h` | `screens/settings/exported_settings.h` | R3 |
| `playlist/playlist_menu.h` | `playlist/save_screen.h` | §1.2 — it declares `save_playlist_screen()` |

### 4.3 Renames — functions (removing the divergence)

| Prefix | To | Reach |
|---|---|---|
| `tree_*` | `browser_*` | 9 files use `tree_context` |
| `ft_*` | `browser_disk_*` | 6 functions |
| `tagtree_*` | `browser_db_*` | ~14 functions, ~9 files |
| `onplay*` | `context_menu_*` | 5 functions |
| `status_*` (in `play_status.c`) | `play_status_*` | 3 functions |
| `logfdisplay`/`logfdump` | `log_viewer_show`/`log_viewer_dump` | 2 |

`struct tree_context` → `struct browser_context` travels with `tree_*`.

### 4.4 Deletions

| File | Why |
|---|---|
| `settings/sound_menu.h` | Declares `recording_menu()`/`rectrigger()` for recording these targets cannot do. Nothing includes it. Verify, then delete. |

---

## 5. Consequences to accept

**Duplicate basenames appear.** `screens/settings/playlist.c` alongside
`playlist/playlist.c`; `screens/settings/theme.c` alongside nothing, but
`screens/settings/album_covers.c` alongside `screens/covers/album_covers.c`.
Includes have been path-qualified since stage 2, so this is legal and
unambiguous — but it is a real cost of rule R3, and worth knowing before
committing to it. The alternative is keeping a `_settings` suffix and accepting
the redundancy.

**The function renames are the expensive half.** `tree_*` and `tagtree_*` reach
about a dozen files each. They are mechanical, but they are the part that can
introduce a real bug, so they get their own stages and their own verification.

**Symbol renames change both binaries, harmlessly.** Established in stage 2:
renaming `albumart_cache_*` to `art_cache_*` left all six affected objects with
BYTE-IDENTICAL `.text` sections, but `rockbox.bin` still changed on both
targets -- the linker lays things out differently when symbol names change.
So:

| Stage kind | Valid gate |
|---|---|
| moves and file renames only (3-6) | whole-binary identity on ipodvideo |
| function/symbol renames (7-9) | **per-object `.text` identity** -- whole-binary identity will NOT hold |

Extract a section with
`arm-elf-eabi-objcopy -O binary --only-section=.text foo.o -` and compare.
The stage-1 baseline was re-verified as reproducible across a failed build, so
when the gate fires it is telling you something real.

**ipod6g binaries also change** on every stage that moves or renames a file,
because `cpu_boost()` expands `__FILE__`/`__LINE__` there. ipodvideo is the
byte-identity target; 6g needs the per-object check. See
`REFACTOR_PROPOSAL.md` §9.1.

---

## 6. Staging

Each stage builds both targets, with the §9.2 gate: exit 0 **and** an unchanged
warning set **and** zero implicit declarations.

| # | Stage | Risk |
|---|---|---|
| 1 | Delete dead `sound_menu.h`; rename `playlist_menu.h` → `save_screen.h` | trivial |
| 2 | `metadata/albumart_cache` → `art_cache`, `albumart_sizes` → `art_sizes`, and fix the inventory description | low |
| 3 | Pure file renames that need no function changes: `peak_meter`, `quick_screen`, `ab_repeat`, `pitch`, `usb`, `file_ops` | low |
| 4 | Create `files/`; move `filetypes`, `file_ops`. Move the two meters to `audio/` | low |
| 5 | Subdivide `screens/`: `playback/`, `covers/`, `system/`, and `menus/` → `settings/` with the R3 renames | medium — many files, no logic |
| 6 | Create `screens/browse/`; move `browser`, `browser_files` → `browser_disk`, `tagtree` → `browser_db` | medium |
| 7 | Function renames: `ft_*`, `status_*`, `onplay*`, `logf*` | medium |
| 8 | Function renames: `tree_*` → `browser_*`, `struct tree_context` → `browser_context` | **highest** — widest reach |
| 9 | Function renames: `tagtree_*` → `browser_db_*` | high |
| 10 | Regenerate `APPS_FILE_INVENTORY.md`; update `apps/README` | trivial |

Stages 1–6 are moves and renames of files. Stages 7–9 touch call sites across
the tree and are where a mistake would actually break something; they are last
so the layout is settled before the riskiest work starts.
