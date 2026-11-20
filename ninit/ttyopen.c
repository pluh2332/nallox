/* SPDX-License-Identifier: BSD-2-Clause */
/* ttyopen - open a terminal line and run login on it */
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/ioctl.h>

int
main(int argc, char **argv)
{
	char path[64], buf[256];
	int fd;
	FILE *f;

	if (argc < 2) {
		fprintf(stderr, "usage: ttyopen tty\n");
		return 1;
	}
	snprintf(path, sizeof path, "/dev/%s", argv[1]);
	setsid();
	fd = open(path, O_RDWR);
	if (fd < 0) {
		perror(path);
		return 1;
	}
	ioctl(fd, TIOCSCTTY, 0);
	dup2(fd, 0);
	dup2(fd, 1);
	dup2(fd, 2);
	if (fd > 2)
		close(fd);
	if ((f = fopen("/etc/issue", "r")) != NULL) {
		while (fgets(buf, sizeof buf, f) != NULL)
			fputs(buf, stdout);
		fclose(f);
	}
	fflush(stdout);
	execl("/bin/login", "login", (char *)NULL);
	perror("/bin/login");
	return 1;
}
