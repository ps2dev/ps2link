/*********************************************************************
 * Copyright (C) 2003 Tord Lindstrom (pukko@home.se)
 * This file is subject to the terms and conditions of the PS2Link License.
 * See the file LICENSE in the main directory of this distribution for more
 * details.
 */

#include <tamtypes.h>
#include <kernel.h>
#include <stdlib.h>
#include <stdio.h>
#include <ioman.h>
#include <intrman.h>
#include <loadcore.h>

#include "ps2ip.h"
#include "net_fio.h"
#include "hostlink.h"

////////////////////////////////////////////////////////////////////////
static void *tty_functarray[16];
static char ttyname[] = "tty";
static struct fileio_driver tty_driver = { 
    &ttyname[0],
    1,
    1,
    "TTY via Udp",
    &tty_functarray[0]
};

static int tty_socket = 0;
static int tty_sema = -1;

////////////////////////////////////////////////////////////////////////
static int dummy()
{
    return -5;
}

////////////////////////////////////////////////////////////////////////
static int dummy0()
{
    return 0;
}

////////////////////////////////////////////////////////////////////////
static int ttyInit(struct fileio_driver *driver)
{
    struct t_sema sema_info;
    struct sockaddr_in saddr;

    sema_info.attr       = 0;
    sema_info.init_count = 1;	/* Unlocked.  */
    sema_info.max_count  = 1;
    if ((tty_sema = CreateSema(&sema_info)) < 0)
	    return -1;

    // Create/open udp socket
    if ((tty_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0)
	    return -1;

    saddr.sin_family      = AF_INET;
    saddr.sin_addr.s_addr = htonl(INADDR_ANY);
    saddr.sin_port        = htons(PKO_PRINTF_PORT);

    return bind(tty_socket, (struct sockaddr *)&saddr, sizeof(saddr));
}

////////////////////////////////////////////////////////////////////////
static int ttyOpen( int fd, char *name, int mode)
{
    return 1;
}

////////////////////////////////////////////////////////////////////////
static int ttyClose( int fd)
{
    return 1;
}


////////////////////////////////////////////////////////////////////////
static int ttyWrite(iop_file_t *file, char *buf, int size)
{
    struct sockaddr_in dstaddr;
    int res;    

    WaitSema(tty_sema);

    dstaddr.sin_family      = AF_INET;
    dstaddr.sin_addr.s_addr = remote_pc_addr;
    dstaddr.sin_port        = htons(PKO_PRINTF_PORT);

    res = sendto(tty_socket, buf, size, 0, (struct sockaddr *)&dstaddr, 
                    sizeof(dstaddr));

    SignalSema(tty_sema);
    return res;
}

////////////////////////////////////////////////////////////////////////
// Entry point for mounting the file system
int ttyMount(void)
{
    int	i;


    tty_driver.device = "tty";
    tty_driver.xx1 = 3;
    tty_driver.version = 1;
    tty_driver.description = "TTY via Udp";
    tty_driver.function_list = tty_functarray;

    for (i=0;i < 16; i++)
        tty_functarray[i] = dummy;
    tty_functarray[0] = ttyInit;
    tty_functarray[1] = dummy0;
    tty_functarray[3] = ttyOpen;
    tty_functarray[4] = ttyClose;
    tty_functarray[6] = ttyWrite;

    close(0);
    close(1);
    FILEIO_del("tty");
    FILEIO_add(&tty_driver);
    open("tty00:", 0x1003);
    open("tty00:", 2);

    return 0;
}
