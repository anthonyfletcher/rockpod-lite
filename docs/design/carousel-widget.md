# Spec: `carousel` widget

Status: draft / proposal
Owner: (you)
Related: `dialog-widget.md`, `ui-refactor-and-layout.md`

## Goal

Extract the reusable machinery inside `apps/gui/album_covers.c` (Cover Flow)
into a standalone **carousel** widget that any "scroll through a row of cover
images" scene can drive via callbacks — the same relationship `gui_synclist`
has with menus/browsers. The immediate payoff is a second consumer: an
**artist** cover flow that reuses 100% of the render/animation/caching code and
only supplies a different data model.

## Non-goals

- Not a general animation framework. It is specifically a horizontal,
  perspective "cover flow" of square thumbnails backed by the album-art cache.
- It does not own the *data model* (what albums/artists exist, sort order),
  the *labels* (album/artist name text), or any menu. Those stay in the scene.
- It does not generate thumbnails. Generation is the album-art cache engine's
  job (`apps/albumart_cache.c`); the carousel is a pure **consumer** of it.

## What the widget owns (migrated out of album_covers.c)

These pieces are currently tangled into `album_covers.c` and move wholesale
into the widget:

- **Slide buffer management** — the buflib context (`buf_ctx`), slide surface
  allocation, and the compaction-safe access pattern.
- **The slide renderer** — `render_slide` and the transposed/column-major
  `pfraw` format (render reads `src[column * sh]`); the perspective/zoom math.
- **The background decode thread** — the "Picture load thread"
  (`PRIORITY_BUFFERING`) that fetches/decodes slides ahead of the selection.
- **Cache consumption** — `load_and_prepare_surface`, `read_aat_transposed`
  (reads a `.aat` row-major thumbnail and transposes it into the column-major
  slide buffer), and the fallback path (`pfraw` / placeholder for an empty or
  not-yet-cached slide).
- **The animation + input loop** — selection index, position/zoom easing,
  wrap behaviour, and the action handling for wheel scroll + select/back.
- **Optional on-slide label** rendering hook (position from a config enum),
  but *not* the label text itself — see callbacks.

## What the scene owns (stays in album_covers.c / artist_covers.c)

- The **index/data model**: building `pf_idx` (the album list), sort order
  (`sort_albums_by_values`, `year_sort_order_values`), and jump-to-selection.
- Resolving a slide index to a **cache key** (the folder identifier / hash the
  album-art cache is keyed by) — this is today's `get_slide_dir` + `mfnv`.
- The **label text** for a slide (album name, or "album — artist").
- Entry/exit UX: the in-screen menu, "rebuild/update cache" actions, settings.

## Proposed interface

Callback + params model, mirroring `gui_synclist`. All lowercase, no struct
typedefs, callback fields are function pointers — house style.

```c
/* apps/gui/widgets/carousel.h */

/* Identifies the source image for one slide. The scene fills this in on
 * demand; the carousel resolves it against the album-art cache. */
struct carousel_slide_src
{
    /* Stable cache key for this slide's folder (the album-art cache is keyed
     * by folder hash). 0 == no art -> carousel draws the placeholder. */
    unsigned long cache_key;
};

/* Scene callbacks. `data` is the opaque pointer handed to carousel_init. */
typedef void carousel_get_slide(int index, void *data,
                                struct carousel_slide_src *out);
typedef const char *carousel_get_label(int index, void *data,
                                       char *buf, size_t buflen);

struct carousel_config
{
    int   cache_size_index;     /* albumart_cache size table index to consume */
    int   label_pos;            /* off / top / bottom (scene's enum, cast)    */
    /* room to grow: background, slide gap, zoom curve, reflection on/off ... */
};

struct carousel;                /* opaque; lives in a scene-provided buffer   */

void carousel_init(struct carousel *c,
                   int num_slides,
                   int initial_index,
                   const struct carousel_config *cfg,
                   carousel_get_slide *get_slide,
                   carousel_get_label *get_label,
                   void *data);

/* Runs the modal carousel loop. Returns the index the user selected, or a
 * negative sentinel (CAROUSEL_CANCELLED) if they backed out. */
int  carousel_run(struct carousel *c);

/* Invalidate + reload after the cache was rebuilt while open. */
void carousel_reload(struct carousel *c);
```

### How a scene uses it

```c
/* album_covers.c (post-refactor sketch) */
static void album_get_slide(int i, void *data, struct carousel_slide_src *out)
{
    struct album_index *idx = data;
    out->cache_key = idx->albums[i].folder_hash;   /* was get_slide_dir+mfnv */
}
static const char *album_get_label(int i, void *data, char *buf, size_t len)
{
    struct album_index *idx = data;
    format_album_label(idx, i, buf, len);           /* album [— artist]      */
    return buf;
}

int album_covers(const char *selected_file)
{
    build_album_index(&idx, selected_file);         /* pf_idx + sort         */
    struct carousel c;
    struct carousel_config cfg = {
        .cache_size_index = albumart_cache_size_index("coverflow"),
        .label_pos        = global_settings.album_covers_show_album_name,
    };
    carousel_init(&c, idx.album_ct, idx.start, &cfg,
                  album_get_slide, album_get_label, &idx);
    int sel = carousel_run(&c);
    ...
}
```

The **artist** cover flow becomes a near-clone that builds an artist index and
returns the artist folder's `cover.jpg` hash as the cache key — no carousel
code changes.

## Cache-key contract (important)

The single hard dependency between carousel and cache is the **key**. Today the
album-art cache stores thumbnails under `thumbcache/<size>/<folderhash>.aat`.
The carousel's `carousel_get_slide` returns exactly that `<folderhash>`; the
carousel does the lookup (`albumart_cache_lookup`) and decode. This keeps the
scene ignorant of file layout and the carousel ignorant of what a "slide" means.

Artist thumbnails (Phase 4 of the DB/thumbnail project) slot in here for free:
the artist scene just hands the carousel the artist folder's hash.

## Open questions

1. **Buffer ownership** — does the scene pass in the working buffer (buflib
   handle) or does `carousel_init` allocate it? Leaning: carousel allocates and
   pins during `carousel_run`, frees on return, so scenes stay simple. Must
   respect the pin/compaction lesson from the skinlist crash.
2. **Reflection / perspective knobs** — expose via `carousel_config` now or
   hard-code to current Cover Flow look? Leaning: hard-code first, add config
   only when the artist flow wants something different.
3. **Empty-library state** — carousel draws placeholder, or scene refuses to
   open? Keep current behaviour (placeholder slide) inside the carousel.

## Migration checklist

1. Create `apps/gui/widgets/carousel.{c,h}`; move `render_slide`, `pfraw`
   format, the load thread, `load_and_prepare_surface`, `read_aat_transposed`,
   buf_ctx management verbatim; add the interface above.
2. Reduce `album_covers.c` to: index build + sort + two callbacks + entry UX.
3. Build & verify Album covers is visually identical + no regression on the
   skinlist/compaction pin fix.
4. Add `artist_covers.c` as the second consumer (Phase 4).
5. Delete the now-dead disabled generation code in `album_covers.c` (the
   `create_albumart_cache` early-return path) — the cache engine owns that now.
