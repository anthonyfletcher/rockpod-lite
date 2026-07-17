/* ts_io_rockbox.c -- ts_io over the Rockbox plugin API.
 *
 * Not built by the host Makefile. Add it to the viewer's SOURCES (and drop
 * ts_io_stdio.c) when building in-tree. The whole platform dependency of the
 * engine is this file: everything else needs only <stddef.h>, <stdint.h> and
 * <string.h>.
 *
 * Usage from a plugin:
 *
 *     static ts_rb_file file;              // caller-owned: no malloc here
 *     static ts_source *src;
 *     void *arena;
 *     size_t arena_size = TS_ARENA_RECOMMENDED;
 *     ts_io io;
 *     ts_config cfg = ts_config_default();
 *
 *     arena = plugin_get_buffer(&arena_size);      // or buflib
 *     if (arena_size < TS_ARENA_PLAIN) return PLUGIN_ERROR;
 *     if (ts_io_rockbox(&io, &file, path) != TS_OK) return PLUGIN_ERROR;
 *     if (ts_open(&src, &io, path, arena, arena_size, &cfg) != TS_OK)
 *         return PLUGIN_ERROR;
 *
 * ts_open takes ownership of the fd: ts_close() calls rb->close() for you.
 *
 * On buflib: the arena must not move while a ts_source is open, so either
 * take a non-movable allocation or pin the handle for the lifetime of the
 * source. Nothing inside the engine copes with the ground shifting under it,
 * because every stage holds pointers to its neighbours.
 */

#include "plugin.h"
#include "txt_source.h"

/* Several readers inside the engine share this one descriptor, and each seeks
 * before it reads because none of them can know where the others left it.
 * That makes this layer the only place that can safely skip a seek: it
 * performs every one, so its idea of the position is never stale. Worth
 * doing -- reading a book is almost entirely sequential, and on a target with
 * a spinning disk behind a FAT driver an lseek is not free. */
typedef struct {
    int      fd;
    ts_off_t pos;                            /* -1 when unknown */
} ts_rb_file;

static long rbio_read(void *ctx, void *buf, size_t n)
{
    ts_rb_file *f = ctx;
    long got = rb->read(f->fd, buf, n);

    if (got > 0 && f->pos >= 0) f->pos += got;
    else if (got < 0) f->pos = -1;
    return got;
}

static ts_off_t rbio_seek(void *ctx, ts_off_t off, int whence)
{
    ts_rb_file *f = ctx;
    off_t r;

    if (whence == SEEK_SET && f->pos == off)
        return off;                          /* already there */

    r = rb->lseek(f->fd, (off_t)off, whence);
    f->pos = (r < 0) ? -1 : (ts_off_t)r;
    return (ts_off_t)r;
}

static ts_off_t rbio_size(void *ctx)
{
    ts_rb_file *f = ctx;
    off_t cur = rb->lseek(f->fd, 0, SEEK_CUR);
    off_t end = rb->lseek(f->fd, 0, SEEK_END);

    rb->lseek(f->fd, cur, SEEK_SET);
    f->pos = (cur < 0) ? -1 : (ts_off_t)cur;
    return (ts_off_t)end;
}

static void rbio_close(void *ctx)
{
    ts_rb_file *f = ctx;

    rb->close(f->fd);
    f->fd = -1;
    f->pos = -1;
}

/* `f` must stay valid until ts_close(); a static in the plugin is fine. */
int ts_io_rockbox(ts_io *io, ts_rb_file *f, const char *path)
{
    int fd = rb->open(path, O_RDONLY);

    if (fd < 0) return TS_ERR_IO;
    f->fd = fd;
    f->pos = 0;
    io->read  = rbio_read;
    io->seek  = rbio_seek;
    io->size  = rbio_size;
    io->close = rbio_close;
    io->ctx   = f;
    return TS_OK;
}
