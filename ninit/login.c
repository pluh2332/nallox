/* SPDX-License-Identifier: BSD-2-Clause */
/* login - minimal login for nallox */
#include <crypt.h>
#include <grp.h>
#include <pwd.h>
#include <shadow.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

static void
ask(const char *prompt, char *buf, size_t n, int echo)
{
	struct termios t, o;
	int changed = 0;

	fputs(prompt, stdout);
	fflush(stdout);
	if (!echo && tcgetattr(0, &o) == 0) {
		t = o;
		t.c_lflag &= ~ECHO;
		tcsetattr(0, TCSAFLUSH, &t);
		changed = 1;
	}
	if (fgets(buf, n, stdin) == NULL)
		exit(1);
	buf[strcspn(buf, "\n")] = '\0';
	if (changed) {
		tcsetattr(0, TCSAFLUSH, &o);
		fputs("\n", stdout);
	}
}

static const char *
hash_for(const struct passwd *pw)
{
	struct spwd *sp;

	if (pw == NULL)
		return "*";
	if (strcmp(pw->pw_passwd, "x") == 0) {
		sp = getspnam(pw->pw_name);
		return sp != NULL ? sp->sp_pwdp : "*";
	}
	return pw->pw_passwd;
}

int
main(void)
{
	char user[64], pass[128], *enc;
	const char *hash, *term;
	struct passwd *pw;
	int tries, ok;

	signal(SIGINT, SIG_IGN);
	term = getenv("TERM");
	for (tries = 0; tries < 3; tries++) {
		ask("nallox login: ", user, sizeof user, 1);
		ask("Password: ", pass, sizeof pass, 0);
		pw = getpwnam(user);
		hash = hash_for(pw);
		ok = 0;
		if (pw != NULL && hash[0] != '*' && hash[0] != '!' && hash[0] != '\0') {
			enc = crypt(pass, hash);
			ok = (enc != NULL && strcmp(enc, hash) == 0);
		}
		if (!ok) {
			sleep(2);
			puts("Login incorrect");
			continue;
		}
		if (initgroups(pw->pw_name, pw->pw_gid) < 0 ||
		    setgid(pw->pw_gid) < 0 || setuid(pw->pw_uid) < 0) {
			perror("login");
			return 1;
		}
		if (chdir(pw->pw_dir) < 0)
			chdir("/");
		clearenv();
		setenv("HOME", pw->pw_dir, 1);
		setenv("SHELL", pw->pw_shell, 1);
		setenv("USER", pw->pw_name, 1);
		setenv("LOGNAME", pw->pw_name, 1);
		setenv("PATH", "/bin:/sbin:/usr/bin:/usr/sbin", 1);
		setenv("TERM", term != NULL ? term : "linux", 1);
		signal(SIGINT, SIG_DFL);
		execl(pw->pw_shell, "-sh", (char *)NULL);
		perror(pw->pw_shell);
		return 1;
	}
	return 1;
}
