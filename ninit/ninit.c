/* SPDX-License-Identifier: BSD-2-Clause */
/* ninit - nallox PID 1 */
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <sys/ioctl.h>
#include <sys/mount.h>
#include <sys/reboot.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

static pid_t shell_pid;
static time_t shell_started;
static int stopping;

static void
mnt(const char *type, const char *dir, unsigned long flags, const char *opts)
{
	mkdir(dir, 0755);
	if (mount(type, dir, type, flags, opts) < 0 && errno != EBUSY)
		fprintf(stderr, "ninit: mount %s: %s\n", dir, strerror(errno));
}

static void
child_setup(void)
{
	sigset_t none;

	sigemptyset(&none);
	sigprocmask(SIG_SETMASK, &none, NULL);
	setsid();
}

static void
open_console(void)
{
	int fd = open("/dev/console", O_RDWR);

	if (fd < 0)
		return;
	ioctl(fd, TIOCSCTTY, 0);
	dup2(fd, 0);
	dup2(fd, 1);
	dup2(fd, 2);
	if (fd > 2)
		close(fd);
}

/* run a shell script and wait for it; kill it if it takes too long */
static int
run_wait(const char *script, int secs)
{
	pid_t pid, r;
	int i;

	if (access(script, R_OK) != 0)
		return 0;
	pid = fork();
	if (pid < 0)
		return -1;
	if (pid == 0) {
		child_setup();
		open_console();
		execl("/bin/sh", "sh", script, (char *)NULL);
		_exit(127);
	}
	for (i = 0; i < secs * 10; i++) {
		r = waitpid(-1, NULL, WNOHANG);
		if (r == pid)
			return 0;
		if (r <= 0)
			usleep(100000);
	}
	kill(pid, SIGKILL);
	waitpid(pid, NULL, 0);
	fprintf(stderr, "ninit: %s timed out\n", script);
	return -1;
}

static void
start_shell(void)
{
	pid_t pid = fork();

	if (pid == 0) {
		child_setup();
		open_console();
		execl("/sbin/ttyopen", "ttyopen", "console", (char *)NULL);
		execl("/bin/sh", "sh", (char *)NULL);
		_exit(127);
	}
	shell_pid = pid;
	shell_started = time(NULL);
}

static void
stop(int cmd)
{
	int i;

	if (stopping)
		return;
	stopping = 1;
	printf("ninit: shutting down\n");
	fflush(stdout);
	run_wait("/etc/rc.shutdown", 15);
	kill(-1, SIGTERM);
	for (i = 0; i < 50; i++) {
		while (waitpid(-1, NULL, WNOHANG) > 0)
			;
		if (waitpid(-1, NULL, WNOHANG) < 0 && errno == ECHILD)
			break;
		usleep(100000);
	}
	kill(-1, SIGKILL);
	sync();
	mount(NULL, "/", NULL, MS_REMOUNT | MS_RDONLY, NULL);
	reboot(cmd);
}

int
main(void)
{
	sigset_t set;
	int sig;
	pid_t pid;

	if (getpid() != 1) {
		fprintf(stderr, "ninit: must run as PID 1\n");
		return 1;
	}

	sigemptyset(&set);
	sigaddset(&set, SIGCHLD);
	sigaddset(&set, SIGUSR1);
	sigaddset(&set, SIGUSR2);
	sigaddset(&set, SIGTERM);
	sigaddset(&set, SIGINT);
	sigprocmask(SIG_BLOCK, &set, NULL);

	umask(022);
	chdir("/");
	mnt("proc", "/proc", MS_NOSUID | MS_NODEV | MS_NOEXEC, NULL);
	mnt("sysfs", "/sys", MS_NOSUID | MS_NODEV | MS_NOEXEC, NULL);
	mnt("devtmpfs", "/dev", MS_NOSUID, "mode=0755");
	mkdir("/dev/pts", 0755);
	mnt("devpts", "/dev/pts", MS_NOSUID | MS_NOEXEC, "mode=0620,ptmxmode=0666");
	mnt("devpts", "/dev/pts", MS_NOSUID | MS_NOEXEC, "mode=0620,ptmxmode=0666");
	mnt("tmpfs", "/run", MS_NOSUID | MS_NODEV, "mode=0755");
	mnt("tmpfs", "/tmp", MS_NOSUID | MS_NODEV, "mode=1777");

	open_console();
	puts("ninit: starting nallox");
	fflush(stdout);

	run_wait("/etc/rc", 120);
	start_shell();

	for (;;) {
		if (sigwait(&set, &sig) != 0)
			continue;
		switch (sig) {
		case SIGCHLD:
			while ((pid = waitpid(-1, NULL, WNOHANG)) > 0) {
				if (pid == shell_pid && !stopping) {
					if (time(NULL) - shell_started < 5)
						sleep(5);
					start_shell();
				}
			}
			break;
		case SIGUSR1:
			stop(RB_HALT_SYSTEM);
			break;
		case SIGUSR2:
			stop(RB_POWER_OFF);
			break;
		case SIGTERM:
		case SIGINT:
			stop(RB_AUTOBOOT);
			break;
		}
	}
}
