#include <stdio.h>
#include "../iop/fileio_reply.h"

static int failed;

static void check(int ok, const char *case_name)
{
    if (!ok) {
        fprintf(stderr, "FAIL %s\n", case_name);
        failed++;
    }
}

int main(void)
{
    unsigned int len = sizeof(pko_pkt_file_rly);
    int descriptor = 0;

    check(pko_fileio_detach_socket(&descriptor) == 0 && descriptor == -1,
          "socket zero is valid and invalidated on close");
    descriptor = 17;
    check(pko_fileio_detach_socket(&descriptor) == 17 && descriptor == -1,
          "connected socket is invalidated on close");
    check(pko_fileio_detach_socket(&descriptor) == -1 && descriptor == -1,
          "closing an invalid socket is idempotent");

    check(pko_fileio_reply_header_valid(PKO_WRITE_RLY, len, PKO_WRITE_RLY, len),
          "valid write reply header");
    check(!pko_fileio_reply_header_valid(PKO_READ_RLY, len, PKO_WRITE_RLY, len),
          "mismatched reply type");
    check(!pko_fileio_reply_header_valid(PKO_WRITE_RLY, len + 1, PKO_WRITE_RLY, len),
          "oversized reply");
    check(!pko_fileio_reply_header_valid(PKO_WRITE_RLY, len - 1, PKO_WRITE_RLY, len),
          "truncated reply");
    check(!pko_fileio_reply_header_valid(PKO_WRITE_RLY, len, PKO_WRITE_RLY, 5),
          "invalid expected short packet");
    check(!pko_fileio_reply_header_valid(PKO_WRITE_RLY, len, PKO_WRITE_RLY, 4097),
          "invalid expected oversized packet");

    check(pko_fileio_read_length_valid(0, 10), "empty read");
    check(pko_fileio_read_length_valid(4, 10), "short read");
    check(pko_fileio_read_length_valid(10, 10), "full read");
    check(!pko_fileio_read_length_valid(-1, 10), "negative read");
    check(!pko_fileio_read_length_valid(11, 10), "oversized read");
    check(!pko_fileio_read_length_valid(1, -1), "negative request");

    check(pko_fileio_write_progress_valid(4, 10), "short write");
    check(pko_fileio_write_progress_valid(10, 10), "full write");
    check(!pko_fileio_write_progress_valid(0, 10), "zero progress");
    check(!pko_fileio_write_progress_valid(-1, 10), "negative write");
    check(!pko_fileio_write_progress_valid(11, 10), "oversized write");
    check(!pko_fileio_write_progress_valid(1, 0), "zero request");

    if (failed) return 1;
    puts("PASS fileio reply headers and read/write bounds");
    return 0;
}
