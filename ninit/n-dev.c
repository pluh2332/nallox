
/*
* n-dev cuz i dont got nothing to load any drivers using ninit, unfortunately
* License: BSD-2-Clause
*/
#define _GNU_SOURCE
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/uio.h>
#include <sys/utsname.h>
#include <dirent.h>
#include <elf.h>
#include <errno.h>
#include <fcntl.h>
#include <fnmatch.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#ifndef MODULE_INIT_COMPRESSED_FILE
#define MODULE_INIT_COMPRESSED_FILE 4
#endif

struct nladdr { unsigned short family, pad; unsigned int pid, groups; };
struct mod { char *name, *path, *deps, **dep; int ndep, tried; };
struct alias { char *pat, *mod; size_t plen; };

static struct mod *mods;
static int nmods, capmods, skipped;
static struct alias *aliases;
static int naliases, capalias;
static char moddir[512];

static void *xrealloc(void *p, size_t n)
{
    p = realloc(p, n);
    if (!p) { fputs("n-dev: out of memory\n", stderr); exit(1); }
    return p;
}

static char *xstrdup(const char *s)
{
    char *p = strdup(s);
    if (!p) { fputs("n-dev: out of memory\n", stderr); exit(1); }
    return p;
}

static void norm(char *s)
{
    for (; *s; s++)
        if (*s == '-') *s = '_';
}

static int endswith(const char *s, const char *suf)
{
    size_t a = strlen(s), b = strlen(suf);
    return a >= b && !strcmp(s + a - b, suf);
}

static char *slurp(const char *path, size_t *len)
{
    int fd = open(path, O_RDONLY | O_CLOEXEC);
    struct stat st;
    char *buf;
    size_t got = 0;
    ssize_t r;

    if (fd < 0) return NULL;
    if (fstat(fd, &st) < 0) { close(fd); return NULL; }
    buf = xrealloc(NULL, st.st_size + 1);
    while (got < (size_t)st.st_size) {
        r = read(fd, buf + got, st.st_size - got);
        if (r <= 0) break;
        got += r;
    }
    close(fd);
    buf[got] = 0;
    if (len) *len = got;
    return buf;
}

static char *modname(const char *path)
{
    const char *b = strrchr(path, '/');
    char *n, *e;

    b = b ? b + 1 : path;
    n = xstrdup(b);
    if ((e = strstr(n, ".ko"))) *e = 0;
    norm(n);
    return n;
}

static int addmod(const char *path)
{
    struct mod *m;

    if (nmods == capmods) {
        capmods = capmods ? capmods * 2 : 1024;
        mods = xrealloc(mods, capmods * sizeof *mods);
    }
    m = &mods[nmods];
    memset(m, 0, sizeof *m);
    m->path = xstrdup(path);
    m->name = modname(path);
    return nmods++;
}

static int findmod(const char *name)
{
    int i;

    for (i = 0; i < nmods; i++)
        if (!strcmp(mods[i].name, name)) return i;
    return -1;
}

/* ---- loading ---- */

static int readindex(void)
{
    char p[600], *buf, *line, *sv, *colon, *tok, *sv2;
    struct mod *m;

    snprintf(p, sizeof p, "%s/modules.dep", moddir);
    if (!(buf = slurp(p, NULL))) return -1;
    for (line = strtok_r(buf, "\n", &sv); line; line = strtok_r(NULL, "\n", &sv)) {
        if (!(colon = strchr(line, ':'))) continue;
        *colon = 0;
        m = &mods[addmod(line)];
        for (tok = strtok_r(colon + 1, " ", &sv2); tok; tok = strtok_r(NULL, " ", &sv2)) {
            m->dep = xrealloc(m->dep, (m->ndep + 1) * sizeof *m->dep);
            m->dep[m->ndep++] = tok;
        }
    }
    return 0;
}

static int readaliases(void)
{
    char p[600], *buf, *line, *sv, *pat, *sp;
    struct alias *a;

    snprintf(p, sizeof p, "%s/modules.alias", moddir);
    if (!(buf = slurp(p, NULL))) return -1;
    for (line = strtok_r(buf, "\n", &sv); line; line = strtok_r(NULL, "\n", &sv)) {
        if (strncmp(line, "alias ", 6)) continue;
        pat = line + 6;
        if (!(sp = strchr(pat, ' '))) continue;
        *sp = 0;
        if (naliases == capalias) {
            capalias = capalias ? capalias * 2 : 4096;
            aliases = xrealloc(aliases, capalias * sizeof *aliases);
        }
        a = &aliases[naliases++];
        a->pat = pat;
        a->mod = sp + 1;
        norm(a->mod);
        a->plen = strcspn(pat, "*?[");
    }
    return 0;
}

static int insert(const char *rel)
{
    char p[1100];
    long r;
    int fd, e, flags = 0;

    snprintf(p, sizeof p, "%s/%s", moddir, rel);
    if ((fd = open(p, O_RDONLY | O_CLOEXEC)) < 0) {
        fprintf(stderr, "n-dev: %s: %s\n", p, strerror(errno));
        return -1;
    }
    if (endswith(rel, ".xz") || endswith(rel, ".zst") || endswith(rel, ".gz"))
        flags = MODULE_INIT_COMPRESSED_FILE;
    r = syscall(SYS_finit_module, fd, "", flags);
    e = errno;
    close(fd);
    if (r == 0) { printf("n-dev: loaded %s\n", rel); fflush(stdout); return 0; }
    if (e == EEXIST) return 0;
    fprintf(stderr, "n-dev: %s: %s\n", rel, strerror(e));
    return -1;
}

static void loadmod(int i)
{
    struct mod *m = &mods[i];
    int d;

    if (m->tried) return;
    m->tried = 1;
    for (d = m->ndep - 1; d >= 0; d--)
        insert(m->dep[d]);
    insert(m->path);
}

static void handle(const char *modalias)
{
    int i, m;
    struct alias *a;

    for (i = 0; i < naliases; i++) {
        a = &aliases[i];
        if (strncmp(modalias, a->pat, a->plen)) continue;
        if (fnmatch(a->pat, modalias, 0)) continue;
        if ((m = findmod(a->mod)) >= 0) loadmod(m);
    }
}

static void scan(const char *dir)
{
    DIR *d = opendir(dir);
    struct dirent *e;
    char p[1024], *s;
    unsigned char t;
    struct stat st;

    if (!d) return;
    while ((e = readdir(d))) {
        if (e->d_name[0] == '.' && (!e->d_name[1] || (e->d_name[1] == '.' && !e->d_name[2])))
            continue;
        snprintf(p, sizeof p, "%s/%s", dir, e->d_name);
        t = e->d_type;
        if (t == DT_UNKNOWN) {
            if (lstat(p, &st) < 0) continue;
            t = S_ISDIR(st.st_mode) ? DT_DIR : S_ISREG(st.st_mode) ? DT_REG : 0;
        }
        if (t == DT_DIR) {
            scan(p);
        } else if (t == DT_REG && !strcmp(e->d_name, "modalias")) {
            if ((s = slurp(p, NULL))) {
                s[strcspn(s, "\r\n")] = 0;
                if (*s) handle(s);
                free(s);
            }
        }
    }
    closedir(d);
}

static int openuevent(void)
{
    struct nladdr a = { AF_NETLINK, 0, 0, 1 };
    int s = socket(AF_NETLINK, SOCK_DGRAM | SOCK_CLOEXEC, 15);
    int sz = 1 << 21;

    if (s < 0) return -1;
#ifdef SO_RCVBUFFORCE
    if (setsockopt(s, SOL_SOCKET, SO_RCVBUFFORCE, &sz, sizeof sz) < 0)
#endif
        setsockopt(s, SOL_SOCKET, SO_RCVBUF, &sz, sizeof sz);
    if (bind(s, (struct sockaddr *)&a, sizeof a) < 0) { close(s); return -1; }
    return s;
}

static void listen_loop(int s)
{
    char buf[8192], *p, *end;
    const char *act, *ma;
    struct nladdr from;
    struct iovec iov;
    struct msghdr mh;
    ssize_t n;

    for (;;) {
        memset(&mh, 0, sizeof mh);
        iov.iov_base = buf;
        iov.iov_len = sizeof buf - 1;
        mh.msg_name = &from;
        mh.msg_namelen = sizeof from;
        mh.msg_iov = &iov;
        mh.msg_iovlen = 1;
        if ((n = recvmsg(s, &mh, 0)) < 0) {
            if (errno == EINTR) continue;
            if (errno == ENOBUFS) { scan("/sys/devices"); continue; }
            perror("n-dev: recvmsg");
            exit(1);
        }
        if (from.pid != 0) continue;    /* only the kernel */
        buf[n] = 0;
        end = buf + n;
        act = ma = NULL;
        for (p = buf + strlen(buf) + 1; p < end; p += strlen(p) + 1) {
            if (!strncmp(p, "ACTION=", 7)) act = p + 7;
            else if (!strncmp(p, "MODALIAS=", 9)) ma = p + 9;
        }
        if (act && ma && !strcmp(act, "add")) handle(ma);
    }
}

/* ---- depmod ---- */

static void walk(const char *rel)
{
    char dir[1024], r[1024];
    DIR *d;
    struct dirent *e;
    struct stat st;
    unsigned char t;

    if (*rel) snprintf(dir, sizeof dir, "%s/%s", moddir, rel);
    else snprintf(dir, sizeof dir, "%s", moddir);
    if (!(d = opendir(dir))) return;
    while ((e = readdir(d))) {
        if (e->d_name[0] == '.' && (!e->d_name[1] || (e->d_name[1] == '.' && !e->d_name[2])))
            continue;
        if (*rel) snprintf(r, sizeof r, "%s/%s", rel, e->d_name);
        else snprintf(r, sizeof r, "%s", e->d_name);
        t = e->d_type;
        if (t == DT_UNKNOWN) {
            snprintf(dir, sizeof dir, "%s/%s", moddir, r);
            if (lstat(dir, &st) < 0) continue;
            t = S_ISDIR(st.st_mode) ? DT_DIR : S_ISREG(st.st_mode) ? DT_REG : 0;
        }
        if (t == DT_DIR) walk(r);
        else if (t == DT_REG) {
            if (endswith(e->d_name, ".ko")) addmod(r);
            else if (strstr(e->d_name, ".ko.")) skipped++;
        }
    }
    closedir(d);
}

static int modinfo(struct mod *m, FILE *fa)
{
    char path[1100];
    int fd, i, rc = -1;
    struct stat st;
    size_t sz;
    unsigned char *b;

    snprintf(path, sizeof path, "%s/%s", moddir, m->path);
    if ((fd = open(path, O_RDONLY | O_CLOEXEC)) < 0) return -1;
    if (fstat(fd, &st) < 0) { close(fd); return -1; }
    sz = st.st_size;
    b = mmap(NULL, sz, PROT_READ, MAP_PRIVATE, fd, 0);
    close(fd);
    if (b == MAP_FAILED) return -1;
    if (sz > sizeof(Elf64_Ehdr) && !memcmp(b, ELFMAG, SELFMAG) && b[EI_CLASS] == ELFCLASS64) {
        Elf64_Ehdr *eh = (Elf64_Ehdr *)b;
        if (eh->e_shoff && eh->e_shoff + (size_t)eh->e_shnum * sizeof(Elf64_Shdr) <= sz
            && eh->e_shstrndx < eh->e_shnum) {
            Elf64_Shdr *sh = (Elf64_Shdr *)(b + eh->e_shoff);
            Elf64_Shdr *str = &sh[eh->e_shstrndx];
            if (str->sh_offset + str->sh_size <= sz) {
                for (i = 0; i < eh->e_shnum; i++) {
                    const char *nm, *p, *end;
                    size_t l;
                    if (sh[i].sh_name >= str->sh_size) continue;
                    nm = (const char *)b + str->sh_offset + sh[i].sh_name;
                    if (strcmp(nm, ".modinfo")) continue;
                    if (sh[i].sh_offset + sh[i].sh_size > sz) break;
                    p = (const char *)b + sh[i].sh_offset;
                    end = p + sh[i].sh_size;
                    while (p < end) {
                        l = strnlen(p, end - p);
                        if (!strncmp(p, "alias=", 6))
                            fprintf(fa, "alias %s %s\n", p + 6, m->name);
                        else if (!strncmp(p, "depends=", 8))
                            m->deps = xstrdup(p + 8);
                        p += l + 1;
                    }
                    rc = 0;
                    break;
                }
            }
        }
    }
    munmap(b, sz);
    return rc;
}

static void closure(int i, int *seen, int *order, int *n)
{
    char *copy, *tok, *sv;
    int j;

    seen[i] = 1;
    if (mods[i].deps && *mods[i].deps) {
        copy = xstrdup(mods[i].deps);
        for (tok = strtok_r(copy, ",", &sv); tok; tok = strtok_r(NULL, ",", &sv)) {
            norm(tok);
            j = findmod(tok);
            if (j >= 0 && !seen[j]) closure(j, seen, order, n);
        }
        free(copy);
    }
    order[(*n)++] = i;
}

static int depmod(void)
{
    char p[600];
    FILE *fa, *fd;
    int i, k, n, bad = 0, *seen, *order;

    walk("");
    if (!nmods) {
        fprintf(stderr, "n-dev: no .ko modules found in %s\n", moddir);
        if (skipped)
            fprintf(stderr, "n-dev: %d compressed modules found; use depmod from kmod\n", skipped);
        return 1;
    }
    snprintf(p, sizeof p, "%s/modules.alias", moddir);
    if (!(fa = fopen(p, "w"))) { perror(p); return 1; }
    for (i = 0; i < nmods; i++)
        if (modinfo(&mods[i], fa) < 0) bad++;
    fclose(fa);

    snprintf(p, sizeof p, "%s/modules.dep", moddir);
    if (!(fd = fopen(p, "w"))) { perror(p); return 1; }
    seen = xrealloc(NULL, nmods * sizeof *seen);
    order = xrealloc(NULL, nmods * sizeof *order);
    for (i = 0; i < nmods; i++) {
        memset(seen, 0, nmods * sizeof *seen);
        n = 0;
        closure(i, seen, order, &n);
        fprintf(fd, "%s:", mods[i].path);
        for (k = n - 2; k >= 0; k--)
            fprintf(fd, " %s", mods[order[k]].path);
        fputc('\n', fd);
    }
    fclose(fd);
    printf("n-dev: indexed %d modules (%d unreadable, %d compressed skipped)\n",
        nmods, bad, skipped);
    return 0;
}

int main(int argc, char **argv)
{
    struct utsname u;
    int s = -1;

    uname(&u);
    snprintf(moddir, sizeof moddir, "/lib/modules/%s", u.release);
    if (argc > 1 && !strcmp(argv[1], "-d"))
        return depmod();
    if (access(moddir, F_OK) < 0) {
        fprintf(stderr, "n-dev: no %s, nothing to load\n", moddir);
        if (argc > 1 && !strcmp(argv[1], "-c")) return 0; for (;;) pause();
    }
    if (readindex() < 0 || readaliases() < 0) {
        fprintf(stderr, "n-dev: no module index in %s (run: n-dev -d)\n", moddir);
        return 1;
    }
    if (!(argc > 1 && !strcmp(argv[1], "-c")))
        s = openuevent();
    scan("/sys/devices");
    if (s < 0) return 0;
    listen_loop(s);
    return 0;
}
