#define _GNU_SOURCE
#define _FILE_OFFSET_BITS 64
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <zlib.h>

/* Decode stdin into a NEW image only; zero chunks become holes. gzip CRC,
 * exact expanded size, O_EXCL and unlink-on-failure protect image transfers.
 * This utility does not mount, format or start any container. */
int main(int argc, char **argv) {
    if (argc != 3) { fputs("usage: gzip-to-sparse NEW_IMAGE EXACT_BYTES < image.gz\n", stderr); return 2; }
    char *end; errno = 0;
    uint64_t expected = strtoull(argv[2], &end, 10);
    if (errno || *end || !expected || expected > (uint64_t)INT64_MAX || argv[1][0] != '/') return 2;
    int fd = open(argv[1], O_CREAT | O_EXCL | O_WRONLY | O_CLOEXEC | O_NOFOLLOW, 0600);
    if (fd < 0) { perror("create new image"); return 1; }
    int result = 1;
    gzFile gz = gzdopen(dup(STDIN_FILENO), "rb");
    unsigned char *buffer = malloc(1024 * 1024);
    uint64_t total = 0;
    if (!gz || !buffer) goto finish;
    if (gzdirect(gz)) { fputs("gzip header required\n", stderr); goto finish; }
    for (;;) {
        int n = gzread(gz, buffer, 1024 * 1024);
        if (n < 0) { int code; fprintf(stderr, "gzip error: %s\n", gzerror(gz, &code)); goto finish; }
        if (!n) break;
        if (total + (uint64_t)n > expected) { fputs("expanded image exceeds expected size\n", stderr); goto finish; }
        size_t i = 0;
        while (i + sizeof(uint64_t) <= (size_t)n) {
            uint64_t word; memcpy(&word, buffer + i, sizeof(word));
            if (word) break;
            i += sizeof(word);
        }
        while (i < (size_t)n && !buffer[i]) ++i;
        if (i == (size_t)n) {
            if (lseek(fd, n, SEEK_CUR) < 0) { perror("seek hole"); goto finish; }
        } else {
            size_t done = 0;
            while (done < (size_t)n) {
                ssize_t written = write(fd, buffer + done, (size_t)n - done);
                if (written < 0 && errno == EINTR) continue;
                if (written <= 0) { perror("write image"); goto finish; }
                done += (size_t)written;
            }
        }
        total += (uint64_t)n;
    }
    if (total != expected || !gzeof(gz)) { fputs("incomplete gzip image\n", stderr); goto finish; }
    if (gzclose(gz) != Z_OK) { gz = NULL; fputs("gzip close failed\n", stderr); goto finish; }
    gz = NULL;
    if (ftruncate(fd, (off_t)expected) || fchmod(fd, 0644) || fsync(fd)) { perror("finish image"); goto finish; }
    result = 0;
finish:
    if (gz) gzclose(gz);
    free(buffer);
    if (close(fd) && !result) { perror("close image"); result = 1; }
    if (result) unlink(argv[1]);
    else fprintf(stderr, "Wrote %" PRIu64 " bytes to new sparse image\n", total);
    return result;
}
