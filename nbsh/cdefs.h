#ifndef _SYS_CDEFS_H
#define _SYS_CDEFS_H
#ifdef __cplusplus
#define __BEGIN_DECLS extern "C" {
#define __END_DECLS }
#else
#define __BEGIN_DECLS
#define __END_DECLS
#endif
#define __RCSID(x)
#define __COPYRIGHT(x)
#define __SCCSID(x)
#define __KERNEL_RCSID(n, x)
#define __dead __attribute__((__noreturn__))
#define __printflike(a, b) __attribute__((__format__(__printf__, a, b)))
#define __predict_true(x) __builtin_expect((x) != 0, 1)
#define __predict_false(x) __builtin_expect((x) != 0, 0)
#define __arraycount(a) (sizeof(a) / sizeof((a)[0]))
#endif
