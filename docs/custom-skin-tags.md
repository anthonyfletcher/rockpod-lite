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

## Widgets and indicators

### `%Sb(bars[, center])` — spectrum analyser

Draws an audio spectrum analyser filling the current viewport. `bars` is the
number of bands, 1–8 (values outside that range are clamped). Pass `center` as a
second argument to grow the bars from the middle rather than up from the bottom.

```
%V(20,40,120,60,-)%Sb(7)
%V(20,40,120,60,-)%Sb(5, center)
```

### `%La(offset[, nowrap])` — list-item album art

Album art for a menu/list row, for use in list-skinning viewports (alongside the
standard `%LT` list text and `%LI` list icon). `offset` selects the row relative
to the one being drawn (0 = that row); it defaults to 0. Pass `nowrap` to stop
the offset wrapping around the ends of the list. Advanced — only meaningful
inside a list viewport (`%Vi`).

```
%La(0)          # album art of the current list row
```

### `%pP` — playlist progress

Overall progress through the current playlist. Like the other bar tags (`%pb`,
`%bl`): used bare it yields a 0–100 value; used with bar parameters it draws a
bar.

```
%pP                          # value, 0..100
%pP(10,0,100,4,invert)       # drawn as a bar
```

### `%lb` — database / cache building

Non-empty (`"b"`) while the music database or the album-art cache is being built
in the background, otherwise empty. Use as a conditional to show a "busy" glyph:

```
%?lb<...building indicator...>
```

### `%lw` — generic working flag

Non-empty (`"w"`) while a generic "working" flag is set in the firmware. Nothing
in normal playback sets it, so it's mainly for custom builds that call the
internal `ui_set_working()`.

```
%?lw<...busy...>
```

### `%la` — animated spinner frame

A frame index that advances about ten times a second. Combine it with a
conditional that lists the frames; `%la` cycles through however many you provide,
so the spinner animates through ordinary refreshes:

```
%?la<Ð|Ñ|Ò|Ó>        # a 4-frame spinner
```

Best placed in a viewport that is only shown while something is loading.

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

---

## Completeness and source of truth

The tags in the first sections (`%tw`, `%Vw`, `%Vh`, `%sel`, `%wr`, `%ma`, `%sl`,
`%sf`, `%pd`, `%Sb`, `%La`, `%lb`, `%lw`, `%la`) are the fork's dedicated custom
tags, registered together in `apps/skin/custom_tags.c` — that file is the
authoritative list if you're checking for additions. `%pP` is a fork-added tag
that lives with the standard tags in `lib/skin_parser/tag_table.c`.

A few otherwise-standard tags also carry fork enhancements rather than being new
tags — most notably `%ft` (documented above; it also gained file-line and
prefix-search forms). Those aren't listed here beyond `%ft`; consult the Rockbox
skin manual for the standard tags and treat this document as the delta on top.
