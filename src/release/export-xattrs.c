#define _GNU_SOURCE
#include <sys/stat.h>
#include <sys/xattr.h>
#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <inttypes.h>

/* Read-only companion to a static system/vendor tar export. Never traverses
 * /data, runtime directories, symlinks or another filesystem. Output is TSV:
 * relative path, xattr name, lowercase hexadecimal bytes. */
static const char *root;
static dev_t device;
static void fail(const char *path) { perror(path); exit(1); }
static void clean_name(const char *s) {
    if (strpbrk(s, "\t\r\n")) { fputs("Unrepresentable name\n", stderr); exit(1); }
}
static void walk(const char *relative, unsigned depth) {
    if (depth > 64) { fputs("Directory depth exceeded\n", stderr); exit(1); }
    clean_name(relative);
    char path[4096];
    if (snprintf(path, sizeof(path), "%s/%s", root, relative) >= (int)sizeof(path)) exit(1);
    struct stat st;
    if (lstat(path, &st)) fail(path);
    if (st.st_dev != device) { fprintf(stderr, "Cross-filesystem entry: %s\n", relative); exit(1); }
    printf("#stat\t%s\t%ju\t%jd.%09ld\t%jd.%09ld\t%jo\t%ju\t%ju\n", relative,
           (uintmax_t)st.st_size, (intmax_t)st.st_mtim.tv_sec, st.st_mtim.tv_nsec,
           (intmax_t)st.st_ctim.tv_sec, st.st_ctim.tv_nsec, (uintmax_t)st.st_mode,
           (uintmax_t)st.st_uid, (uintmax_t)st.st_gid);
    ssize_t size = llistxattr(path, NULL, 0);
    if (size < 0) fail(path);
    if (size > 65536) exit(1);
    if (size) {
        char *names = malloc((size_t)size);
        if (!names || llistxattr(path, names, (size_t)size) != size) fail(path);
        for (char *name = names; name < names + size; name += strlen(name) + 1) {
            clean_name(name);
            ssize_t n = lgetxattr(path, name, NULL, 0);
            if (n < 0 || n > 1048576) fail(path);
            unsigned char *data = malloc((size_t)n + 1);
            if (!data || lgetxattr(path, name, data, (size_t)n) != n) fail(path);
            printf("%s\t%s\t", relative, name);
            for (ssize_t i = 0; i < n; ++i) printf("%02x", data[i]);
            putchar('\n'); free(data);
        }
        free(names);
    }
    if (!S_ISDIR(st.st_mode)) return;
    DIR *d = opendir(path);
    if (!d) fail(path);
    struct dirent *e;
    while ((e = readdir(d))) {
        if (!strcmp(e->d_name, ".") || !strcmp(e->d_name, "..")) continue;
        char next[4096];
        if (snprintf(next, sizeof(next), "%s/%s", relative, e->d_name) >= (int)sizeof(next)) exit(1);
        walk(next, depth + 1);
    }
    if (closedir(d)) fail(path);
}
int main(int argc, char **argv) {
    if (argc != 2 || getuid() || argv[1][0] != '/' || !strcmp(argv[1], "/")) return 2;
    root = argv[1]; struct stat st;
    if (lstat(root, &st) || !S_ISDIR(st.st_mode)) return 2;
    device = st.st_dev;
    const char *paths[] = {"system", "vendor", "init", "init.environ.rc", "init.droidspaces", "init.arm13-caps"};
    for (unsigned i = 0; i < sizeof(paths) / sizeof(paths[0]); ++i) walk(paths[i], 0);
    return fflush(stdout) || ferror(stdout) ? 1 : 0;
}
