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
`struct dialog_style` of colours/metrics/optional 9-patch, read from the theme.

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
    unsigned border_color;
    int      border_width;
    unsigned box_bg;
    int      corner_radius;     /* 0 == square corners                     */
    int      margin;            /* content inset inside the box            */
    const struct bitmap *bg_9patch;  /* optional; NULL == flat fill+border */
    int      font;              /* FONT_UI by default                      */
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

Deliberately small. A dialog can differ from the theme in: border colour/width,
box background, corner radius, content margin, and an optional 9-patch
background bitmap (`struct screen` already has `nine_segment_bmp`). Everything
else (font, fg/bg text colours) comes from the active theme viewport as today.
No dialog markup language, no per-dialog skin files.

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

## Migration checklist

1. Create `apps/gui/widgets/dialog.{c,h}` with the frame + theme + flush guard
   + loop, and `dialog_style`.
2. Reimplement `dialog_popup` first (lowest risk, no buttons); keep `splashf`
   as a wrapper. Verify splashes unchanged.
3. Reimplement `dialog_yesno`; keep `gui_syncyesno_run*` wrappers.
4. Reimplement `dialog_input` from the current click-wheel editor; drop
   `load_kbd` + the `kbd` param; keep a `kbd_input` wrapper only if plugins
   still need it.
5. Once plugins are gone: delete wrappers, rename call sites to `dialog_*`.
