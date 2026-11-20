/* SPDX-License-Identifier: BSD-2-Clause */
/* do - run a command as root (nallox) */
#define _GNU_SOURCE
#include <crypt.h>
#include <errno.h>
#include <grp.h>
#include <pwd.h>
#include <shadow.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <termios.h>
#include <unistd.h>

static void
ask(const char *prompt, char *buf, size_t n)
{
	struct termios t, o;
	int changed = 0;

	fputs(prompt, stderr);
	fflush(stderr);
	if (tcgetattr(0, &o) == 0) {
		t = o;
		t.c_lflag &= ~ECHO;
		tcsetattr(0, TCSAFLUSH, &t);
		changed = 1;
	}
	if (fgets(buf, n, stdin) == NULL) {
		if (changed)
			tcsetattr(0, TCSAFLUSH, &o);
		exit(1);
	}
	buf[strcspn(buf, "\n")] = '\0';
	if (changed) {
		tcsetattr(0, TCSAFLUSH, &o);
		fputs("\n", stderr);
	}
}

static int
allowed(const struct passwd *pw, int *nopass)
{
	FILE *f;
	struct stat st;
	struct group *g;
	char line[256], *w, *rule;
	gid_t groups[64];
	int ng = 64, i, ok = 0;

	if (stat("/etc/run.conf", &st) < 0 || st.st_uid != 0 || (st.st_mode & 022))
		return 0;
	if ((f = fopen("/etc/run.conf", "r")) == NULL)
		return 0;
	getgrouplist(pw->pw_name, pw->pw_gid, groups, &ng);
	if (ng > 64)
		ng = 64;
	while (!ok && fgets(line, sizeof line, f) != NULL) {
		line[strcspn(line, "\n")] = '\0';
		w = strtok(line, " \t");
		if (w == NULL || w[0] == '#' || strcmp(w, "permit") != 0)
			continue;
		*nopass = 0;
		rule = strtok(NULL, " \t");
		if (rule != NULL && strcmp(rule, "nopass") == 0) {
			*nopass = 1;
			rule = strtok(NULL, " \t");
		}
		if (rule == NULL)
			continue;
		if (rule[0] == ':') {
			g = getgrnam(rule + 1);
			if (g != NULL)
				for (i = 0; i < ng; i++)
					if (groups[i] == g->gr_gid)
						ok = 1;
		} else if (strcmp(rule, pw->pw_name) == 0)
			ok = 1;
	}
	fclose(f);
	return ok;
}

int
main(int argc, char **argv)
{
	struct passwd *me;
	struct spwd *sp;
	char pass[128], *enc;
	const char *term;
	int nopass = 0;

	me = getpwuid(getuid());
	if (me == NULL) {
		fprintf(stderr, "run: unknown user\n");
		return 1;
	}
	if (getuid() != 0) {
		if (!allowed(me, &nopass)) {
			fprintf(stderr, "run: not permitted\n");
			return 1;
		}
		if (!nopass) {
			sp = getspnam(me->pw_name);
			ask("run: password: ", pass, sizeof pass);
			enc = (sp != NULL && sp->sp_pwdp[0] == '$') ? crypt(pass, sp->sp_pwdp) : NULL;
			if (enc == NULL || strcmp(enc, sp->sp_pwdp) != 0) {
				sleep(2);
				fprintf(stderr, "run: authentication failed\n");
				return 1;
			}
		}
	}
	term = getenv("TERM");
	if (initgroups("root", 0) < 0 || setgid(0) < 0 || setuid(0) < 0) {
		perror("do");
		return 1;
	}
	clearenv();
	setenv("PATH", "/bin:/sbin:/usr/bin:/usr/sbin", 1);
	setenv("HOME", "/root", 1);
	setenv("USER", "root", 1);
	setenv("LOGNAME", "root", 1);
	setenv("TERM", term != NULL ? term : "linux", 1);
	if (argc < 2)
		execl("/bin/sh", "sh", (char *)NULL);
	else
		execvp(argv[1], argv + 1);
	perror(argc < 2 ? "/bin/sh" : argv[1]);
	return 1;
}
