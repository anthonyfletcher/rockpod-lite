# Spec: `dialog` widget (modal box primitive)

Status: draft / proposal
Owner: (you)
Related: `carousel-widget.md`, `ui-refactor-and-layout.md`

## Goal

Factor the identical "modal box" skeleton that `splash.c`, `yesno.c`, and
`keyboard.c` each reimplement into one **dialog** primitive. The three become
thin *content providers* on top of it:

- `dialog_popup`  — self-closing message box, no buttons   (was `splashf`)
- `dialog_yesno`  — message box with a button row           (was `gui_syncyesno_*`)
- `dialog_input`  — single-line text editor                 (was `kbd_input`)

Theming is done with **properties**, not a render engine (agreed): a small
`struct dialog_style` of colours/metrics plus an optional icon bitmap, read
from the theme. No 9-patch/background image — the frame is drawn, and an
optional icon is inset at the left of the content (see "Icon layout").

## Why this is worth doing

All three already duplicate the same sequence, and two of them independently
carry the *same theme-overdraw workaround* (`skin_render_inhibit_flush`) — the
same class of hazard as the USB-screen flap. Centralising it means a new modal
can't get the theme interaction subtly wrong.

Shared skeleton, verified in the current code:

```
enable theme on parent viewport   (yesno.c:287, keyboard.c:383)
measure content -> size + centre a `box` viewport
fill_viewport + draw_border_viewport
draw content, clipped to the box   (VP_FLAG_VP_SET_CLEAN)
flush ONLY the box region
inhibit skin flush so SBS/backdrop doesn't overdraw   (yesno/keyboard)
run input/timeout loop
viewportmanager_theme_undo
```

Only three things vary: the **interior**, the **dismissal policy**, and the
**result type**.

## Proposed interface

```c
/* apps/gui/widgets/dialog.h */

/* Property-based theming. Populated from the theme/global config by
 * dialog_style_default(); a caller may override individual fields. */
struct dialog_style
{
    unsigned box_border_color; /* default theme foreground color */
    int      box_border_width; /* default 1 */
    unsigned box_fg; /* default theme foreground color */
    unsigned box_bg; /* default theme background color */
    int      box_border_corner_radius;     /* default 0 == square corners */
    int      box_margin;            /* content inset inside the box - default 5 */
    const struct bitmap *icon;  /* optional; inset at left of content, text
                                 * column shifted right and wrapped beside it */
    int      box_font;              /* FONT_UI by default */
    int      button_font;              /* FONT_UI by default */
    int      button_border_width; /* default 1 */
    int      button_border_corner_radius;     /* default 0 == square corners */
    unsigned button_border_color; /* default theme foreground color */
    unsigned button_bg; /* default theme background color */
    unsigned button_fg; /* default theme foreground color */
    unsigned button_border_color_selected /* default theme foreground color */;
    unsigned button_bg_selected /* default theme foreground color */;
    unsigned button_fg_selected /* default theme background color */;
};

void dialog_style_default(struct dialog_style *s);

/* A running dialog. Owns the framed box, theme enable/undo, flush guard, and
 * the event loop. The interior is drawn by the caller's `draw` callback into
 * the content viewport the dialog provides. */
struct dialog;

struct dialog_callbacks
{
    /* Draw the interior into `content` (already set active + clipped). */
    void (*draw)(struct dialog *d, struct screen *s,
                 struct viewport *content, void *data);
    /* Handle one action. Return a DIALOG_* disposition. */
    int  (*on_action)(struct dialog *d, int action, void *data);
};

enum dialog_disposition {
    DIALOG_CONTINUE = 0,   /* keep looping / redraw                        */
    DIALOG_ACCEPT,         /* close, "OK/yes" outcome                      */
    DIALOG_CANCEL,         /* close, "cancel/no" outcome                   */
};

void dialog_init(struct dialog *d,
                 const struct text_message *title,   /* NULL for none      */
                 const struct dialog_style *style,   /* NULL == default    */
                 const struct dialog_callbacks *cb,
                 void *data);

/* Loops until a callback returns ACCEPT/CANCEL, or `timeout_ticks` elapses
 * (TIMEOUT_BLOCK for none). Returns the final disposition. */
int dialog_run(struct dialog *d, int timeout_ticks);
```

## The three concrete dialogs

### `dialog_popup` (was splash)

Message box, no buttons, dismissed by timer or any key. Note `splash.c` is the
one that does *not* enable the theme today (`viewport_set_defaults` only) — the
primitive should support "frame only, no theme SBS" as a style/flag so popup
keeps its lightweight behaviour.

Public verbs stay familiar (huge call-site count — see refactor spec):

```c
void dialog_popupf(int ticks, const char *fmt, ...);   /* replaces splashf */
void dialog_popup_progress(int cur, int total, const char *fmt, ...);
```

### `dialog_yesno` (was gui_syncyesno)

Interior = message text; a button row is the interior's bottom band. The
`w_title` / `w_tmo` variants collapse into one options struct:

```c
struct yesno_opts {
    const struct text_message *title;      /* optional */
    int  timeout_ticks;                    /* TIMEOUT_BLOCK for none */
    enum yesno_res tmo_default;            /* result on timeout */
    const struct text_message *yes_msg;    /* shown on accept, optional */
    const struct text_message *no_msg;
};
enum yesno_res dialog_yesno(const struct text_message *msg,
                            const struct yesno_opts *opts /* NULL == blocking */);
```

### `dialog_input` (was keyboard / kbd_input)

The keyboard is already a click-wheel single-line editor (`keyboard.c:22-29`:
layouts/morse/grid "intentionally gone"). As a `dialog_input` it keeps its
interior (edit line in a clipped sub-viewport + wheel character cycling) and
drops the dead loadable-layout API:

```c
/* NEW: no ucschar_t *kbd, no load_kbd */
int dialog_input(char *buffer, int buflen);
```

Removed as part of this work:
- `int load_kbd(unsigned char *filename)`  — delete (`keyboard.c:91`, header).
- the `ucschar_t *kbd` parameter of `kbd_input` — ignored today (`(void)kbd`).
- the header comment referencing LoadableKeyboardLayouts.

## Theming by properties — scope

Deliberately small. A dialog can differ from the theme.  No dialog markup language, 
no per-dialog skin files.

Where the values come from: `dialog_style_default()` reads the current theme
(same source `viewport_set_defaults` uses) plus, optionally, a few new global
settings if you want user-tunable dialog chrome later. Start with theme-derived
only; add settings if wanted.

## Constraints during transition

- **Plugin ABI (temporary):** while `plugin.h` still exports `splashf`,
  `kbd_input`, `gui_syncyesno_run`, keep those symbols as thin wrappers over
  the new primitives so plugins keep building. Once the plugin architecture is
  removed (your long-term plan) the wrappers can go and call sites move to the
  `dialog_*` names.
- **`text_message`** stays the shared multi-line text payload type.
- The flush-inhibit guard must live *inside* `dialog_run` so no caller has to
  remember it.

## Design review — corrections to the proposal above

Read against the current code, the proposal is directionally right but needs
five corrections before implementation. Each is cited to the source it breaks
on.

1. **`splash` is not loop-driven — the shared core is the frame, not the
   loop.** `splashf` draws a framed box and returns immediately; the *caller*
   sleeps (`splash.c:227-243`). It has no `get_action`, no theme enable, and no
   `skin_render_inhibit_flush`. It is called from inside blocking operations
   that must not pump input. So the genuinely reusable core is the **frame**
   (measure -> centre -> fill -> border -> flush-region); the **`dialog_run`
   input loop is shared only by yesno and keyboard.** Factor the primitive as a
   frame API *plus* a separate `dialog_run`; popup uses the frame only.

2. **`enum dialog_disposition` is too small.** `dialog_yesno` must return
   `YESNO_USB` on `SYS_USB_CONNECTED` (`yesno.c:375-378`) and a timeout default
   that may be YES or NO (`yesno.c:362-367`). CONTINUE/ACCEPT/CANCEL cannot
   carry "aborted by system event" nor express the timeout result. Add a
   `DIALOG_ABORT` disposition (system/USB event) and let the caller supply the
   timeout disposition.

3. **No periodic-redraw hook.** yesno repaints once per second to update the
   countdown (`yesno.c:308-316`). `{draw, on_action}` cannot drive a timed
   redraw. `dialog_run` needs a tick/idle contract (e.g. a `redraw_interval` or
   an `on_tick` callback that can request a redraw).

4. **Measurement is caller-owned.** splash word-wraps to *shrink-fit* the
   content (`splash.c:87-166`); yesno/keyboard use fixed width (display minus
   margin) and fit height to content. The primitive centres and frames a box
   whose size the content provider computes — via a `measure` callback or by
   sizing the box before `dialog_run`. The primitive does not word-wrap.

5. **`struct dialog_style` is mostly speculative — defer it.** Nothing in the
   current code has a configurable border colour/width, corner radius, or
   per-button selected/unselected colours; all three draw with
   `DRMODE_INVERSEVID` fills and theme fg/bg only (`yesno.c:191-194`,
   `splash.c:169-203`, `keyboard.c:277-280`). The mechanical refactor should be
   behaviour-identical and carry **no style struct** (theme fg/bg + the optional
   icon only). Introduce `dialog_style` as its own later phase (Phase 4).

Minor: keyboard is `SCREEN_MAIN`-only (`keyboard.c:344`) while splash/yesno loop
`FOR_NB_SCREENS`; harmless on iPod (`NB_SCREENS==1`) but the primitive's screen
model should be stated. splash's broken-theme colour fallback and `max_width`
artifact locking (`splash.c:48-59,166-186`) stay in the popup layer, not the
primitive.

Plugin ABI facts: `splashf`, `splash_progress`, `splash_progress_set_delay`,
`gui_syncyesno_run`, `kbd_input` are all exported in `plugin.h`
(`212-214, 342, 834`); wrappers must keep exact signatures including
`kbd_input(..., ucschar_t *kbd)`. `load_kbd` is **not** in `plugin.h`, so it can
be deleted from core, but it is still called from ~7 core files (`filetree.c`,
`settings.c`, `tagtree.c`, `onplay.c`, `fileop.c`, `playlist_viewer.c`,
`playlist_catalog.c`), which must be updated.

Structural note: `apps/gui/widgets/` does not exist yet, and the file moves are
owned by `ui-refactor-and-layout.md` (its Step 0). To keep this work
independently reviewable, build `dialog.{c,h}` **in place in `apps/gui/`** and
let the directory reshuffle happen separately.

## Icon layout

No 9-patch or background image. The dialog frame is drawn as today (fill +
border). A style may carry an **optional icon bitmap**, inset at the left of the
content area; the text column is shifted right by the icon width (+ a gap) and
wraps within the remaining width, aligned under itself — not under the icon:

```
+-------------------------+
| [icon]  text here and   |
|         aligned here     |
+-------------------------+
```

Layout rules:
- No icon (`icon == NULL`, the common case): content spans the full padded
  width, exactly as the three dialogs draw today. Zero behaviour change.
- With an icon: reserve `icon->width + gap` on the left; the icon is drawn
  top-aligned to the first text line (or vertically centred in the content
  band — decide once, in Stage 4). Text measurement (gap 4, caller-owned)
  accounts for the reduced column width so wrapping stays correct.
- The icon lives in `struct dialog_style`, so it arrives with the theming phase
  (Stage 4); Stages 1-3 pass no icon and are pixel-identical to today.

## Staged implementation plan

Each stage builds and is verifiable on its own. The build scripts assume macOS
(`sysctl`/`brew`); verification is run by the user on the simulator and device.

**Stage 0 — Lock the contract (design only, no code).**
Fold corrections 1-4 and the screen-model note into a revised `dialog.h`: split
the frame API from `dialog_run`; widen the disposition enum with `DIALOG_ABORT`;
add the tick/redraw hook; make measurement caller-owned; drop `dialog_style`
from the initial interface. Write the on-paper mapping of popup/yesno/input onto
the API. -> verify: each of the three dialogs maps cleanly with no leftover
behaviour.

**Stage 1 — Primitive + `dialog_popup` (lowest risk, no input loop).** *(done)*
Add `apps/gui/dialog.{c,h}` (frame core + theme/flush helpers + `dialog_run`);
wire into `apps/SOURCES`. Reimplement splash's framing on the frame API; keep
`splashf` / `splash_progress` / `splash_progress_set_delay` as wrappers with
unchanged signatures; keep the broken-theme fallback and `max_width` locking in
the popup layer. -> verify (sim + device): splashes and the progress meter are
pixel-identical; nothing that calls `splashf` mid-operation blocks on input.

**Stage 2 — `dialog_yesno` on `dialog_run`.** *(done)*
Move the message + button-row interior onto the run loop; the
`skin_render_inhibit_flush` guard now lives inside `dialog_run`. Collapse the
`w_title`/`w_tmo` variants into `yesno_opts`; keep `gui_syncyesno_run`,
`_w_title`, `_w_tmo`, and `yesno_pop*` as wrappers. -> verify: Yes/No selection,
timeout countdown + default, USB abort, result messages, and talk are unchanged.

**Stage 3 — `dialog_input` from the click-wheel editor.** *(done; insertion defect tracked in `keyboard-gap-caret.md`)*
Reimplement `keyboard.c` on `dialog_run`; drop `load_kbd` and the
`ucschar_t *kbd` param internally; keep a `kbd_input(buffer, buflen, kbd)`
wrapper for the plugin ABI and update the ~7 core `load_kbd`/`kbd_input` call
sites; remove the LoadableKeyboardLayouts header comment. -> verify: text entry,
caret/wheel/backspace behaviour, and the discard-confirm prompt (which itself
calls `dialog_yesno`) all work; plugins still link.

**Stage 4 — Theming properties + optional icon.** *(done)*
Introduce `struct dialog_style` + `dialog_style_default()` sourcing from the
theme, and the optional left-inset icon (see "Icon layout"). This is the home
for the border/radius/button-colour fields the interface above front-loads;
add only the ones actually wanted. -> verify: default style reproduces
Stages 1-3 exactly; an icon and overrides render as specified.

See "Stage 4 output" below for what was actually built.

**Stage 5 — Post-plugin cleanup (after plugin removal).**
Delete the `splashf`/`kbd_input`/`gui_syncyesno_run` wrappers, rename call sites
to `dialog_*`, and (per `ui-refactor-and-layout.md`) move the files into
`apps/gui/widgets/`.

## Stage 0 output — the contract, and how the three map onto it

The revised interface lives in `apps/gui/dialog.h`. It differs from the
"Proposed interface" section above per the design review: the frame is split
from the loop, the disposition enum gains `DIALOG_ABORT`, `on_action` receives
`ACTION_NONE` on idle (so countdown/timeout are content-owned), `measure` gives
the content ownership of geometry, there is no `dialog_style` yet, and the
`use_theme` flag is gone (theme-ness is implied by which entry point you use).
Two entry points:

- `dialog_frame_box()` — fill + border a centred box, return its content
  viewport (optionally inset past a left icon). No theme, no loop.
- `dialog_run()` — theme enable/undo + the skin-flush guard + the get_action
  loop, redrawing only when a callback calls `dialog_request_redraw()`.

**`dialog_popup` (was splash).** Frame path only — no `dialog_run`, no input
loop, so `splashf` stays non-blocking (`splash.c:227-243`). Keeps its own
word-wrap, `max_width` artifact locking, and broken-theme colour fallback in
`dialog_popup.c`; sets up drawmode/foreground, calls `dialog_frame_box()` for
the fill+border+content-vp, draws the wrapped lines (and, for
`dialog_popup_progress`, the scrollbar), flushes, and the caller sleeps. `icon`
NULL until Stage 4.

**`dialog_yesno` (was gui_syncyesno).** `dialog_run` with
`context = CONTEXT_YESNOSCREEN`, `poll_ticks = HZ/2`, `title` from `yesno_opts`,
`margin = 10`. `data` points to a small context struct holding the message,
selection, `tmo_default`, `end_tick`, last-drawn second, and result.
  - `measure` → full-width box (`sw - 2*12`), height = lines + gap + button row
    (+ countdown line), matching `yesno.c:167-188`.
  - `draw` → message via `put_message`, then the Yes/No button row and countdown
    (`yesno.c:113-206`).
  - `on_action` → ACCEPT/CANCEL confirm; PREV/NEXT change selection +
    `dialog_request_redraw`; `ACTION_NONE` bumps the countdown (redraw on second
    change) and, past `end_tick`, splashes "Timeout" and returns the disposition
    for `tmo_default`; USB via `default_event_handler` → stash `YESNO_USB`,
    return `DIALOG_ABORT` (`yesno.c:342-384`). The backlight-off gate stays here.
  The result-message + talk tail and `yesno_pop*` stay in `dialog_yesno.c`;
  `gui_syncyesno_run` / `_w_title` / `_w_tmo` become thin wrappers.

**`dialog_input` (was keyboard).** `dialog_run` with
`context = CONTEXT_KEYBOARD`, `poll_ticks = TIMEOUT_BLOCK`, `title = NULL`,
`margin = 10`. `data` points to the existing `struct kbd_edit`.
  - `measure` → full-width box, height = edit line + gap + button row
    (`keyboard.c:250-274`).
  - `draw` → the clipped, horizontally-scrolled edit line + caret and the
    Cancel/OK row (`keyboard.c:282-338`).
  - `on_action` → wheel/caret/backspace/delete + focus moves, all requesting
    redraw; SELECT/ABORT map to ACCEPT/CANCEL (the discard-confirm prompt calls
    `dialog_yesno`); USB → `DIALOG_ABORT` (`keyboard.c:395-475`). Accept-side
    trim + re-encode stays in `dialog_input.c`.
  `load_kbd` and the `ucschar_t *kbd` param are dropped from the core function;
  a `kbd_input(buffer, buflen, kbd)` wrapper remains for the plugin ABI.

All three map with no leftover behaviour, which closes Stage 0.

## Stage 4 output — theming by properties

`struct dialog_style` lives in `apps/gui/dialog.h` and carries what the proposal
front-loaded: box fg/bg/border colour, border width, corner radius, margin, font,
the optional icon, and the button row's normal/selected colours, border width,
radius and font.

**Colours inherit rather than being sampled from the theme.** Every colour field
defaults to `DIALOG_COLOR_INHERIT` and every font to `DIALOG_FONT_INHERIT`,
resolved at *draw* time to whatever the screen already carries. That is what
makes the default style bit-for-bit identical to Stages 1-3 (the theme's colours,
via the theme-enabled parent viewport) and it is also what keeps the popup's
broken-theme fallback working: it overrides the screen's fg/bg before framing, and
the frame inherits the override. Reading `viewport_set_defaults()` into the style
instead would have quietly diverged wherever an SBS viewport sets its own colours.

**Reaching the style.** `dialog_init()` takes a style (NULL == the default), and
`dialog_set_default_style()` restyles every dialog at once - the popup included,
which has no style parameter of its own. That is the only way to theme popups
without changing `splashf`'s signature, which the plugin ABI still pins.

**Consequences of the icon.** Insetting text past an icon meant the interior had
to be drawn into the *content* viewport rather than the box:
- `dialog_frame_box()` now takes a style, draws the icon, and hands back a content
  viewport already inset by the margin and the icon column (`dialog_get_insets()`
  is exposed so `measure()` callbacks can size a box around a content area).
- The `draw` callback receives that content viewport; yes/no and text input
  dropped their own `YN_PAD`/`KBD_PAD` insets, which the style's `box_margin`
  (default 10, the same value) now owns.
- The popup word-wraps to the narrowed column and grows the box by the icon's
  width, so an icon never sits under the first line of text. It pins
  `box_margin` to its own `RECT_SPACING`, since its hand-rolled layout assumes it.
- Both dialogs' near-identical button renderers collapsed into
  `dialog_draw_button()`, which is where the button style fields are read.

**Rounded corners** are drawn by a small pair of helpers in `dialog.c` (a
per-row-span fill and an outline whose inner edge shares the outer corner's
centre). Radius 0 short-circuits to plain `fillrect`/`drawrect`, so the default
path is unchanged. There is no anti-aliasing.

Icon vertical alignment: **top-aligned** to the first text line, per "Icon
layout".
