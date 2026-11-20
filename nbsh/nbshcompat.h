#ifndef NBSHCOMPAT_H
#define NBSHCOMPAT_H
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <limits.h>
#include <locale.h>
#include <errno.h>
#include <signal.h>
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>
#include <pwd.h>
#include <termios.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/ioctl.h>
#include <sys/wait.h>
#define __unused __attribute__((__unused__))
#define __nothing ((void)0)
#define __UNVOLATILE(a) ((void *)(uintptr_t)(volatile void *)(a))
#define F_MAXFD 9999
#define setproctitle(...) ((void)0)
#define CTL_USER 8
#define USER_CS_PATH 1
static inline int sysctl(const int *n, unsigned l, void *o, size_t *ol, const void *nw, size_t nl) { return -1; }
#ifndef timespeccmp
#define timespeccmp(tsp, usp, cmp) (((tsp)->tv_sec == (usp)->tv_sec) ? ((tsp)->tv_nsec cmp (usp)->tv_nsec) : ((tsp)->tv_sec cmp (usp)->tv_sec))
#endif
const char *signalname(int);
int signalnumber(const char *);
int signalnext(int);
const char *getprogname(void);
void *setmode(const char *);
mode_t getmode(const void *, mode_t);
#define MAXBSIZE 65536
#define SYS_NMLN 65
#define SIGINFO SIGPWR
#define LC_C_LOCALE ((locale_t)0)
#define CTL_HW 6
#define HW_MACHINE_ARCH 10
#define DEFFILEMODE (S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH)
#ifndef timespecadd
#define timespecadd(tsp, usp, vsp) do { (vsp)->tv_sec = (tsp)->tv_sec + (usp)->tv_sec; (vsp)->tv_nsec = (tsp)->tv_nsec + (usp)->tv_nsec; if ((vsp)->tv_nsec >= 1000000000L) { (vsp)->tv_sec++; (vsp)->tv_nsec -= 1000000000L; } } while (0)
#endif
#ifndef timespecsub
#define timespecsub(tsp, usp, vsp) do { (vsp)->tv_sec = (tsp)->tv_sec - (usp)->tv_sec; (vsp)->tv_nsec = (tsp)->tv_nsec - (usp)->tv_nsec; if ((vsp)->tv_nsec < 0) { (vsp)->tv_sec--; (vsp)->tv_nsec += 1000000000L; } } while (0)
#endif
void setprogname(const char *);
void *emalloc(size_t);
void *ecalloc(size_t, size_t);
void *erealloc(void *, size_t);
char *estrdup(const char *);
char *estrndup(const char *, size_t);
int reallocarr(void *, size_t, size_t);
intmax_t strtoi(const char *, char **, int, intmax_t, intmax_t, int *);
uintmax_t strtou(const char *, char **, int, uintmax_t, uintmax_t, int *);
#define __weak_alias(a, b)
#define __strong_alias(a, b)
#define _DIAGASSERT(x) ((void)0)
#define __UNCONST(a) ((void *)(uintptr_t)(const void *)(a))
#define __CAST(t, e) ((t)(e))
#define S_ISTXT S_ISVTX
#define __RENAME(x)
#ifndef __STRING
#define __STRING(x) #x
#endif
#define st_mtimespec st_mtim
#define st_atimespec st_atim
#define st_ctimespec st_ctim
#define st_birthtimespec st_ctim
#define st_flags st_rdev
#define UF_NODUMP 0
#define UF_IMMUTABLE 0
#define UF_APPEND 0
#define UF_OPAQUE 0
#define UF_SETTABLE 0
#define SF_ARCHIVED 0
#define SF_IMMUTABLE 0
#define SF_APPEND 0
#define SF_NOUNLINK 0
#define SF_SNAPSHOT 0
#define SF_LOG 0
#define SF_SETTABLE 0
#define _PC_ACL_EXTENDED 9998
#define _PC_ACL_NFS4 9999
#define HN_DECIMAL 0x01
#define HN_NOSPACE 0x02
#define HN_B 0x04
#define HN_DIVISOR_1000 0x08
#define HN_GETSCALE 0x10
#define HN_AUTOSCALE 0x20
int humanize_number(char *, size_t, int64_t, const char *, int, int);
void strmode(mode_t, char *);
const char *user_from_uid(uid_t, int);
const char *group_from_gid(gid_t, int);
char *getbsize(int *, long *);
int chflags(const char *, unsigned long);
int fchflags(int, unsigned long);
int lchflags(const char *, unsigned long);
char *fflagstostr(unsigned long);
#include <sys/sysmacros.h>
#define S_IFWHT 0160000
#define S_ISWHT(m) (((m) & S_IFMT) == S_IFWHT)
#define S_BLKSIZE 512
#define SIZE_T_MAX SIZE_MAX
#define NODEV ((dev_t)-1)
int undelete(const char *);
int lutimens(const char *, const struct timespec *);
long lpathconf(const char *, int);
uint32_t arc4random(void);
char *flags_to_string(unsigned long, const char *);
int fcpxattr(int, int);
#endif
