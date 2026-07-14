# Plan: gap-caret text insertion (`dialog_input`)

Status: **implemented** in `apps/recorder/keyboard.c`; builds clean, not yet
verified on device (see "Verification")
Related: `dialog-widget.md` (Stage 3)

## The defect

The click-wheel editor is a pure **overtype** model. The caret always sits *on* an
existing character (`text[caret]`) and the wheel replaces that character in place
(`wheel_step`, `apps/recorder/keyboard.c`). The line only ever grows at the end,
via `caret_right()` appending a space. Nothing anywhere opens a gap.

Consequences:

- Move the caret back into the text and the wheel **overwrites** the character
  under it. A character cannot be added between two existing ones.
- A space is just another charset entry, so "insert a space" has the same
  problem: wheeling to `' '` replaces rather than splits.
- The only workaround is retyping the whole tail: `helo` -> `hello` means
  overtyping `o`->`l`, pressing Right to append a space at the end, then wheeling
  it to `o`. O(tail) wheel spins, unusable beyond a couple of characters.

Deletion, by contrast, works anywhere (hold-Left backspaces, hold-Right deletes).
The model is asymmetric: characters can be removed anywhere but only added at the
end.

## The fix: caret between characters

`caret` becomes an insertion point in `0..len` — a bar *between* characters, not a
block *on* one. A new `editing` flag means "the character at `caret-1` is the one
the wheel is currently composing".

- **Wheel, `editing` false:** insert `charset[0]` (space) at the caret, advance the
  caret past it, set `editing`, then apply the step. One forward click therefore
  yields `A` immediately; one click back from there yields a space. This is what
  makes a mid-string space possible.
- **Wheel, `editing` true:** cycle `text[caret-1]` in place, exactly as
  `wheel_step` does today, acceleration included.
- **Left / Right:** move the caret one gap and clear `editing`, so moving away
  commits the character and the next spin inserts a fresh one. Right at the end of
  the line does nothing — growing the line is the wheel's job now, which removes
  the append-a-space hack in `caret_right()`.
- **Hold-Left / hold-Right:** backspace the character before the bar / delete the
  character after it, as today, plus clearing `editing`.
- **Empty is a legal state.** `ensure_nonempty()` and the auto-appended trailing
  space at open both go away. Opening puts the bar at the end of the existing text
  with nothing selected.

## Drawing

- Bar caret: a 2px `fillrect` at the gap.
- The character being composed keeps today's inverse-video block.

The two shapes then state honestly what the wheel will do: a block means "I will
change this character", a bar means "I will insert a new one".

The scroll-to-keep-the-caret-visible math needs a guard for `caret == len`, which
today would read one past the end of the text.

## Unchanged

- Accept: trim leading/trailing spaces, re-encode UTF-8.
- Focus/button row, discard-confirm prompt, `dialog_run` wiring, `kbd_measure`.
- **No keymap change.** Insertion becomes the wheel's job, so `action.h` and
  `keymap-ipod.c` are untouched.

## Files

- `apps/recorder/keyboard.c` — edit model, draw routine, and the interaction
  comment at the top of the file (it currently documents the overtype behaviour).
- `docs/design/dialog-widget.md` — the Stage 3 mapping paragraph.

## Verification

Insert mid-word; insert a space mid-string; backspace/delete at both ends; empty
the line completely; long-text horizontal scrolling; Cancel-while-dirty (the
discard prompt, which routes through `dialog_yesno`).

## As built — three points the spec left open

- **Inserting the blank.** The spec says "insert `charset[0]` (space)". The code
  inserts a literal `' '` instead: `build_charset()` guarantees a space is *in*
  the cycle but not that it is *first*, so a localized charset whose first entry
  isn't a space would otherwise insert the wrong character. With the default
  charset (space at index 0) the behaviour is identical — one forward click still
  yields `A`.
- **Wheel at maximum length.** `wheel_step()` returns `false` and changes nothing
  when the line is full, so a no-op spin doesn't set `dirty` and can't provoke a
  spurious discard prompt on Cancel.
- **Left/Right at the ends of the line.** They clear `editing` even when the bar
  cannot move, so a tap always commits the character being composed. Right at the
  end of the line is otherwise a no-op, as specified.

## Accepted cost, and the escape hatch

Repairing an existing character becomes backspace-then-wheel rather than a single
spin. If that proves annoying in practice, **hold-SELECT**
(`BUTTON_SELECT|BUTTON_REPEAT`) is free on the iPod keymap and is the natural home
for a "re-edit the character before the caret" action — flip `editing` back on
without deleting, restoring one-spin repair. Deliberately deferred until the plain
gap caret has been used.
