# Custom skin tags

Extra skin tags added by this build (RockPod-Lite) on top of the standard
Rockbox skin language. They work in `.wps`, `.sbs` and `.fms` files exactly like
built-in tags.

**These tags are specific to this firmware.** A theme that uses them will not
render correctly on stock Rockbox or on themes.rockbox.org — the tags simply
won't be recognised there. Keep that in mind if you plan to share a theme.

Notation used below: `i` = integer, `s` = string, `t` = a single tag (e.g.
`%it`), `[...]` = any one of, `|` = following arguments optional, `*` = the
preceding group repeats.

---

## Text measurement

### `%tw(text[, fontid])` — text width in pixels

Returns the pixel width of `text` when drawn. With one argument it measures in
the **current viewport's font**; with a second argument it measures in an
explicit loaded font id (the number you pass to `%Fl` / the last field of `%Vl`).

The explicit-font form matters when you decide layout in the viewport-
declaration ($VD) block, which does *not* run in your title's font — pass the
font id so the measurement is correct.

```
# Does the title fit on one 132px line of font 2? Pick single vs wrapped layout.
%?if(%tw(%it,2), >, 132)<%Vd(Title_Wrapped)|%Vd(Title_One_Line)>
```

Returns an integer, so use it inside `%if(...)` with `<`, `>`, `<=`, `>=`, `=`,
`!=`.

### `%Vw` / `%Vh` — current viewport width / height

The current viewport's width / height in pixels, as integers. Handy for writing
one layout that adapts instead of hard-coding pixel numbers.

```
%?if(%tw(%it), >, %Vw)<...too wide...|...fits...>
```

---

## Word wrap

### `%wr(n, text)` — nth word-wrapped line

`n` is a 0-based line index. `%wr` wraps `text` to the **current viewport's
width** (in its font), breaking on spaces, and returns the `n`th resulting line.
A `text` that already fits lands wholly on line 0, so lines 1+ come back empty.

Put each line on its own physical line inside a tall enough viewport:

```
%Vl(Song_Title,180,62,132,60,2)
%wr(0,%it)
%wr(1,%it)
%wr(2,%it)
```

This replaces the old technique of slicing a string with `%ss` at guessed
character positions. `%wr` measures real glyph widths, so the break lands
correctly regardless of the font or which letters the title uses.

> **Layout note.** If your viewport-declaration line ends in a tag that carries
> "no line break" (most commonly `%Vf`), keep the `%wr` lines *below* it on their
> own lines — don't put `%wr(0,...)` on the same physical line as `%Vf`, or the
> no-break will merge the first two output lines together.

---

## Select / case

### `%sel(subject, key1, value1, key2, value2, ..., [default])` — pick by match

Evaluates `subject`, then returns the `value` paired with the first `key` equal
to it. A lone trailing argument (odd one out) is the default when nothing
matches; with no default and no match, `%sel` produces nothing.

Keys and values may each be a literal, a number, or a tag. `%sel` can be nested
(a value may itself be a `%sel`).

```
# Map a list title to an icon glyph, falling back to 'x'.
%sel(%Lt, %Sx(Genre),Ï, %Sx(Album),Î, %Sx(Artist),s, x)
```

This replaces long `%?if(a)<...|%?if(b)<...|...>>` chains that test the *same*
subject over and over: `%sel` names the subject once and short-circuits on the
first match.

> **What `%sel` cannot do:** its values are evaluated for their *text*. Tags that
> act by side effect rather than returning text — notably `%Vd` / `%VI`
> (enable a viewport) — do **not** work as `%sel` values. Keep viewport-selection
> logic (`%?if(mode,=,x)<%Vd(A)|%Vd(B)>`) as ordinary conditionals.

---

## String and arithmetic helpers

### `%sl(text)` — string length

Number of characters (not bytes) in `text`.

```
%?if(%sl(%it), >, 20)<...long title layout...|...short title layout...>
```

### `%sf(haystack, needle)` — find substring

0-based character index of the first occurrence of `needle` in `haystack`, or
`-1` if not found.

```
%?if(%sf(%it, -), >=, 0)<...title contains a hyphen...>
```

### `%pd(n, text)` — pad or truncate to n columns

Returns `text` padded with trailing spaces, or truncated, to exactly `n`
characters — useful for lining up columns in a monospaced layout.

```
%pd(8, %ia): %it
```

### `%ma(a, op, b)` — integer arithmetic

Evaluates `a op b`, where `op` is one of `+ - * / %` (division and remainder by
zero yield 0). `a` and `b` may be numbers or tags.

```
%ma(%Vw, /, 2)                # half the current viewport width
```

---

## Changed behaviour: `%ft` key matching

`%ft(file, key)` (read the text following `key` in a file) now **trims
whitespace around `key`** and **skips the whitespace between the key and its
value**. In practice this means:

- `%ft(prefs, mode:)` and `%ft(prefs, mode: )` behave identically — you no
  longer have to match the file's exact spacing.
- For a line `mode: artist`, either form returns `artist` (no leading space).

If you previously relied on `%ft` returning a value's leading space verbatim
(e.g. reading a line whose value is a single space), that no longer works —
compute spacing in the skin instead.
