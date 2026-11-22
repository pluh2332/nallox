#include <time.h>
typedef void *timezone_t;
static timezone_t tzalloc(const char *n) { (void)n; return (timezone_t)1; }
static void tzfree(timezone_t z) { (void)z; }
static struct tm *localtime_rz(timezone_t z, const time_t *t, struct tm *tm) { (void)z; return localtime_r(t, tm); }
static time_t mktime_z(timezone_t z, struct tm *tm) { (void)z; return mktime(tm); }
static size_t strftime_z(timezone_t z, char *s, size_t n, const char *f, const struct tm *tm) { (void)z; return strftime(s, n, f, tm); }
