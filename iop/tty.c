/*********************************************************************
 * Copyright (C) 2003 Tord Lindstrom (pukko@home.se)
 * This file is subject to the terms and conditions of the PS2Link License.
 * See the file LICENSE in the main directory of this distribution for more
 * details.
 */


#include "ps2link.h"

#include "net_fio.h"

struct filedesc_info
{
    int unkn0;
    int unkn4;
    int device_id;   // the X in hostX
    int own_fd;
};

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
static int ttyWrite( int fd, char *buf, int size)
{
    struct sockaddr_in dstaddr;
    int n;    
    struct filedesc_info *fd_info;

    fd_info = (struct filedesc_info *)fd;

    if (fd_info->unkn4 >= 2) {
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
