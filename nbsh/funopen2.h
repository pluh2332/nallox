#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
struct fo2 { void *c; ssize_t (*r)(void *, void *, size_t); ssize_t (*w)(void *, const void *, size_t); off_t (*s)(void *, off_t, int); int (*cl)(void *); };
static ssize_t fo2_r(void *p, char *b, size_t n) { struct fo2 *f = p; return f->r ? f->r(f->c, b, n) : -1; }
static ssize_t fo2_w(void *p, const char *b, size_t n) { struct fo2 *f = p; return f->w ? f->w(f->c, b, n) : -1; }
static int fo2_s(void *p, off_t *o, int wh) { struct fo2 *f = p; off_t r; if (!f->s) return -1; r = f->s(f->c, *o, wh); if (r == (off_t)-1) return -1; *o = r; return 0; }
static int fo2_c(void *p) { struct fo2 *f = p; int r = f->cl ? f->cl(f->c) : 0; free(f); return r; }
static FILE *funopen2(const void *cookie, ssize_t (*r)(void *, void *, size_t), ssize_t (*w)(void *, const void *, size_t), off_t (*s)(void *, off_t, int), int (*fl)(void *), int (*cl)(void *))
{
    struct fo2 *f = malloc(sizeof *f);
    cookie_io_functions_t io = { (cookie_read_function_t *)fo2_r, (cookie_write_function_t *)fo2_w, fo2_s, fo2_c };
    (void)fl;
    if (!f) return NULL;
    f->c = (void *)cookie; f->r = r; f->w = w; f->s = s; f->cl = cl;
    return fopencookie(f, r && w ? "r+" : w ? "w" : "r", io);
}
