/*********************************************************************
 * Copyright (C) 2003 Tord Lindstrom (pukko@home.se)
 * Copyright (C) 2004 adresd (adresd_ps2dev@yahoo.com)
 * This file is subject to the terms and conditions of the PS2Link License.
 * See the file LICENSE in the main directory of this distribution for more
 * details.
 */

#include <tamtypes.h>
#include <kernel.h>
#include <sifrpc.h>
#include <loadfile.h>
#include <iopcontrol.h>
#include <fileio.h>

#include "cd.h"
#include "byteorder.h"
#include "ps2regs.h"
#include "hostlink.h"

#ifdef DEBUG
#define dbgprintf(args...) printf(args)
#else
#define dbgprintf(args...) do { } while(0)
#endif

////////////////////////////////////////////////////////////////////////
// Prototypes
static int cmdThread(void);
static int pkoExecEE(pko_pkt_execee_req *cmd);
static int pkoStopVU(pko_pkt_stop_vu *);
static int pkoStartVU(pko_pkt_start_vu *);
static int pkoDumpMem(pko_pkt_dump_mem *);
static int pkoDumpReg(pko_pkt_dump_regs *);
static void pkoReset(void);
static int pkoLoadElf(char *path);
static int pkoGSExec(pko_pkt_gsexec_req *);

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
int excepscrdump = 1;


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
// takes cmd->data and sends it to GIF via path1
static int
pkoGSExec(pko_pkt_gsexec_req *cmd) {
    char data[16384];
	int fd;
    int len;
	fd = fioOpen(cmd->file, O_RDONLY);
	if ( fd < 0 ) {
		return fd;
	}
	len = fioRead(fd, data, 128);
	fioClose(fd);
    // stop/reset dma02

    // dmasend via GIF channel
    asm __volatile__(
        "move   $10, %0;"
        "move   $11, %1;"
        "lui    $8, 0x1001;"
        "sw     $11, -24544($8);"
        "sw     $10, -24560($8);"
        "ori    $9, $0, 0x101;"
        "sw     $9, -24576($8);"
	:: "r" (data), "r" ((ntohs(cmd->size))/16) );

    // dmawait for GIF channel
    asm __volatile__(
        "lui    $8, 0x1001;"
        "dmawait:"
        "lw     $9, -24576($8);"
        "nop;"
        "andi   $9, $9, 0x100;"
        "nop;"
        "bnez   $9, dmawait;"
        "nop;"
       );
	return 0;
}

////////////////////////////////////////////////////////////////////////
// command to dump cmd->size bytes of memory from cmd->offset
static int 
pkoDumpMem(pko_pkt_dump_mem *cmd) {
	int fd;
	unsigned int offset;
	unsigned int size;
    char path[PKO_MAX_PATH];
    int ret;
   	size = ntohl(cmd->size);
    offset = ntohl(cmd->offset);
	scr_printf("dump mem from 0x%x, size %d\n", 
		ntohl(cmd->offset), ntohl(cmd->size)
		);
    memcpy(path, cmd->argv, PKO_MAX_PATH);
	fd = fioOpen(path, O_WRONLY|O_CREAT);
	if ( fd > 0 ) {
		if ((ret = fioWrite(fd, offset, size)) > 0) {
		} else {
			printf("EE: pkoDumpMem() fioWrite failed\n");
			return fd;
		}
	}
	fioClose(fd);
	return ret;
}

////////////////////////////////////////////////////////////////////////
// command to dump various registers ( gs|vif|intc etc )
static int
pkoDumpReg(pko_pkt_dump_regs *cmd) {
	int fd, ret;
    char path[PKO_MAX_PATH];
	unsigned int i, j, size;
	unsigned int dmaregs[51] = {
		0x1000e000, 0x1000e010, 0x1000e020, 0x1000e030,
		0x1000e040, 0x1000e050, 0x1000e060, 0x1000f520,				// Dma common registers
		0x10008000, 0x10008010, 0x10008020, 0x10008030, 0x10008040, 0x10008050, // DMA0
		0x10009000, 0x10009010, 0x10009020, 0x10009030, 0x10009040, 0x10009050, // DMA1
		0x1000a000, 0x1000a010, 0x1000a020, 0x1000a030, 0x1000a040, 0x1000a050, // DMA2
		0x1000b000, 0x1000b010, 0x1000b020,							// DMA3
		0x1000b400, 0x1000b410, 0x1000b420, 0x1000b430,				// DMA4
		0x1000c000, 0x1000c010, 0x1000c020,							// DMA5
		0x1000c400, 0x1000c410, 0x1000c420,							// DMA6
		0x1000c800, 0x1000c810, 0x1000c820,							// DMA7
		0x1000d000, 0x1000d010, 0x1000d020, 0x1000d080,				// DMA8
		0x1000d400, 0x1000d410, 0x1000d420, 0x1000d430, 0x1000d480	// DMA9
	};
	unsigned int intcregs[2] = {
		0x1000F000, 0x1000F010
	};
	unsigned int gsregs[2] = {
		0x12001000, 0x12001080
	};
	unsigned int gifregs[8] = {
		0x10003020, 0x10003040, 0x10003050, 0x10003060,
	   	0x10003070, 0x10003080, 0x10003090, 0x100030a0
	};
	unsigned int timerregs[14] = {
		0x10000000, 0x10000010, 0x10000020, 0x10000030,
		0x10000800, 0x10000810, 0x10000820, 0x10000830,
		0x10001000, 0x10001010, 0x10001020, 
		0x10001800, 0x10001810, 0x10001820
	};
	unsigned int sifregs[1] = {
		0x1000F230
	};
	unsigned int vif0regs[19] = {
		0x10003800, 0x10003810, 0x10003820, 0x10003830, 0x10003840,
		0x10003850, 0x10003860, 0x10003870, 0x10003880, 0x10003890,
		0x100038d0, 0x10003900, 0x10003910, 0x10003920, 0x10003930,
		0x10003940, 0x10003950, 0x10003960, 0x10003970
	};		
	unsigned int vif1regs[23] = {
		0x10003c00, 0x10003c10, 0x10003c20, 0x10003c30, 0x10003c40,
	   	0x10003c50, 0x10003c60, 0x10003c70, 0x10003c80, 0x10003c90,
	   	0x10003ca0, 0x10003cb0, 0x10003cc0, 0x10003cd0, 0x10003ce0,
	   	0x10003d00, 0x10003d10, 0x10003d20, 0x10003d30, 0x10003d40,
	   	0x10003d50, 0x10003d60, 0x10003d70
	};
	unsigned int fiforegs[5] = {
		0x10005000, 0x10007000
	};
	unsigned int ipuregs[4] = {
		0x10002000, 0x10002010, 0x10002020, 0x10002030
	};
	unsigned int REGALL_SIZE = 
		sizeof(dmaregs)+sizeof(intcregs)+sizeof(timerregs)+
		sizeof(gsregs)+sizeof(sifregs)+sizeof(fiforegs)+
		sizeof(gifregs)+sizeof(vif0regs)+sizeof(vif1regs)+
		sizeof(ipuregs);
	unsigned int regs[REGALL_SIZE];

	size = 0;
	j = 0;
	switch(ntohl(cmd->regs)) {
		case REGDMA:
			for (i = 0; i < sizeof(dmaregs)/4; i++) {
				regs[j++] = *((volatile unsigned int *)dmaregs[i]);
			}
			size = sizeof(dmaregs);
			break;
		case REGINTC:
			for (i = 0; i < sizeof(intcregs)/4; i++) {
				regs[j++] = *((volatile unsigned int *)intcregs[i]);
			}
			size = sizeof(intcregs);
			break;
		case REGTIMER:
			for (i = 0; i < sizeof(timerregs)/4; i++) {
				regs[j++] = *((volatile unsigned int *)timerregs[i]);
			}
			size = sizeof(timerregs);
			break;
		case REGGS:
			for (i = 0; i < sizeof(gsregs)/4; i++) {
				regs[j++] = *((volatile unsigned int *)gsregs[i]);
			}
			size = sizeof(gsregs);
			break;
		case REGSIF:
			for (i = 0; i < sizeof(sifregs)/4; i++) {
				regs[j++] = *((volatile unsigned int *)sifregs[i]);
			}
			size = sizeof(sifregs);
			break;
		case REGFIFO:
			for (i = 0; i < sizeof(fiforegs)/4; i++) {
				regs[j++] = *((volatile unsigned int *)fiforegs[i]);
			}
			size = sizeof(fiforegs);
			break;
		case REGGIF:
			for (i = 0; i < sizeof(gifregs)/4; i++) {
				regs[j++] = *((volatile unsigned int *)gifregs[i]);
			}
			size = sizeof(gifregs);
			break;
		case REGVIF0:
			for (i = 0; i < sizeof(vif0regs)/4; i++) {
				regs[j++] = *((volatile unsigned int *)vif0regs[i]);
			}
			size = sizeof(vif0regs);
			break;
		case REGVIF1:
			for (i = 0; i < sizeof(vif1regs)/4; i++) {
				regs[j++] = *((volatile unsigned int *)vif1regs[i]);
			}
			size = sizeof(vif1regs);
			break;
		case REGIPU:
			for (i = 0; i < sizeof(ipuregs)/4; i++) {
				regs[j++] = *((volatile unsigned int *)ipuregs[i]);
			}
			size = sizeof(ipuregs);
			break;
		case REGVU1:
			asm(
				// stop VU0 and VU1
				"ori	$8, $0, 0x101;"
				"ctc2	$8, $28;"
				"vnop;"
				// FIXME remember to save/restore vi1 and vf1

				// Save VU1 floats
				"move	$8, %0;"			// save away dst
				"ori	$9, $0, 1024;"		// start of vu1 mapped floats
				"ctc2   $9, $vi01;"			// ctc2 t0, vi1
				"ori	$9, $0, 31;"
				"vu1_float_loop:"
				"vlqi	$vf01, ($vi01++);"
				"sqc2   $vf01, 0($8);"
				"addiu	$9, -1;"
				"addiu	$8, 16;"
				"bnez	$9, vu1_float_loop;"
				"nop;"

				// Save VU1 Integers
				"ori	$9, $0, 1056;"
				"ctc2   $9, $vi01;"			// ctc2 t0, vi1
				"ori	$9, $0, 15;"
				"vu1_int_loop:"
				"vlqi	$vf01, ($vi01++);"
				"sqc2   $vf01, 0($8);"
				"addiu	$9, -1;"
				"addiu	$8, 16;"
				"bnez	$9, vu1_int_loop;"
				"nop;"

				// Stop GIF
				// Save VU1 Control registers
				"ori	$9, $0, 1072;"
				"ctc2	$9, $vi01;"
				"vlqi	$vf01, ($vi0++);"
				"sqc2   $vf01, 0($8);"
				"vlqi	$vf01, ($vi0++);"
				"sqc2   $vf01, 16($8);"
				"vlqi	$vf01, ($vi0++);"
				"sqc2   $vf01, 32($8);"
				"viaddi	$vi01, $vi01, 1;"
				"vlqi	$vf01, ($vi0++);"
				"sqc2   $vf01, 48($8);"
				"vlqi	$vf01, ($vi0++);"
				"sqc2   $vf01, 64($8);"
				"vlqi	$vf01, ($vi0++);"
				"sqc2   $vf01, 80($8);"
				"vlqi	$vf01, ($vi0++);"
				"sqc2   $vf01, 96($8);"
				"viaddi	$vi01, $vi01, 2;"
				"vlqi	$vf01, ($vi0++);"
				"sqc2   $vf01, 112($8);"

				// FIXME remember to save/restore vi1 and vf1

				"ori	$8, $0, 0x202;"
				"ctc2	$8, $28;"
				"vnop;"
				:: "r" (regs)
			   );
			size = 896;
			break;
		case REGVU0:
			asm(
				// stop VU0
				"ori	$8, $0, 0x1;"
				"ctc2	$8, $28;"
				"vnop;"
				// Dump VU0 float registers
				"sqc2   $vf0, 0(%0);"
				"sqc2   $vf1, 16(%0);"
				"sqc2   $vf2, 32(%0);"
				"sqc2   $vf3, 48(%0);"
				"sqc2   $vf4, 64(%0);"
				"sqc2   $vf5, 80(%0);"
				"sqc2   $vf6, 96(%0);"
				"sqc2   $vf7, 112(%0);"
				"sqc2   $vf8, 128(%0);"
				"sqc2   $vf9, 144(%0);"
				"sqc2   $vf10, 160(%0);"
				"sqc2   $vf11, 176(%0);"
				"sqc2   $vf12, 192(%0);"
				"sqc2   $vf13, 208(%0);"
				"sqc2   $vf14, 224(%0);"
				"sqc2   $vf15, 240(%0);"
				"sqc2   $vf16, 256(%0);"
				"sqc2   $vf17, 272(%0);"
				"sqc2   $vf18, 288(%0);"
				"sqc2   $vf19, 304(%0);"
				"sqc2   $vf20, 320(%0);"
				"sqc2   $vf21, 336(%0);"
				"sqc2   $vf22, 352(%0);"
				"sqc2   $vf23, 368(%0);"
				"sqc2   $vf24, 384(%0);"
				"sqc2   $vf25, 400(%0);"
				"sqc2   $vf26, 416(%0);"
				"sqc2   $vf27, 432(%0);"
				"sqc2   $vf28, 448(%0);"
				"sqc2   $vf29, 464(%0);"
				"sqc2   $vf30, 480(%0);"
				"sqc2   $vf31, 496(%0);"

				// Save VU0 integer registers
				"cfc2	$8, $vi00;"
				"sq		$8, 512(%0);"
				"cfc2	$8, $vi01;"
				"sq		$8, 528(%0);"
				"cfc2	$8, $vi02;"
				"sq		$8, 544(%0);"
				"cfc2	$8, $vi03;"
				"sq		$8, 560(%0);"
				"cfc2	$8, $vi04;"
				"sq		$8, 576(%0);"
				"cfc2	$8, $vi05;"
				"sq		$8, 592(%0);"
				"cfc2	$8, $vi06;"
				"sq		$8, 608(%0);"
				"cfc2	$8, $vi07;"
				"sq		$8, 624(%0);"
				"cfc2	$8, $vi08;"
				"sq		$8, 640(%0);"
				"cfc2	$8, $vi09;"
				"sq		$8, 656(%0);"
				"cfc2	$8, $vi10;"
				"sq		$8, 672(%0);"
				"cfc2	$8, $vi11;"
				"sq		$8, 688(%0);"
				"cfc2	$8, $vi12;"
				"sq		$8, 704(%0);"
				"cfc2	$8, $vi13;"
				"sq		$8, 720(%0);"
				"cfc2	$8, $vi14;"
				"sq		$8, 736(%0);"
				"cfc2	$8, $vi15;"
				"sq		$8, 752(%0);"
				
				// Reset VU0
				"ori	$8, $0, 0x2;"
				"ctc2	$8, $28;"
				"vnop;"
				:: "r" (regs));
			size = 896;
			break;
		case REGALL:
			for (i = 0; i < sizeof(dmaregs)/4; i++) {
				regs[j++] = *((volatile unsigned int *)dmaregs[i]);
			}
			for (i = 0; i < sizeof(intcregs)/4; i++) {
				regs[j++] = *((volatile unsigned int *)intcregs[i]);
			}
			for (i = 0; i < sizeof(timerregs)/4; i++) {
				regs[j++] = *((volatile unsigned int *)timerregs[i]);
			}
			for (i = 0; i < sizeof(gsregs)/4; i++) {
				regs[j++] = *((volatile unsigned int *)gsregs[i]);
			}
			for (i = 0; i < sizeof(sifregs)/4; i++) {
				regs[j++] = *((volatile unsigned int *)sifregs[i]);
			}
			for (i = 0; i < sizeof(fiforegs)/4; i++) {
				regs[j++] = *((volatile unsigned int *)fiforegs[i]);
			}
			for (i = 0; i < sizeof(gifregs)/4; i++) {
				regs[j++] = *((volatile unsigned int *)gifregs[i]);
			}
			for (i = 0; i < sizeof(vif0regs)/4; i++) {
				regs[j++] = *((volatile unsigned int *)vif0regs[i]);
			}
			for (i = 0; i < sizeof(vif1regs)/4; i++) {
				regs[j++] = *((volatile unsigned int *)vif1regs[i]);
			}
			for (i = 0; i < sizeof(ipuregs)/4; i++) {
				regs[j++] = *((volatile unsigned int *)ipuregs[i]);
			}
			size = REGALL_SIZE;
			break;
	}

    memcpy(path, cmd->argv, PKO_MAX_PATH);
	fd = fioOpen(path, O_WRONLY|O_CREAT);
	if ( fd > 0 ) {
		if ((ret = fioWrite(fd, regs, size)) > 0) {
		} else {
			scr_printf("EE: pkoDumpReg() fioWrite failed\n");
			return fd;
		}
	}
	fioClose(fd);
	return ret;
}

////////////////////////////////////////////////////////////////////////
// command to stop VU0/1 and VIF0/1
static int
pkoStopVU(pko_pkt_stop_vu *cmd) {
	if ( ntohs(cmd->vpu) == 0 ) {
		asm(
			"ori     $25, $0, 0x1;"
			"ctc2    $25, $28;"
			"sync.p;"
			"vnop;"
		   );
		VIF0_FBRST = 0x2;
	} else if ( ntohs(cmd->vpu) == 1 ) {
		asm(
			"ori     $25, $0, 0x100;"
			"ctc2    $25, $28;"
			"sync.p;"
			"vnop;"
		   );
		VIF1_FBRST = 0x2;
	}
	return 0;
}

////////////////////////////////////////////////////////////////////////
// command to reset VU0/1 VIF0/1
static int
pkoStartVU(pko_pkt_start_vu *cmd) {
	if ( ntohs(cmd->vpu) == 0 ) {
		asm(
			"ori     $25, $0, 0x2;"
			"ctc2    $25, $28;"
			"sync.p;"
			"vnop;"
		   );
		VIF0_FBRST = 0x8;
	} else if ( ntohs(cmd->vpu) == 1 ) {
		asm(
			"ori     $25, $0, 0x200;"
			"ctc2    $25, $28;"
			"sync.p;"
			"vnop;"
		   );
		VIF1_FBRST = 0x8;
	}
	return 0;
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

    if ((boot == B_MC) || (boot == B_HOST) || (boot == B_UNKN || B_DMS3)) {
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
    asm __volatile__("ei");
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
    printf("EE: Cmd thread\n");

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
        case PKO_NETDUMP_CMD:
            excepscrdump = 0;
            break;        
        case PKO_SCRDUMP_CMD:
            excepscrdump = 1;
            break;        
        case PKO_EXECEE_CMD: 
            dbgprintf("EE: Rpc EXECEE called\n");
            ret = pkoExecEE(pkt);
            break;
		case PKO_START_VU:
			dbgprintf("EE: Start VU\n");
			ret = pkoStartVU(pkt);
			break;
		case PKO_STOP_VU:
			dbgprintf("EE: Stop VU\n");
			ret = pkoStopVU(pkt);
			break;
		case PKO_DUMP_MEM:
			dbgprintf("EE: Dump mem\n");
			ret = pkoDumpMem(pkt);
			break;
		case PKO_DUMP_REG:
			dbgprintf("EE: Dump reg\n");
			ret = pkoDumpReg(pkt);
			break;
		case PKO_GSEXEC_CMD:
			dbgprintf("EE: GS Exec\n");
			ret = pkoGSExec(pkt);
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
