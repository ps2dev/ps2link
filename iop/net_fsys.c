/*********************************************************************
 * Copyright (C) 2003 Tord Lindstrom (pukko@home.se)
 * Copyright (c) 2003 Marcus R. Brown <mrbrown@0xd6.org>
 *
 * This file is subject to the terms and conditions of the PS2Link License.
 * See the file LICENSE in the main directory of this distribution for more
 * details.
 */

#include "ps2link.h"

#include "net_fio.h"

static char fsname[] = "host";

////////////////////////////////////////////////////////////////////////
/* We need(?) to protect the net access, so the server doesn't get
 * confused if two processes calls a fsys func at the same time
 */
static int fsys_sema;
static int fsys_pid = 0;
static void *fsys_functarray[16];

////////////////////////////////////////////////////////////////////////
static int fsysError()
{
    return -ESRCH;
}

////////////////////////////////////////////////////////////////////////
static int fsysInit(iop_device_t *driver)
{
    iop_thread_t mythread;
    int pid;
    int i;

    dbgprintf("initializing %s\n", driver->name);

    // Start socket server thread

    mythread.attr = TH_C; // attr
    mythread.thread = (void *)pko_file_serv; // entry
    mythread.stacksize = 0x800;
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

    return 0;
}

////////////////////////////////////////////////////////////////////////
static int fsysDestroy(iop_device_t *driver)
{
    WaitSema(fsys_sema);
    pko_close_fsys();
    //    ExitDeleteThread(fsys_pid);
    SignalSema(fsys_sema);
    DeleteSema(fsys_sema);
    return 0;
}


////////////////////////////////////////////////////////////////////////
static int fsysOpen(iop_file_t *file, const char *name, int mode)
{
    int fsys_fd;
    
    dbgprintf("fsysOpen..\n");
    dbgprintf("  fd: %x, name: %s, mode: %d\n\n", file, name, mode);

    WaitSema(fsys_sema);
    fsys_fd = pko_open_file(name, mode);
    SignalSema(fsys_sema);

    file->privdata = (void *)fsys_fd;
    return fsys_fd;
}



////////////////////////////////////////////////////////////////////////
static int fsysClose(iop_file_t *file)
{
    int ret;
    
    dbgprintf("fsys_close..\n"
           "  fd: %x\n\n", file);
    
    WaitSema(fsys_sema);
    ret = pko_close_file((int)file->privdata);
    SignalSema(fsys_sema);

    return ret;
}



////////////////////////////////////////////////////////////////////////
static int fsysRead(iop_file_t *file, char *buf, int size)
{
    int ret, fd = (int)file->privdata;

    dbgprintf("fsysRead..."
           "  fd: %x\n"
           "  bf: %x\n"
           "  sz: %d\n"
           "  ow: %d\n\n", file, (int)buf, size, fd);

    WaitSema(fsys_sema);
    ret = pko_read_file(fd, buf, size);
    SignalSema(fsys_sema);

    return ret;
}




////////////////////////////////////////////////////////////////////////
static int fsysWrite(iop_file_t *file, char *buf, int size)
{
    int ret, fd = (int)file->privdata;
    
    dbgprintf("fsysWrite..."
           "  fd: %x\n", file);

    WaitSema(fsys_sema);
    ret = pko_write_file(fd, buf, size);
    SignalSema(fsys_sema);

    return ret;
}



////////////////////////////////////////////////////////////////////////
static int fsysLseek(iop_file_t *file, long offset, int whence)
{
    int ret, fd = (int)file->privdata;

    dbgprintf("fsysLseek..\n"
           "  fd: %x\n"
           "  of: %x\n"
           "  wh: %x\n\n", file, offset, whence);

    WaitSema(fsys_sema);
    ret = pko_lseek_file(fd, offset, whence);
    SignalSema(fsys_sema);

    return ret;
}

static void * fsd_ops[] = { fsysInit, fsysDestroy, fsysError, fsysOpen, fsysClose,
	fsysRead, fsysWrite, fsysLseek, fsysError, fsysError, fsysError, fsysError,
	fsysError, fsysError, fsysError, fsysError, fsysError
};

iop_device_t fsys_driver = {
	"host",
	IOP_DT_FS,
	1,
	"fsys driver",
	(iop_device_ops_t *)&fsd_ops
};

////////////////////////////////////////////////////////////////////////
// Entry point for mounting the file system
int fsysMount(void)
{
    iop_sema_t sema_info;

    sema_info.attr = 1;
    sema_info.initial = 1;
    sema_info.max = 1;
    fsys_sema = CreateSema(&sema_info);

    DelDrv(fsname);
    AddDrv(&fsys_driver);

    return 0;
}

int fsysUnmount(void)
{
    DelDrv(fsname);
    return 0;
}
