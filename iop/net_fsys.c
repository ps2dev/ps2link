/*********************************************************************
 * Copyright (C) 2003 Tord Lindstrom (pukko@home.se)
 * Copyright (C) 2004 adresd (adresd_ps2dev@yahoo.com)
 * This file is subject to the terms and conditions of the PS2Link License.
 * See the file LICENSE in the main directory of this distribution for more
 * details.
 */

#include <tamtypes.h>
#include <kernel.h>
#include <fileio.h>
#include <stdlib.h>
#include <stdio.h>
#include <intrman.h>
#include <loadcore.h>
#include <thsemap.h>

#include "net_fio.h"

//#define DEBUG
#ifdef DEBUG
#define dbgprintf(args...) printf(args)
#else
#define dbgprintf(args...) do { } while(0)
#endif


static char fsname[] = "host";

////////////////////////////////////////////////////////////////////////
static struct fileio_driver fsys_driver;

/* File desc struct is probably larger than this, but we only
 * need to access the word @ offset 0x0C (in which we put our identifier)
 */
struct filedesc_info
{
    int unkn0;
    int unkn4;
    int device_id;   // the X in hostX
    int own_fd;
};

////////////////////////////////////////////////////////////////////////
/* We need(?) to protect the net access, so the server doesn't get
 * confused if two processes calls a fsys func at the same time
 */
static int fsys_sema;
static int fsys_pid = 0;
static struct fileio_driver fsys_driver;
static void *fsys_functarray[16];

////////////////////////////////////////////////////////////////////////
static int dummy5()
{
    printf("dummy function called\n");
    return -5;
}

////////////////////////////////////////////////////////////////////////
static void fsysInit( struct fileio_driver *driver)
{
    struct t_thread mythread;
    int pid;
    int i;

    dbgprintf("initializing %s\n", driver->device);

    // Start socket server thread

    mythread.type = 0x02000000; // attr
    mythread.unknown = 0; // option
    mythread.function = (void *)pko_file_serv; // entry
    mythread.stackSize = 0x800;
    mythread.priority = 0x45; // We really should choose prio w more effort

    pid = CreateThread(&mythread);

    if (pid > 0) {
        if ((i=StartThread(pid, NULL)) < 0) {
            printf("StartThread failed (%d)\n", i);
        }
    } 
    else {
        printf("CreateThread failed (%d)\n", pid);
    }
    
    fsys_pid = pid;
    dbgprintf("Thread id: %x\n", pid);
}

////////////////////////////////////////////////////////////////////////
static int fsysDestroy(void)
{
    WaitSema(fsys_sema);
    pko_close_fsys();
    //    ExitDeleteThread(fsys_pid);
    SignalSema(fsys_sema);
    DeleteSema(fsys_sema);
    return 0;
}


////////////////////////////////////////////////////////////////////////
static int fsysOpen( int fd, char *name, int mode)
{
    struct filedesc_info *fd_info;
    int fsys_fd;
    
    dbgprintf("fsysOpen..\n");
    dbgprintf("  fd: %x, name: %s, mode: %d\n\n", fd, name, mode);

    fd_info = (struct filedesc_info *)fd;

    WaitSema(fsys_sema);
    fsys_fd = pko_open_file(name, mode);
    SignalSema(fsys_sema);
    fd_info->own_fd = fsys_fd;

    return fsys_fd;
}



////////////////////////////////////////////////////////////////////////
static int fsysClose( int fd)
{
    struct filedesc_info *fd_info;
    int ret;
    
    dbgprintf("fsys_close..\n"
           "  fd: %x\n\n", fd);
    
    fd_info = (struct filedesc_info *)fd;
    WaitSema(fsys_sema);
    ret = pko_close_file(fd_info->own_fd);    
    SignalSema(fsys_sema);

    return ret;
}



////////////////////////////////////////////////////////////////////////
static int fsysRead( int fd, char *buf, int size)
{
    struct filedesc_info *fd_info;
    int ret;

    fd_info = (struct filedesc_info *)fd;

    dbgprintf("fsysRead..."
           "  fd: %x\n"
           "  bf: %x\n"
           "  sz: %d\n"
           "  ow: %d\n\n", fd, (int)buf, size, fd_info->own_fd);

    WaitSema(fsys_sema);
    ret = pko_read_file(fd_info->own_fd, buf, size);
    SignalSema(fsys_sema);

    return ret;
}




////////////////////////////////////////////////////////////////////////
static int fsysWrite( int fd, char *buf, int size)
{
    struct filedesc_info *fd_info;
    int ret;
    
    dbgprintf("fsysWrite..."
           "  fd: %x\n", fd);

    fd_info = (struct filedesc_info *)fd;
    WaitSema(fsys_sema);
    ret = pko_write_file(fd_info->own_fd, buf, size);
    SignalSema(fsys_sema);
    return ret;
}



////////////////////////////////////////////////////////////////////////
static int fsysLseek( int fd, unsigned int offset, int whence)
{
    struct filedesc_info *fd_info;
    int ret;

    dbgprintf("fsysLseek..\n"
           "  fd: %x\n"
           "  of: %x\n"
           "  wh: %x\n\n", fd, offset, whence);

    fd_info = (struct filedesc_info *)fd;
    WaitSema(fsys_sema);
    ret = pko_lseek_file(fd_info->own_fd, offset, whence);
    SignalSema(fsys_sema);
    return ret;
}

////////////////////////////////////////////////////////////////////////
static int fsysDopen(int fd, char *name)
{
    struct filedesc_info *fd_info;
    int fsys_fd;
    
    dbgprintf("fsysDopen..\n");
    dbgprintf("  fd: %x, name: %s\n\n", fd, name);

    fd_info = (struct filedesc_info *)fd;

    WaitSema(fsys_sema);
    fsys_fd = pko_open_dir(name);
    SignalSema(fsys_sema);
    fd_info->own_fd = fsys_fd;

    return fsys_fd;
}

////////////////////////////////////////////////////////////////////////
static int fsysDread(int fd, void *buf)
{
    struct filedesc_info *fd_info;
    int ret;

    fd_info = (struct filedesc_info *)fd;

    dbgprintf("fsysDread..."
           "  fd: %x\n"
           "  bf: %x\n"
           "  ow: %d\n\n", fd, (int)buf, fd_info->own_fd);

    WaitSema(fsys_sema);
    ret = pko_read_dir(fd_info->own_fd, buf);
    SignalSema(fsys_sema);

    return ret;

}

////////////////////////////////////////////////////////////////////////
static int fsysDclose(int fd)
{
    struct filedesc_info *fd_info;
    int ret;
    
    dbgprintf("fsys_dclose..\n"
           "  fd: %x\n\n", fd);
    
    fd_info = (struct filedesc_info *)fd;
    WaitSema(fsys_sema);
    ret = pko_close_dir(fd_info->own_fd);    
    SignalSema(fsys_sema);

    return ret;
}

////////////////////////////////////////////////////////////////////////
// Entry point for mounting the file system
int fsysMount(void)
{
    int	i;
    struct t_sema sema_info;

    fsys_driver.device = fsname;
    fsys_driver.xx1 = 16;
    fsys_driver.version = 1;
    fsys_driver.description = "fsys driver";
    fsys_driver.function_list = fsys_functarray;
    for (i=0;i < 16; i++)
        fsys_functarray[i] = dummy5;
    fsys_functarray[0] = fsysInit;
    fsys_functarray[1] = fsysDestroy;
    fsys_functarray[3] = fsysOpen;
    fsys_functarray[4] = fsysClose;
    fsys_functarray[5] = fsysRead;
    fsys_functarray[6] = fsysWrite;
    fsys_functarray[7] = fsysLseek;

    fsys_functarray[12] = fsysDopen;
    fsys_functarray[13] = fsysDclose;
    fsys_functarray[14] = fsysDread;

    sema_info.attr = 1;
    sema_info.option = 0;
    sema_info.init_count = 1;
    sema_info.max_count = 1;
    fsys_sema = CreateSema(&sema_info);

    FILEIO_del(fsname);
    FILEIO_add(&fsys_driver);

    return 0;
}

int fsysUnmount(void)
{
    FILEIO_del(fsname);
    return 0;
}
