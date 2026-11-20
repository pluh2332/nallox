/* SPDX-License-Identifier: BSD-2-Clause */
/* passwd - change a password (nallox) */
#define _GNU_SOURCE
#include <crypt.h>
#include <errno.h>
#include <fcntl.h>
#include <pwd.h>
#include <shadow.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/random.h>
#include <termios.h>
#include <time.h>
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
check(const char *pw, const char *hash)
{
	char *enc;

	if (hash == NULL || hash[0] == '*' || hash[0] == '!')
		return 0;
	if (hash[0] == '\0')
		return pw[0] == '\0';
	enc = crypt(pw, hash);
	return enc != NULL && strcmp(enc, hash) == 0;
}

int
main(int argc, char **argv)
{
	static const char al[] =
	    "./ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";
	struct passwd *me, *pw;
	struct spwd *sp;
	char old[128], n1[128], n2[128], salt[17], setting[32], line[1024];
	char name[64], newhash[256], *enc, *p1, *p2, *p3;
	unsigned char rnd[16];
	const char *target;
	FILE *in, *out;
	int i, fd, found = 0;
	long days;

	me = getpwuid(getuid());
	if (me == NULL) {
		fprintf(stderr, "passwd: unknown user\n");
		return 1;
	}
	target = argc > 1 ? argv[1] : me->pw_name;
	if (getuid() != 0 && strcmp(target, me->pw_name) != 0) {
		fprintf(stderr, "passwd: only root can change another user's password\n");
		return 1;
	}
	pw = getpwnam(target);
	if (pw == NULL) {
		fprintf(stderr, "passwd: no such user: %s\n", target);
		return 1;
	}
	strlcpy(name, pw->pw_name, sizeof name);
	sp = getspnam(name);
	if (sp == NULL) {
		fprintf(stderr, "passwd: no shadow entry for %s\n", name);
		return 1;
	}
	if (getuid() != 0) {
		ask("Current password: ", old, sizeof old);
		if (!check(old, sp->sp_pwdp)) {
			sleep(2);
			fprintf(stderr, "passwd: incorrect password\n");
			return 1;
		}
	}
	ask("New password: ", n1, sizeof n1);
	ask("Retype new password: ", n2, sizeof n2);
	if (strcmp(n1, n2) != 0) {
		fprintf(stderr, "passwd: passwords do not match\n");
		return 1;
	}
	if (n1[0] == '\0') {
		fprintf(stderr, "passwd: empty password not allowed\n");
		return 1;
	}
	if (getrandom(rnd, sizeof rnd, 0) != (ssize_t)sizeof rnd) {
		fprintf(stderr, "passwd: no random numbers available\n");
		return 1;
	}
	for (i = 0; i < 16; i++)
		salt[i] = al[rnd[i] % 64];
	salt[16] = '\0';
	snprintf(setting, sizeof setting, "$6$%s$", salt);
	enc = crypt(n1, setting);
	if (enc == NULL || enc[0] != '$') {
		fprintf(stderr, "passwd: could not hash the password\n");
		return 1;
	}
	strlcpy(newhash, enc, sizeof newhash);
	days = (long)(time(NULL) / 86400);

	in = fopen("/etc/shadow", "r");
	fd = open("/etc/shadow.new", O_WRONLY | O_CREAT | O_EXCL, 0600);
	if (in == NULL || fd < 0) {
		fprintf(stderr, "passwd: cannot update /etc/shadow (is another passwd running?)\n");
		return 1;
	}
	out = fdopen(fd, "w");
	while (fgets(line, sizeof line, in) != NULL) {
		p1 = strchr(line, ':');
		p2 = p1 ? strchr(p1 + 1, ':') : NULL;
		p3 = p2 ? strchr(p2 + 1, ':') : NULL;
		if (p3 != NULL && (size_t)(p1 - line) == strlen(name) &&
		    strncmp(line, name, p1 - line) == 0) {
			fprintf(out, "%s:%s:%ld%s", name, newhash, days, p3);
			found = 1;
		} else
			fputs(line, out);
	}
	fclose(in);
	fflush(out);
	fsync(fd);
	fclose(out);
	if (!found) {
		unlink("/etc/shadow.new");
		fprintf(stderr, "passwd: %s not found in /etc/shadow\n", name);
		return 1;
	}
	if (rename("/etc/shadow.new", "/etc/shadow") < 0) {
		perror("passwd: rename");
		unlink("/etc/shadow.new");
		return 1;
	}
	fprintf(stderr, "passwd: password updated for %s\n", name);
	return 0;
}
