/* was: apps/text_viewer/ts_io_core.c */
/* ts_io_core.c -- ts_io over the core file API.
 *
 * A port of ts_io_rockbox.c with rb->read/lseek/close replaced by the core's
 * own calls; see ts_io_core.h. The whole platform dependency of the engine is
 * this file: everything else needs only <stddef.h>, <stdint.h> and <string.h>.
 *
 * The arena handed to ts_open() must not move while the source is open --
 * every stage holds pointers to its neighbours -- so callers taking it from
 * buflib must pin the handle for the lifetime of the ts_source.
 */

#include "file.h"
#include "ts_io_core.h"

/* Several readers inside the engine share this one descriptor, and each seeks
 * before it reads because none of them can know where the others left it.
 * That makes this layer the only place that can safely skip a seek: it
 * performs every one, so its idea of the position is never stale. Worth
 * doing -- reading a book is almost entirely sequential, and on a target with
 * a spinning disk behind a FAT driver an lseek is not free. */
static long coreio_read(void *ctx, void *buf, size_t n)
{
    struct ts_core_file *f = ctx;
    long got = (long)read(f->fd, buf, n);

    if (got > 0 && f->pos >= 0) f->pos += got;
    else if (got < 0) f->pos = -1;
    return got;
}

static ts_off_t coreio_seek(void *ctx, ts_off_t off, int whence)
{
    struct ts_core_file *f = ctx;
    off_t r;

    if (whence == SEEK_SET && f->pos == off)
        return off;                          /* already there */

    r = lseek(f->fd, (off_t)off, whence);
    f->pos = (r < 0) ? -1 : (ts_off_t)r;
    return (ts_off_t)r;
}

static ts_off_t coreio_size(void *ctx)
{
    struct ts_core_file *f = ctx;
    off_t cur = lseek(f->fd, 0, SEEK_CUR);
    off_t end = lseek(f->fd, 0, SEEK_END);

    lseek(f->fd, cur, SEEK_SET);
    f->pos = (cur < 0) ? -1 : (ts_off_t)cur;
    return (ts_off_t)end;
}

static void coreio_close(void *ctx)
{
    struct ts_core_file *f = ctx;

    close(f->fd);
    f->fd = -1;
    f->pos = -1;
}

int ts_io_core(ts_io *io, struct ts_core_file *f, const char *path)
{
    int fd = open(path, O_RDONLY);

    if (fd < 0) return TS_ERR_IO;
    f->fd = fd;
    f->pos = 0;
    io->read  = coreio_read;
    io->seek  = coreio_seek;
    io->size  = coreio_size;
    io->close = coreio_close;
    io->ctx   = f;
    return TS_OK;
}
