# Spec: UI refactor — folder layout, naming, and migration

Status: draft / proposal
Owner: (you)
Related: `carousel-widget.md`, `dialog-widget.md`

## Goal

Reorganise `apps/gui/` into a legible **widgets vs scenes** structure, adopt
consistent naming (`dialog_yesno`, `dialog_popup`, `dialog_input`,
`coverflow_*`), and migrate the codebase onto the new `carousel` and `dialog`
widgets — all in incremental, buildable steps.

Context: the plugin architecture is being retired (plugins fold into core), so
the frozen `plugin.h` ABI stops being a long-term constraint. During the
transition it still must build, so ABI-facing symbols keep thin wrappers until
plugins are gone.

## Current problem

`apps/gui/` mixes three different kinds of thing in one flat directory with no
naming signal:

- **Draw primitives**: `viewport.c`, `line.c`, `icon.c`, `scrollbar.c`,
  `backdrop.c`
- **Reusable widgets**: `list.c`, `option_select.c`, `color_picker.c`,
  `mask_select.c`, `folder_select.c`, `splash.c`, `yesno.c`, `statusbar*.c`
- **Full-screen scenes**: `wps.c`, `quickscreen.c`, `pitchscreen.c`,
  `usb_screen.c`, `album_covers.c`

...plus `skin_engine/` (its own subsystem). Nothing in a filename tells you
which layer it is, and related things (the three modal dialogs) don't share a
prefix.

## Proposed layout

```
apps/gui/
  primitives/     draw + layout primitives, no policy
    viewport.c  line.c  icon.c  scrollbar.c  backdrop.c
  widgets/        reusable, callback-driven, embeddable UI
    list.c
    dialog.c            (new: modal box primitive)
    dialog_popup.c      (was splash.c)
    dialog_yesno.c      (was yesno.c)
    dialog_input.c      (was recorder/keyboard.c, trimmed)
    carousel.c          (new: extracted from album_covers.c)
    option_select.c  color_picker.c  mask_select.c  folder_select.c
    statusbar.c  statusbar-skinned.c
  scenes/         full-screen flows the user navigates to
    wps.c  quickscreen.c  pitchscreen.c  usb_screen.c
    coverflow_albums.c   (was album_covers.c, data model only)
    coverflow_artists.c  (new, Phase 4)
  skin_engine/    unchanged
```

Top-level `apps/*.c` scenes (`root_menu`, `tree`, `tagtree`, `filetree`,
`menu`, `playlist_viewer`, `bookmark`, `onplay`, `shortcuts`, `screens.c`) are
out of scope for the move for now — they're navigation/glue and moving them is a
larger, separate churn. Revisit once `gui/` is clean.

## Naming conventions

- **Widgets** are named after their type; a family shares a prefix:
  `dialog`, `dialog_popup`, `dialog_yesno`, `dialog_input`, `carousel`, `list`.
- **Scenes** are named after the flow: `coverflow_albums`, `coverflow_artists`,
  `wps`, `quickscreen`, `usb_screen`.
- Public functions match the file: `dialog_yesno()`, `dialog_popupf()`,
  `dialog_input()`, `carousel_run()`. House style otherwise unchanged
  (all-lowercase, `UPPER_CASE` macros/enums, no struct typedefs, brace on new
  line for functions).

## Build-system implications (the risky part)

1. **SOURCES files** (`apps/SOURCES`) are C-preprocessed path lists. Every moved
   file's path must be updated there. This is mechanical but easy to get subtly
   wrong under the target `#ifdef`s — verify by building **all three** configs
   (ipod6g HW, ipodvideo HW, sim) plus **checkwps**.
2. **Include paths.** Lots of files do `#include "gui/list.h"` etc. To minimise
   churn, add the new subdirectories to the compiler include search path (in the
   build config) so bare `#include "list.h"` keeps resolving, rather than
   rewriting every include site. Where includes are already `"gui/xxx.h"`,
   decide once: either keep a compatibility include path or rewrite to
   `"gui/widgets/xxx.h"`. Recommendation: **add include dirs**, rewrite only the
   handful of `gui/`-qualified includes that become ambiguous.
3. **plugin.h / plugin API.** While plugins exist, `splashf`, `kbd_input`,
   `gui_syncyesno_run`, and `gui_synclist*` remain exported. Keep them building
   via wrappers (see dialog spec). Do **not** rename these call-for-call until
   the plugin removal lands.

## Migration order (each step builds & runs)

**Step 0 — pure move, no behaviour change.**
`git mv` the already-clean files into `primitives/`, `widgets/`, `scenes/`;
update `apps/SOURCES` and include paths; rename `album_covers.*` ->
`coverflow_albums.*` and `recorder/keyboard.*` -> `gui/widgets/dialog_input.*`
(content unchanged yet). Build all three configs + checkwps. One reviewable
"move only" commit. Use `git mv` so history follows.

**Step 1 — dialog primitive.**
Add `widgets/dialog.{c,h}`. Reimplement `dialog_popup` (was splash) on it,
keeping `splashf` as a wrapper. Then `dialog_yesno`, then `dialog_input`
(dropping `load_kbd` + the `ucschar_t *kbd` param). Verify popups, yes/no
prompts, and text entry are visually identical. Retires two copies of the
`skin_render_inhibit_flush` workaround.

**Step 2 — carousel widget.**
Add `widgets/carousel.{c,h}`; move the render/thread/cache-consumer machinery
out of `coverflow_albums.c`; reduce that file to index + callbacks. Verify
Album covers unchanged (watch the pin/compaction fix). Delete the disabled
`create_albumart_cache` early-return dead code.

**Step 3 — second carousel consumer.**
Add `scenes/coverflow_artists.c` using the same carousel with an artist index
+ artist-folder `cover.jpg` cache keys (ties into thumbnail-cache Phase 4).

**Step 4 — theming properties.**
Add `struct dialog_style` sourcing from the theme; optionally expose a few
global settings for dialog chrome (border/bg/radius). No skin language.

**Step 5 — post-plugin cleanup (after plugin removal).**
Delete the `splashf`/`kbd_input`/`gui_syncyesno_run` compatibility wrappers;
move call sites to the `dialog_*` names; drop any remaining plugin-ABI
scaffolding around these widgets.

## Risks & mitigations

- **Broken target `#ifdef`s in SOURCES** → build all three configs + checkwps
  after Step 0; keep Step 0 behaviour-free so any breakage is obviously a path
  issue, not logic.
- **Include ambiguity after moves** → prefer added include dirs over mass
  rewrites; grep for `"gui/` includes and fix only the ambiguous ones.
- **Cover Flow regressions** → the carousel extraction must preserve the buflib
  **pin during render** fix (skinlist/compaction crash) and the column-major
  `pfraw`/`.aat` transpose exactly.
- **Plugin build breakage mid-transition** → wrappers stay until plugins are
  gone; don't rename ABI symbols early.
- **History legibility** → `git mv` for every move; keep "move" and "logic"
  changes in separate commits.

## Definition of done

- `apps/gui/` split into `primitives/ widgets/ scenes/ skin_engine/`.
- `splash`/`yesno`/`keyboard` reimplemented as `dialog_popup`/`dialog_yesno`/
  `dialog_input` on one `dialog` primitive; dead keyboard-layout API removed.
- `album_covers` split into `carousel` widget + `coverflow_albums` scene, with
  `coverflow_artists` as a proof-of-reuse second consumer.
- All three build configs + checkwps green at every step.
