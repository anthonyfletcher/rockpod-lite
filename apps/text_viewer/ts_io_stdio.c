/* ts_io_stdio.c -- host-side ts_io implementation.
 *
 * The Rockbox equivalent is the same shape over rb->read / rb->lseek /
 * rb->close; see README.md. */

#include "txt_source.h"
#include <stdio.h>

/* Mirrors ts_io_rockbox: the engine's readers share one descriptor and each
 * seeks before reading, so skipping the redundant seeks is this layer's job.
 * It sees every seek and read, so its idea of the position is never stale. */
static ts_off_t io_pos = -1;

static long io_read(void *ctx, void *buf, size_t n)
{
    size_t got = fread(buf, 1, n, (FILE *)ctx);
    if (!got && ferror((FILE *)ctx)) { io_pos = -1; return -1; }
    if (io_pos >= 0) io_pos += (ts_off_t)got;
    return (long)got;
}

static ts_off_t io_seek(void *ctx, ts_off_t off, int whence)
{
    if (whence == SEEK_SET && io_pos == off) return off;
    if (fseek((FILE *)ctx, (long)off, whence) != 0) { io_pos = -1; return -1; }
    io_pos = ftell((FILE *)ctx);
    return io_pos;
}

static ts_off_t io_size(void *ctx)
{
    FILE *f = ctx;
    long cur = ftell(f), end;

    if (fseek(f, 0, SEEK_END) != 0) { io_pos = -1; return -1; }
    end = ftell(f);
    fseek(f, cur, SEEK_SET);
    io_pos = cur;
    return end;
}

static void io_close(void *ctx) { fclose((FILE *)ctx); }

int ts_io_stdio(ts_io *io, const char *path)
{
    FILE *f = fopen(path, "rb");

    if (!f) return TS_ERR_IO;
    io->read = io_read;
    io->seek = io_seek;
    io->size = io_size;
    io->close = io_close;
    io->ctx = f;
    io_pos = 0;
    return TS_OK;
}
