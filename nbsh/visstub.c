#include <ctype.h>
int strvis(char *dst, const char *src, int flag) {
	char *d = dst;
	(void)flag;
	for (; *src; src++)
		*d++ = isprint((unsigned char)*src) ? *src : '?';
	*d = 0;
	return (int)(d - dst);
}
