/*********************************************************************
 * Copyright (C) 2003 Tord Lindstrom (pukko@home.se)
 * This file is subject to the terms and conditions of the PS2Link License.
 * See the file LICENSE in the main directory of this distribution for more
 * details.
 */

#define PKO_PORT        0x4711
#define PKO_PRINTF_PORT 0x4712

#define PKO_OPEN_CMD    0xbabe0111
#define PKO_OPEN_RLY    0xbabe0112
#define PKO_CLOSE_CMD   0xbabe0121
#define PKO_CLOSE_RLY   0xbabe0122
#define PKO_READ_CMD    0xbabe0131
#define PKO_READ_RLY    0xbabe0132
#define PKO_WRITE_CMD   0xbabe0141
#define PKO_WRITE_RLY   0xbabe0142
#define PKO_LSEEK_CMD   0xbabe0151
#define PKO_LSEEK_RLY   0xbabe0152

#define PKO_RESET_CMD   0xbabe0201
#define PKO_EXECIOP_CMD 0xbabe0202
#define PKO_EXECEE_CMD  0xbabe0203
#define PKO_POWEROFF_CMD 0xbabe0204
#define PKO_SCRDUMP_CMD 0xbabe205
#define PKO_NETDUMP_CMD 0xbabe206

#define PKO_RPC_RESET   1
#define PKO_RPC_EXECEE  2
#define PKO_RPC_DUMMY   3
#define PKO_RPC_SCRDUMP 4
#define PKO_RPC_NETDUMP 5

#define PKO_MAX_PATH   256

typedef struct
{
    unsigned int cmd;
    unsigned short len;
} __attribute__((packed)) pko_pkt_hdr;

typedef struct
{
    unsigned int cmd;
    unsigned short len;
    unsigned int retval;
} __attribute__((packed)) pko_pkt_file_rly;

typedef struct
{
    unsigned int cmd;
    unsigned short len;
    int flags;
    char path[PKO_MAX_PATH];
} __attribute__((packed)) pko_pkt_open_req;

typedef struct
{
    unsigned int cmd;
    unsigned short len;
    int fd;
} __attribute__((packed)) pko_pkt_close_req;

typedef struct
{
    unsigned int cmd;
    unsigned short len;
    int fd;
    int nbytes;
} __attribute__((packed)) pko_pkt_read_req;

typedef struct
{
    unsigned int cmd;
    unsigned short len;
    int retval;
    int nbytes;
} __attribute__((packed)) pko_pkt_read_rly;

typedef struct
{
    unsigned int cmd;
    unsigned short len;
    int fd;
    int nbytes;
} __attribute__((packed)) pko_pkt_write_req;

typedef struct
{
    unsigned int cmd;
    unsigned short len;
    int fd;
    int offset;
    int whence;
} __attribute__((packed)) pko_pkt_lseek_req;


////

typedef struct
{
    unsigned int cmd;
    unsigned short len;
} __attribute__((packed)) pko_pkt_reset_req;

typedef struct
{
    unsigned int cmd;
    unsigned short len;
    int  argc;
    char argv[PKO_MAX_PATH];
} __attribute__((packed)) pko_pkt_execee_req;

typedef struct
{
    unsigned int cmd;
    unsigned short len;
    int  argc;
    char argv[PKO_MAX_PATH];
} __attribute__((packed)) pko_pkt_execiop_req;

typedef struct
{
    unsigned int cmd;
    unsigned short len;
} __attribute__((packed)) pko_pkt_poweroff_req;

#define PKO_MAX_WRITE_SEGMENT (1460 - sizeof(pko_pkt_write_req))
#define PKO_MAX_READ_SEGMENT  (1460 - sizeof(pko_pkt_read_rly))
