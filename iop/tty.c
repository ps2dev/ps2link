/*********************************************************************
 * Copyright (C) 2003 Tord Lindstrom (pukko@home.se)
 * Copyright (c) 2003 Marcus R. Brown <mrbrown@0xd6.org>
 *
 * This file is subject to the terms and conditions of the PS2Link License.
 * See the file LICENSE in the main directory of this distribution for more
 * details.
 */

#include "ps2link.h"

static int tty_socket = 0;

////////////////////////////////////////////////////////////////////////
static int ttyError() { return -ESRCH; }

////////////////////////////////////////////////////////////////////////
static int ttyInit(iop_device_t *driver)
{
    int sock;
    struct sockaddr_in saddr;
    int n;

    // Create/open udp socket
    sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock < 0) {
        return -1;
    }

    tty_socket = sock;

    memset(&saddr, 0, sizeof(saddr));
    saddr.sin_family = AF_INET;
    saddr.sin_addr.s_addr = htonl(INADDR_ANY);
    saddr.sin_port = htons(PKO_PRINTF_PORT);
    n = bind(sock, (struct sockaddr *)&saddr, sizeof(saddr));

    return 0;
}

static int ttyDeinit(iop_device_t *driver)
{
    return 0;
}


////////////////////////////////////////////////////////////////////////
static int ttyOpen(iop_file_t *file, char *name, int mode)
{
    /* Assume it's STDOUT.  */
    file->privdata = (void *)1;
    return 1;
}

////////////////////////////////////////////////////////////////////////
static int ttyClose(iop_file_t *file)
{
    return 1;
}


////////////////////////////////////////////////////////////////////////
static int ttyWrite(iop_file_t *file, char *buf, int size)
{
    struct sockaddr_in dstaddr;
    int n;    

    if ((int)file->privdata != 1) {
        return size;
    }

    memset(&dstaddr, 0, sizeof(dstaddr));
    dstaddr.sin_family = AF_INET;
	dstaddr.sin_addr.s_addr = remote_pc_addr;
    dstaddr.sin_port = htons(PKO_PRINTF_PORT);

    n = sendto(tty_socket, buf, size, 0, (struct sockaddr *)&dstaddr, 
                    sizeof(dstaddr));

    return size;
}

static void * tty_ops[] = { ttyInit, ttyDeinit, ttyError, ttyOpen, ttyClose,
	ttyError, ttyWrite, ttyError, ttyError, ttyError, ttyError, ttyError,
	ttyError, ttyError, ttyError, ttyError, ttyError
};

iop_device_t tty_driver = {
	"tty",
	IOP_DT_CONS|IOP_DT_CHAR,
	1,
	"TTY via Udp",
	(iop_device_ops_t *)&tty_ops
};

////////////////////////////////////////////////////////////////////////
// Entry point for mounting the file system
int ttyMount(void)
{
    close(0);
    close(1);

    DelDrv("tty");
    AddDrv(&tty_driver);

    /* Create STDIN and STDOUT.  */
    open("tty00:", O_RDONLY);
    open("tty00:", O_WRONLY);

    return 0;
}
