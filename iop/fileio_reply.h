#ifndef PS2LINK_FILEIO_REPLY_H
#define PS2LINK_FILEIO_REPLY_H

#include "hostlink.h"

#define PKO_FILEIO_PACKET_LIMIT 4096

/* Detach before disconnect, so every close path invalidates the handle. */
static int pko_fileio_detach_socket(int *current_socket)
{
    int socket = *current_socket;
    *current_socket = -1;
    return socket;
}

static int pko_fileio_reply_header_valid(unsigned int command,
                                          unsigned int length,
                                          unsigned int expected_command,
                                          unsigned int expected_length)
{
    return expected_length >= sizeof(pko_pkt_hdr) &&
           expected_length <= PKO_FILEIO_PACKET_LIMIT &&
           command == expected_command && length == expected_length;
}

static int pko_fileio_read_length_valid(int received, int requested)
{
    return requested >= 0 && received >= 0 && received <= requested;
}

/* A successful nonempty write must advance by no more than it requested. */
static int pko_fileio_write_progress_valid(int written, int requested)
{
    return requested > 0 && written > 0 && written <= requested;
}

#endif
