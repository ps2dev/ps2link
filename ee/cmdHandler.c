/*********************************************************************
 * Copyright (C) 2003 Tord Lindstrom (pukko@home.se)
 * This file is subject to the terms and conditions of the PS2Link License.
 * See the file LICENSE in the main directory of this distribution for more
 * details.
 */

#include <tamtypes.h>
#include <kernel.h>
#include <sifrpc.h>
#include <loadfile.h>
#include <iopcontrol.h>

#include "cd.h"
#include "byteorder.h"
#include "ps2regs.h"
#include "hostlink.h"

//#define DEBUG
#ifdef DEBUG
#define dbgprintf(args...) printf(args)
#else
#define dbgprintf(args...) do { } while(0)
#endif

////////////////////////////////////////////////////////////////////////
// Prototypes
static int cmdThread(void);
static int pkoExecEE(pko_pkt_execee_req *cmd);
static void pkoReset(void);
static int pkoLoadElf(char *path);

// Flags for which type of boot (oh crap, make a header file dammit)
#define B_CD 1
#define B_MC 2
#define B_HOST 3
#define B_DMS3 4
#define B_UNKN 5

////////////////////////////////////////////////////////////////////////
// Globals
extern u32 _start;
extern int _gp;
extern int boot;
extern char elfName[];

int userThreadID = 0;
static int cmdThreadID = 0;
static char userThreadStack[16*1024] __attribute__((aligned(16)));
static char cmdThreadStack[16*1024] __attribute__((aligned(64)));

// The 'global' argv string area
static char argvStrings[PKO_MAX_PATH];

////////////////////////////////////////////////////////////////////////
// How about that header file again?
#define MAX_ARGS 16
#define MAX_ARGLEN 256

struct argData
{
    int flag;                     // Contains thread id atm
    int argc;
    char *argv[MAX_ARGS];
} __attribute__((packed)) userArgs;

////////////////////////////////////////////////////////////////////////
int sifCmdSema;
int sif0HandlerId = 0;
// XXX: Hardcoded address atm.. Should be configurable!!
unsigned int *sifDmaDataPtr =(unsigned int*)(0x20100000-2048);


////////////////////////////////////////////////////////////////////////
// Create the argument struct to send to the user thread
int
makeArgs(int cmdargc, char *cmdargv, struct argData *arg_data)
{
    int i;
    int t;
    int argc;

    argc = 0;

    if (cmdargc > MAX_ARGS)
        cmdargc = MAX_ARGS;
    cmdargv[PKO_MAX_PATH-1] = '\0';

    memcpy(argvStrings, cmdargv, PKO_MAX_PATH);

    dbgprintf("cmd->argc %d, argv[0]: %s\n", cmdargc, cmdargv);

    i = 0;
    t = 0;
    do {
        arg_data->argv[i] = &argvStrings[t];
        dbgprintf("arg_data[%d]=%s\n", i, arg_data->argv[i]);
        dbgprintf("argvStrings[%d]=%s\n", t, &argvStrings[t]);
        t += strlen(&argvStrings[t]);
        t++;
        i++;
    } while ((i < cmdargc) && (t < PKO_MAX_PATH));

    arg_data->argc = i;

    return 0;
}

////////////////////////////////////////////////////////////////////////
// Load the actual elf, and create a thread for it
// Return the thread id
static int
pkoLoadElf(char *path)
{
    ee_thread_t th_attr;
    t_ExecData elfdata;
    int ret;
    int pid;

    ret = SifLoadElf(path, &elfdata);

    FlushCache(0);
    FlushCache(2);

    dbgprintf("EE: LoadElf returned %d\n", ret);

    dbgprintf("EE: Creating user thread (ent: %x, gp: %x, st: %x)\n", 
              elfdata.epc, elfdata.gp, elfdata.sp);

    if (elfdata.epc == 0) {
        dbgprintf("EE: Could not load file\n");
        return -1;
    }

    th_attr.func = (void *)elfdata.epc;
    th_attr.stack = userThreadStack;
    th_attr.stack_size = sizeof(userThreadStack);
    th_attr.gp_reg = (void *)elfdata.gp;
    th_attr.initial_priority = 64;

    pid = CreateThread(&th_attr);
    if (pid < 0) {
        printf("EE: Create user thread failed %d\n", pid);
        return -1;
    }

    dbgprintf("EE: Created user thread: %d\n", pid);

    return pid;
}


////////////////////////////////////////////////////////////////////////
// Load and start the requested elf
static int
pkoExecEE(pko_pkt_execee_req *cmd)
{
    char path[PKO_MAX_PATH];
    int ret;
    int pid;

    if (userThreadID) {
        return -1;
    }

    dbgprintf("EE: Executing file %s...\n", cmd->argv);
    memcpy(path, cmd->argv, PKO_MAX_PATH);

    scr_printf("Executing file %s...\n", path);

    pid = pkoLoadElf(path);
    if (pid < 0) {
        scr_printf("Could not execute file %s\n", path);
        return -1;
    }

    FlushCache(0);
    FlushCache(2);

    userThreadID = pid;

    makeArgs(ntohl(cmd->argc), path, &userArgs);

    // Hack away..
    userArgs.flag = (int)&userThreadID;

    ret = StartThread(userThreadID, &userArgs);
    if (ret < 0) {
        printf("EE: Start user thread failed %d\n", ret);
        cmdThreadID = 0;
        DeleteThread(userThreadID);
        return -1;
    }
    return ret;
}

////////////////////////////////////////////////////////////////////////
static void
pkoReset(void)
{
    char *argv[1];
    // Check if user thread is running, if so kill it

#if 1
    if (userThreadID) {
        TerminateThread(userThreadID);
        DeleteThread(userThreadID);
    }
#endif
    userThreadID = 0;

    RemoveDmacHandler(5, sif0HandlerId);
    sif0HandlerId = 0;

    SifInitRpc(0);
    cdvdInit(CDVD_INIT_NOWAIT);
    cdvdInit(CDVD_EXIT);
    SifIopReset(NULL, 0);
    SifExitRpc();
    while(SifIopSync());
#if 1
    SifInitRpc(0);
    cdvdExit();
    SifExitRpc();
#endif

    FlushCache(0);
    FlushCache(2);

    if ((boot == B_MC) || (boot == B_HOST) || (boot == B_UNKN) /*mrb*/ || (boot == B_DMS3)) {
        argv[0] = elfName;
        ExecPS2(&_start, 0, 1, argv);
    }
    else {
        LoadExecPS2(elfName, 0, NULL);
    }
}

////////////////////////////////////////////////////////////////////////
// Sif dma interrupt handler, wakes up the cmd handler thread if 
// data was sent to our dma area
int
pkoCmdIntrHandler()
{
    int flag;

    flag = *sifDmaDataPtr;

    iSifSetDChain();

    if (flag) {
        // Clear new packet flag
        *sifDmaDataPtr = 0;

        iWakeupThread(cmdThreadID);
    }
    asm __volatile__("sync");
    EI;
    return 0;
}

////////////////////////////////////////////////////////////////////////
// Sif cmd handler thread, waits to be woken by the sif dma intr handler
static int
cmdThread()
{
    void *pkt;
    unsigned int cmd;
    int ret;
    int done;
    dbgprintf("EE: Cmd thread\n");

    done = 0;
    while(!done) {
        SleepThread();

        cmd = ntohl(sifDmaDataPtr[1]);
        pkt = &sifDmaDataPtr[1];

        switch(cmd) {
        case PKO_RESET_CMD:
            pkoReset();
            ret = 0;
            done = 1;
            break;
        
        case PKO_EXECEE_CMD: 
            dbgprintf("EE: Rpc EXECEE called\n");
            ret = pkoExecEE(pkt);
            break;
        default: 
            printf("EE: Unknown rpc cmd (%x)!\n", cmd);
            ret = -1;
        }
    }

    ExitDeleteThread();
    return 0;
}

////////////////////////////////////////////////////////////////////////
// Create cmd thread & install sif dma interrupt handler
int 
initCmdRpc(void)
{
    ee_thread_t th_attr;
    int ret;

    th_attr.func = cmdThread;
    th_attr.stack = cmdThreadStack;
    th_attr.stack_size = sizeof(cmdThreadStack);
    th_attr.gp_reg = &_gp;
    th_attr.initial_priority = 0x2;

    ret = CreateThread(&th_attr);
    if (ret < 0) {
        printf("EE: initCmdRpc createThread failed %d\n", ret);
        return -1;
    }

    cmdThreadID = ret;

    ret = StartThread(cmdThreadID, 0);
    if (ret < 0) {
        printf("EE: initCmdRpc startThread failed %d\n", ret);
        cmdThreadID = 0;
        DeleteThread(cmdThreadID);
        return -1;
    }

    // Install our sif dma interrupt handler
    FlushCache(0);
    sifDmaDataPtr[0] = 0;

    if (D_STAT & 0x20)
        D_STAT = 0x20;   // Clear dma chan 5 irq
    if (!(D5_CHCR & 0x100))
        SifSetDChain();

    sif0HandlerId = AddDmacHandler(5, pkoCmdIntrHandler, 0);
    EnableDmac(5); 
    return 0;
}
