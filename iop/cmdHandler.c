/*********************************************************************
 * Copyright (C) 2003 Tord Lindstrom (pukko@home.se)
 * Copyright (C) 2003,2004 adresd (adresd_ps2dev@yahoo.com)
 * This file is subject to the terms and conditions of the PS2Link License.
 * See the file LICENSE in the main directory of this distribution for more
 * details.
 */

#include <types.h>
#include <ioman.h>
#include <sysclib.h>
#include <stdio.h>
#include <thbase.h>
#include <intrman.h>
#include <sifman.h>
#include <sifrpc.h>
#include <modload.h>

#include "ps2ip.h"
#include "hostlink.h"

#define ntohl(x) htonl(x)
#define ntohs(x) htons(x)

//#define DEBUG
#ifdef DEBUG
#define dbgprintf(args...) printf(args)
#else
#define dbgprintf(args...) do { } while(0)
#endif

/* 
 * This is to fix the dev9 shutdown on reset
 */
extern void dev9Shutdown(void);
extern void dev9IntrDisable(int mask);
extern int sceSifSetDma(SifDmaTransfer_t *dmat, int count);
//////////////////////////////////////////////////////////////////////////
// How about a header file?..
extern int fsysUnmount(void);

//////////////////////////////////////////////////////////////////////////

#define BUF_SIZE 1024
static char recvbuf[BUF_SIZE] __attribute__((aligned(16)));
static unsigned int rpc_data[1024/4]__attribute__((aligned(16)));

#define PKO_DMA_DEST ((void *)0x200ff800)
//unsigned int *dma_ptr =(unsigned int*)(0x20100000-2048);

//////////////////////////////////////////////////////////////////////////
static void 
pkoExecIop(char *buf, int len)
{
    pko_pkt_execiop_req *cmd;
    int retval;
    int arglen;
    char *path;
    char *args;
    unsigned int argc;
    int id;
    int i;

    cmd = (pko_pkt_execiop_req *)buf;

    dbgprintf("IOP cmd: EXECIOP\n");

    if (len != sizeof(pko_pkt_execiop_req)) {
        dbgprintf("IOP cmd: exec_iop got a broken packet (%d)!\n", len);
        return;
    }

    // Make sure arg vector is null-terminated
    cmd->argv[PKO_MAX_PATH-1] = '\0';

    printf("IOP cmd: %d args\n", ntohl(cmd->argc));

    path = &cmd->argv[0];
    args = &cmd->argv[strlen(cmd->argv) + 1];
    argc = ntohl(cmd->argc);

    arglen = 0;
    for (i = 0; i < (argc - 1); i++) {
        printf("arg %d: %s (%d)\n", i, &args[arglen], arglen);
        arglen += strlen(&args[arglen]) + 1;
    }

    id = LoadStartModule(cmd->argv, arglen, args, &retval);
    printf("loadmodule: id %d, ret %d\n", id, retval);
}

//////////////////////////////////////////////////////////////////////////
unsigned int
pkoSetSifDma(void *dest, void *src, unsigned int length, unsigned int mode)
{
    struct t_SifDmaTransfer sendData;
    int oldIrq;
    int id;

    sendData.src = (unsigned int *)src;
    sendData.dest = (unsigned int *)dest;
    sendData.size = length;
    sendData.attr = mode;

    CpuSuspendIntr(&oldIrq);
    id = sceSifSetDma(&sendData, 1);
    CpuResumeIntr(oldIrq);

    return id;
}

//////////////////////////////////////////////////////////////////////////
unsigned int
pkoSendSifCmd(unsigned int cmd, void *src, unsigned int len)
{
    unsigned int dmaId;

    rpc_data[0] = cmd;

    memcpy(&rpc_data[1], src, 
           (len > sizeof(rpc_data) ? sizeof(rpc_data) : len));

    len = len > sizeof(rpc_data) ? sizeof(rpc_data) : len;

    dmaId = pkoSetSifDma(PKO_DMA_DEST, rpc_data, len, 4);

    if(dmaId == 0) {
        printf("IOP: sifSendCmd %x failed\n", cmd);
        return -1;
    }
    return 0;
}


//////////////////////////////////////////////////////////////////////////
static void 
pkoExecEE(char *buf, int len)
{
    int ret;

    ret = pkoSendSifCmd(PKO_RPC_EXECEE, buf, len);
};
//////////////////////////////////////////////////////////////////////////
static void 
pkoGSExec(char *buf, int len)
{
    int ret;

    ret = pkoSendSifCmd(PKO_RPC_GSEXEC, buf, len);
};
//////////////////////////////////////////////////////////////////////////
static void 
pkoNetDump(char *buf, int len)
{
    int ret;

    ret = pkoSendSifCmd(PKO_RPC_NETDUMP, buf, len);
};
//////////////////////////////////////////////////////////////////////////
static void 
pkoScrDump(char *buf, int len)
{
    int ret;

    ret = pkoSendSifCmd(PKO_RPC_SCRDUMP, buf, len);
};

//////////////////////////////////////////////////////////////////////////
static void
pkoPowerOff()
{
    // This is to do the dev9 shutdown on reset
    dev9IntrDisable(-1);
    dev9Shutdown();

    *((unsigned char *)0xBF402017) = 0;
    *((unsigned char *)0xBF402016) = 0xF;
}

//////////////////////////////////////////////////////////////////////////
static void 
pkoReset(char *buf, int len)
{
    int ret;

    dbgprintf("IOP cmd: RESET\n");

    if (len != sizeof(pko_pkt_reset_req)) {
        dbgprintf("IOP cmd: exec_ee got a broken packet (%d)!\n", len);
        return;
    }

    LoadStartModule("rom0:CLEARSPU", 0, NULL, &ret);
    printf("unmounting\n");
    fsysUnmount();
    printf("unmounted\n");
    DelDrv("tty");

    // This is to do the dev9 shutdown on reset
    dev9IntrDisable(-1);
    dev9Shutdown();

    ret = pkoSendSifCmd(PKO_RPC_RESET, buf, len);
};

static void
pkoStopVU(char *buf, int len) {
    int ret;

    ret = pkoSendSifCmd(PKO_RPC_STOPVU, buf, len);
};

static void
pkoStartVU(char *buf, int len)  {
    int ret;
    ret = pkoSendSifCmd(PKO_RPC_STARTVU, buf, len);
};

static void
pkoDumpMem(char *buf, int len) {
    int ret;
    ret = pkoSendSifCmd(PKO_RPC_DUMPMEM, buf, len);
};

static void
pkoDumpReg(char *buf, int len) {
    int ret;
    ret = pkoSendSifCmd(PKO_RPC_DUMPREG, buf, len);
};

static void
pkoWriteMem(char *buf, int len) {
    int ret;
    ret = pkoSendSifCmd(PKO_RPC_WRITEMEM, buf, len);
};

//////////////////////////////////////////////////////////////////////////
static void 
cmdListener(int sock)
{
    int done;
    int len;
    int addrlen;
    unsigned int cmd;
    pko_pkt_hdr *header;
    struct sockaddr_in remote_addr;

    done = 0;

    while(!done) {

        addrlen = sizeof(remote_addr);
        len = recvfrom(sock, &recvbuf[0], BUF_SIZE, 0, 
                            (struct sockaddr *)&remote_addr, 
                            &addrlen);
        dbgprintf("IOP cmd: received packet (%d)\n", len);

        if (len < 0) {
            dbgprintf("IOP: cmdListener: recvfrom error (%d)\n", len);
            continue;
        }
        if (len < sizeof(pko_pkt_hdr)) {
            continue;
        }

        header = (pko_pkt_hdr *)recvbuf;
        cmd = ntohl(header->cmd);
        switch (cmd) {

        case PKO_EXECIOP_CMD:
            pkoExecIop(recvbuf, len);
            break;
        case PKO_EXECEE_CMD:
            pkoExecEE(recvbuf, len);
            break;
        case PKO_POWEROFF_CMD:
            pkoPowerOff();
            break;
        case PKO_RESET_CMD:
            pkoReset(recvbuf, len);
            break;
        case PKO_SCRDUMP_CMD:
            pkoScrDump(recvbuf, len);
            break;
        case PKO_NETDUMP_CMD:
            pkoNetDump(recvbuf, len);
            break;
		case PKO_START_VU:
			pkoStartVU(recvbuf, len);
			break;
		case PKO_STOP_VU:
			pkoStopVU(recvbuf, len);
			break;
		case PKO_DUMP_MEM:
			pkoDumpMem(recvbuf, len);
			break;
		case PKO_DUMP_REG:
			pkoDumpReg(recvbuf, len);
			break;
		case PKO_GSEXEC_CMD:
			pkoGSExec(recvbuf, len);
			break;
        case PKO_WRITE_MEM:
            pkoWriteMem(recvbuf, len);
            break;
        default: 
            dbgprintf("IOP cmd: Uknown cmd received\n");
            break;
        }

        dbgprintf("IOP cmd: waiting for next pkt\n");
    }
}

//////////////////////////////////////////////////////////////////////////
static void 
cmdThread(void *arg)
{
    struct sockaddr_in serv_addr;
    //    struct sockaddr_in remote_addr;
    int sock;
    int ret;

    dbgprintf( "IOP cmd: Server Thread Started.\n" );

    sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock < 0)
    {
        dbgprintf( "IOP cmd: Socket error %d\n", sock);
        ExitDeleteThread();
    }

    memset((void *)&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_addr.sin_port = htons(PKO_CMD_PORT);
   
    ret = bind(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr));
    if (ret < 0) {
        dbgprintf("IOP cmd: Udp bind error (%d)\n", sock);
        ExitDeleteThread();
    }

    // Do tha thing
    dbgprintf("IOP cmd: Listening\n");

    cmdListener(sock);

    ExitDeleteThread();
}


//////////////////////////////////////////////////////////////////////////
int cmdHandlerInit(void)
{
    iop_thread_t thread;
    int pid;
    int ret;

    dbgprintf("IOP cmd: Starting thread\n");

    SifInitRpc(0);

    thread.attr = 0x02000000;
    thread.option = 0;
    thread.thread = (void *)cmdThread;
    thread.stacksize = 0x800;
    thread.priority = 60; //0x1e;

    pid = CreateThread(&thread);
    if (pid >= 0) {
        ret = StartThread(pid, 0);
        if (ret < 0) {
            dbgprintf("IOP cmd: Could not start thread\n");
        }
    }
    else {
        dbgprintf("IOP cmd: Could not create thread\n");
    }
    return 0;
}
