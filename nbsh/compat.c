#include <signal.h>
#include <string.h>
static const struct { const char *name; int num; } sigtab[] = {
#define S(n) { #n, SIG##n },
S(HUP) S(INT) S(QUIT) S(ILL) S(TRAP) S(ABRT) S(BUS) S(FPE) S(KILL) S(USR1)
S(SEGV) S(USR2) S(PIPE) S(ALRM) S(TERM) S(CHLD) S(CONT) S(STOP) S(TSTP)
S(TTIN) S(TTOU) S(URG) S(XCPU) S(XFSZ) S(VTALRM) S(PROF) S(WINCH) S(IO) S(SYS)
{ 0, 0 } };
const char *signalname(int n) {
	for (int i = 0; sigtab[i].name; i++)
		if (sigtab[i].num == n) return sigtab[i].name;
	return 0;
}
int signalnumber(const char *s) {
	for (int i = 0; sigtab[i].name; i++)
		if (strcmp(sigtab[i].name, s) == 0) return sigtab[i].num;
	return 0;
}
int signalnext(int n) {
	for (int i = n + 1; i < 65; i++)
		if (signalname(i)) return i;
	return 0;
}
#include <errno.h>
#include <sys/types.h>
static const char *nb_prog;
void setprogname(const char *p) {
	const char *s = strrchr(p, '/');
	nb_prog = s ? s + 1 : p;
}
const char *getprogname(void) { return nb_prog ? nb_prog : program_invocation_short_name; }
#include <errno.h>
#include <inttypes.h>
#include <stdlib.h>
int reallocarr(void *p, size_t n, size_t s) {
	void **pp = p, *q;
	size_t sz;
	if (n == 0 || s == 0) { free(*pp); *pp = NULL; return 0; }
	if (__builtin_mul_overflow(n, s, &sz)) return EOVERFLOW;
	q = realloc(*pp, sz);
	if (q == NULL) return ENOMEM;
	*pp = q;
	return 0;
}
intmax_t strtoi(const char *nptr, char **endptr, int base, intmax_t lo, intmax_t hi, int *rstatus) {
	int serrno = errno, st, dummy;
	char *ep;
	intmax_t rv;
	if (endptr == NULL) endptr = &ep;
	if (rstatus == NULL) rstatus = &dummy;
	errno = 0;
	rv = strtoimax(nptr, endptr, base);
	st = errno;
	errno = serrno;
	if (st == EINVAL) { *rstatus = EINVAL; return 0; }
	if (st == ERANGE) { *rstatus = ERANGE; return rv < 0 ? lo : hi; }
	if (rv < lo) { *rstatus = ERANGE; return lo; }
	if (rv > hi) { *rstatus = ERANGE; return hi; }
	if (*endptr == nptr) *rstatus = ECANCELED;
	else if (**endptr != '\0') *rstatus = ENOTSUP;
	else *rstatus = 0;
	return rv;
}
uintmax_t strtou(const char *nptr, char **endptr, int base, uintmax_t lo, uintmax_t hi, int *rstatus) {
	int serrno = errno, st, dummy;
	char *ep;
	uintmax_t rv;
	if (endptr == NULL) endptr = &ep;
	if (rstatus == NULL) rstatus = &dummy;
	errno = 0;
	rv = strtoumax(nptr, endptr, base);
	st = errno;
	errno = serrno;
	if (st == EINVAL) { *rstatus = EINVAL; return 0; }
	if (st == ERANGE) { *rstatus = ERANGE; return hi; }
	if (rv < lo) { *rstatus = ERANGE; return lo; }
	if (rv > hi) { *rstatus = ERANGE; return hi; }
	if (*endptr == nptr) *rstatus = ECANCELED;
	else if (**endptr != '\0') *rstatus = ENOTSUP;
	else *rstatus = 0;
	return rv;
}
#include <pwd.h>
#include <grp.h>
#include <stdio.h>
const char *user_from_uid(uid_t uid, int nouser) {
	static char buf[32];
	struct passwd *pw = getpwuid(uid);
	if (pw) return pw->pw_name;
	if (nouser) return 0;
	snprintf(buf, sizeof buf, "%u", (unsigned)uid);
	return buf;
}
const char *group_from_gid(gid_t gid, int nogroup) {
	static char buf[32];
	struct group *gr = getgrgid(gid);
	if (gr) return gr->gr_name;
	if (nogroup) return 0;
	snprintf(buf, sizeof buf, "%u", (unsigned)gid);
	return buf;
}
int chflags(const char *p, unsigned long f) { (void)p; (void)f; return 0; }
int fchflags(int fd, unsigned long f) { (void)fd; (void)f; return 0; }
int lchflags(const char *p, unsigned long f) { (void)p; (void)f; return 0; }
char *fflagstostr(unsigned long f) { (void)f; return strdup(""); }
#include <errno.h>
int undelete(const char *p) { (void)p; errno = ENOSYS; return -1; }
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/random.h>
#include <stdint.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
int lutimens(const char *p, const struct timespec ts[2]) { return utimensat(AT_FDCWD, p, ts, AT_SYMLINK_NOFOLLOW); }
long lpathconf(const char *p, int n) { return pathconf(p, n); }
uint32_t arc4random(void) { uint32_t x = 0; (void)getrandom(&x, sizeof x, 0); return x; }
char *flags_to_string(unsigned long f, const char *def) { (void)f; return strdup(def); }
int fcpxattr(int a, int b) { (void)a; (void)b; return 0; }
void *acl_get_file(const char *p, int t) { (void)p; (void)t; errno = ENOTSUP; return 0; }
void *acl_get_link_np(const char *p, int t) { (void)p; (void)t; errno = ENOTSUP; return 0; }
void *acl_get_fd_np(int fd, int t) { (void)fd; (void)t; errno = ENOTSUP; return 0; }
int acl_set_file(const char *p, int t, void *a) { (void)p; (void)t; (void)a; errno = ENOTSUP; return -1; }
int acl_set_link_np(const char *p, int t, void *a) { (void)p; (void)t; (void)a; errno = ENOTSUP; return -1; }
int acl_set_fd_np(int fd, void *a, int t) { (void)fd; (void)a; (void)t; errno = ENOTSUP; return -1; }
int acl_free(void *a) { (void)a; return 0; }
int acl_is_trivial_np(void *a, int *triv) { (void)a; if (triv) *triv = 1; return 0; }
