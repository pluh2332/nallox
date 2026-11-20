/* SPDX-License-Identifier: BSD-2-Clause */
/* n-daemon - run a program in the background, optionally restarting it */
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

static pid_t child;
static volatile sig_atomic_t stopping;

static void
on_term(int sig)
{
	(void)sig;
	stopping = 1;
	if (child > 0)
		kill(child, SIGTERM);
}

int
main(int argc, char **argv)
{
	const char *pidfile = NULL, *logfile = "/dev/null";
	int supervise = 0, i, fd, p[2];
	char c;
	FILE *f;
	time_t t0;

	for (i = 1; i < argc; i++) {
		if (strcmp(argv[i], "-s") == 0)
			supervise = 1;
		else if (strcmp(argv[i], "-p") == 0 && i + 1 < argc)
			pidfile = argv[++i];
		else if (strcmp(argv[i], "-l") == 0 && i + 1 < argc)
			logfile = argv[++i];
		else if (strcmp(argv[i], "--") == 0) {
			i++;
			break;
		} else
			break;
	}
	if (i >= argc) {
		fprintf(stderr, "usage: n-daemon [-s] [-p pidfile] [-l logfile] -- command [args]\n");
		return 1;
	}
	if (pipe(p) < 0)
		return 1;
	switch (fork()) {
	case -1:
		return 1;
	case 0:
		break;
	default:
		close(p[1]);
		while (read(p[0], &c, 1) > 0)
			;
		return 0;
	}
	close(p[0]);
	setsid();
	if (pidfile != NULL && (f = fopen(pidfile, "w")) != NULL) {
		fprintf(f, "%d\n", (int)getpid());
		fclose(f);
	}
	if ((fd = open("/dev/null", O_RDONLY)) >= 0) {
		dup2(fd, 0);
		close(fd);
	}
	if ((fd = open(logfile, O_WRONLY | O_CREAT | O_APPEND, 0644)) >= 0) {
		dup2(fd, 1);
		dup2(fd, 2);
		close(fd);
	}
	close(p[1]);
	if (!supervise) {
		execvp(argv[i], argv + i);
		fprintf(stderr, "n-daemon: %s: %s\n", argv[i], strerror(errno));
		return 127;
	}
	signal(SIGTERM, on_term);
	signal(SIGINT, on_term);
	for (;;) {
		t0 = time(NULL);
		child = fork();
		if (child == 0) {
			signal(SIGTERM, SIG_DFL);
			signal(SIGINT, SIG_DFL);
			execvp(argv[i], argv + i);
			fprintf(stderr, "n-daemon: %s: %s\n", argv[i], strerror(errno));
			_exit(127);
		}
		if (child > 0)
			while (waitpid(child, NULL, 0) < 0 && errno == EINTR)
				;
		child = 0;
		if (stopping)
			break;
		if (time(NULL) - t0 < 2)
			sleep(1);
	}
	return 0;
}
