/*********************************************************************
 * Copyright (C) 2003 Tord Lindstrom (pukko@home.se)
 * Copyright (C) 2003,2004 adresd (adresd_ps2dev@yahoo.com)
 * This file is subject to the terms and conditions of the PS2Link License.
 * See the file LICENSE in the main directory of this distribution for more
 * details.
 */

#include <tamtypes.h>
#include <kernel.h>
#include <sifrpc.h>
#include <iopheap.h>
#include <loadfile.h>
#include <iopcontrol.h>
#include <fileio.h>
#include <malloc.h>
#include <string.h>

#include "cd.h"
#include "hostlink.h"
#include "excepHandler.h"

#include <sbv_patches.h>

extern int initCmdRpc(void);

//#define DEBUG
#ifdef DEBUG
#define dbgprintf(args...) printf(args)
#define dbgscr_printf(args...) scr_printf(args)
#else
#define dbgprintf(args...) do { } while(0)
#define dbgscr_printf(args...) do { } while(0)
#endif

#define IRX_BUFFER_BASE 0x1F80000
int irx_buffer_addr = 0;

////////////////////////////////////////////////////////////////////////
// Globals
extern int userThreadID;

// Argv name+path & just path
char elfName[256] __attribute__((aligned(16)));
char elfPath[256];

char iomanX_path[PKO_MAX_PATH];
char ps2dev9_path[PKO_MAX_PATH];
char ps2ip_path[PKO_MAX_PATH];
char ps2smap_path[PKO_MAX_PATH];
char ps2link_path[PKO_MAX_PATH];

void *iomanX_mod, *ps2dev9_mod, *ps2ip_mod, *ps2smap_mod, *ps2link_mod;
int iomanX_size, ps2dev9_size, ps2ip_size, ps2smap_size, ps2link_size;

const char *eeloadimg = "rom0:UDNL rom0:EELOADCNF";
char *imgcmd;

// Flags for which type of boot
#define B_CD 1
#define B_MC 2
#define B_HOST 3
#define B_DMS3 4
#define B_UNKN 5
int boot;

////////////////////////////////////////////////////////////////////////
// Prototypes
static void loadModules(void);
static void pkoLoadModule(char *path, int argc, char *argv);
static void getIpConfig(void);

////////////////////////////////////////////////////////////////////////
#define IPCONF_MAX_LEN  (3*16)

char if_conf[IPCONF_MAX_LEN];
int if_conf_len;

char ip[16] __attribute__((aligned(16))) = "192.168.0.10";
char netmask[16] __attribute__((aligned(16))) = "255.255.255.0";
char gw[16] __attribute__((aligned(16))) = "192.168.0.1";

////////////////////////////////////////////////////////////////////////
// Parse network configuration from IPCONFIG.DAT
// Note: parsing really should be made more robust...
static void
getIpConfig(void)
{
    int fd;
    int i;
    int t;
    int len;
    char c;
    char buf[IPCONF_MAX_LEN];
    static char path[256];

    if (boot == B_CD) {
        sprintf(path, "%s%s;1", elfPath, "IPCONFIG.DAT");
    }
    else {
        sprintf(path, "%s%s", elfPath, "IPCONFIG.DAT");
    }
    fd = fioOpen(path, O_RDONLY);

    if (fd < 0) 
    {
        scr_printf("Could not find IPCONFIG.DAT, using defaults\n"
                   "Net config: %s  %s  %s\n", ip, netmask, gw);
        // Set defaults
        memset(if_conf, 0x00, IPCONF_MAX_LEN);
        i = 0;
        strncpy(&if_conf[i], ip, 15);
        i += strlen(ip) + 1;
        
        strncpy(&if_conf[i], netmask, 15);
        i += strlen(netmask) + 1;
        
        strncpy(&if_conf[i], gw, 15);
        i += strlen(gw) + 1;
        
        if_conf_len = i;
        dbgscr_printf("conf len %d\n", if_conf_len);
        return;
    }

    memset(if_conf, 0x00, IPCONF_MAX_LEN);
    memset(buf, 0x00, IPCONF_MAX_LEN);

    len = fioRead(fd, buf, IPCONF_MAX_LEN - 1); // Let the last byte be '\0'
    fioClose(fd);

    if (len < 0) {
        dbgprintf("Error reading ipconfig.dat\n");
        return;
    }

    dbgscr_printf("ipconfig: Read %d bytes\n", len);

    i = 0;
    // Clear out spaces (and potential ending CR/LF)
    while ((c = buf[i]) != '\0') {
        if ((c == ' ') || (c == '\r') || (c == '\n'))
            buf[i] = '\0';
        i++;
    }

    scr_printf("Net config: ");
    for (t = 0, i = 0; t < 3; t++) {
        strncpy(&if_conf[i], &buf[i], 15);
        scr_printf("%s  ", &if_conf[i]);
        i += strlen(&if_conf[i]) + 1;
    }
    scr_printf("\n");

    if_conf_len = i;
}

////////////////////////////////////////////////////////////////////////
// Wrapper to load irx module from disc/rom
static void
pkoLoadModule(char *path, int argc, char *argv)
{
    int ret;

    ret = SifLoadModule(path, argc, argv);
    if (ret < 0) {
        scr_printf("Could not load module %s: %d\n", path, ret);
        SleepThread();
    }
}

/* Load a module into RAM.  */
void * modbuf_load(const char *filename, int *filesize)
{
	void *res = NULL;
	int fd = -1, size;

	if ((fd = fioOpen(filename, O_RDONLY)) < 0)
		goto out;

	if ((size = fioLseek(fd, 0, SEEK_END)) < 0)
		goto out;

	fioLseek(fd, 0, SEEK_SET);
	fioLseek(fd, 0, SEEK_SET);

	res = (void *)irx_buffer_addr;
	irx_buffer_addr += size + 32 - (size % 16);
	dbgscr_printf(" modbuf_load : %s , %d (0x%X)\n",filename,size,(int)res);

	if (fioRead(fd, res, size) != size)
		res = NULL;

	if (filesize)
		*filesize = size;

out:
	if (fd >= 0)
		fioClose(fd);

	return res;
}

static int loadHostModBuffers()
{
    if (irx_buffer_addr == 0)
    {
    irx_buffer_addr = IRX_BUFFER_BASE;

    getIpConfig();

    if (!(iomanX_mod = modbuf_load(iomanX_path, &iomanX_size)))
        {return -1;}

    if (!(ps2dev9_mod = modbuf_load(ps2dev9_path, &ps2dev9_size)))
        {return -1;}

    if (!(ps2ip_mod = modbuf_load(ps2ip_path, &ps2ip_size)))
        return -1;

    if (!(ps2smap_mod = modbuf_load(ps2smap_path, &ps2smap_size)))
        return -1;

    if (!(ps2link_mod = modbuf_load(ps2link_path, &ps2link_size)))
        return -1;
    }
    else dbgscr_printf("Using Cached Modules\n");
    return 0;
}

////////////////////////////////////////////////////////////////////////
// Load all the irx modules we need, according to 'boot mode'
static void
loadModules(void)
{
    printf(" loadModules \n");
    if (boot == B_MC)
    {
        pkoLoadModule("rom0:SIO2MAN", 0, NULL);
        pkoLoadModule("rom0:MCMAN", 0, NULL);  	
        pkoLoadModule("rom0:MCSERV", 0, NULL); 
    }

    if (boot == B_HOST) {
        SifExecModuleBuffer(iomanX_mod, iomanX_size, 0, NULL,NULL);
        SifExecModuleBuffer(ps2dev9_mod, ps2dev9_size, 0, NULL,NULL);
        SifExecModuleBuffer(ps2ip_mod, ps2ip_size, 0, NULL,NULL);
        SifExecModuleBuffer(ps2smap_mod, ps2smap_size, if_conf_len, &if_conf[0],NULL);
        SifExecModuleBuffer(ps2link_mod, ps2link_size, 0, NULL,NULL);
    } else {
        getIpConfig();
        pkoLoadModule(iomanX_path, 0, NULL);
        pkoLoadModule(ps2dev9_path, 0, NULL);
        pkoLoadModule(ps2ip_path, 0, NULL);
        pkoLoadModule(ps2smap_path, if_conf_len, &if_conf[0]);
        pkoLoadModule(ps2link_path, 0, NULL);
    }
}

////////////////////////////////////////////////////////////////////////
// C standard strrchr func..
char *strrchr(const char *s, int i)
{
    const char *last = NULL;
    char c = i;

    while (*s) {
        if (*s == c) {
            last = s;
        }
        s++;
    }

    if (*s == c) {
        last = s;
    }

    return (char *) last;
}

////////////////////////////////////////////////////////////////////////
// Split path (argv[0]) at the last '/', '\' or ':' and initialise
// elfName (whole path & name to the elf, for example 'cdrom:\pukklink.elf')
// elfPath (path to where the elf was started, for example 'cdrom:\')
static void
setPathInfo(char *path)
{
    char *ptr;

    strncpy(elfName, path, 255);
    strncpy(elfPath, path, 255);
    elfName[255] = '\0'; 
    elfPath[255] = '\0';


    ptr = strrchr(elfPath, '/');
    if (ptr == NULL) {
        ptr = strrchr(elfPath, '\\');
        if (ptr == NULL) {
            ptr = strrchr(elfPath, ':');
            if (ptr == NULL) {
                scr_printf("Did not find path (%s)!\n", path);
                SleepThread();
            }
        }
    }
    
    ptr++;
    *ptr = '\0';

    /* Paths to modules.  */
    sprintf(iomanX_path, "%s%s", elfPath, "IOMANX.IRX");
    sprintf(ps2dev9_path, "%s%s", elfPath, "PS2DEV9.IRX");
    sprintf(ps2ip_path, "%s%s", elfPath, "PS2IP.IRX");
    sprintf(ps2smap_path, "%s%s", elfPath, "PS2SMAP.IRX");
    sprintf(ps2link_path, "%s%s", elfPath, "PS2LINK.IRX");
    if (boot == B_CD) {
	    strcat(iomanX_path, ";1");
	    strcat(ps2dev9_path, ";1");
	    strcat(ps2ip_path, ";1");
	    strcat(ps2smap_path, ";1");
	    strcat(ps2link_path, ";1");
    }

    dbgscr_printf("path is %s\n", elfPath);
}

////////////////////////////////////////////////////////////////////////
// Clear user memory
void
wipeUserMem(void)
{
    int i;
    // Whipe user mem
    for (i = 0x100000; i < 0x2000000 ; i += 64) {
        asm (
            "\tsq $0, 0(%0) \n"
            "\tsq $0, 16(%0) \n"
            "\tsq $0, 32(%0) \n"
            "\tsq $0, 48(%0) \n"
            :: "r" (i) );
    }
}

////////////////////////////////////////////////////////////////////////
// Clear user memory - Load High and Host version
void
wipeUserMemLoadHigh(void)
{
    int i;
    // Whipe user mem, apart from last bit
    for (i = 0x100000; i < 0x1F00000 ; i += 64) {
        asm (
            "\tsq $0, 0(%0) \n"
            "\tsq $0, 16(%0) \n"
            "\tsq $0, 32(%0) \n"
            "\tsq $0, 48(%0) \n"
            :: "r" (i) );
    }
}


////////////////////////////////////////////////////////////////////////
int
main(int argc, char *argv[])
{
    //    int ret;
    char *bootPath;

    init_scr();
    installExceptionHandlers();
    scr_printf("Welcome to ps2link v1.22\n");
#ifdef _LOADHIGHVER
    scr_printf("Highload version\n");
#endif

    // argc == 0 usually means naplink..
    if (argc == 0) {
        bootPath = "host:";
    }
    // reload1 usually gives an argc > 60000 (yea, this is kinda a hack..)
    else if (argc != 1) {
        bootPath = "mc0:/BWLINUX/";
    }
    else {
        bootPath = argv[0];
    }

    SifInitRpc(0);
    cdvdInit(CDVD_INIT_NOWAIT);
    dbgscr_printf("Checking argv\n");
    boot = 0;

    setPathInfo(bootPath);

    if (!strncmp(bootPath, "cdrom", strlen("cdrom"))) {
        // Booting from cd
        scr_printf("Booting from cdrom (%s)\n", bootPath);
        boot = B_CD;
    }
    else if(!strncmp(bootPath, "mc", strlen("mc"))) {
        // Booting from my mc
        scr_printf("Booting from mc dir (%s)\n", bootPath);
        boot = B_MC;
    }
    else if(!strncmp(bootPath, "host", strlen("host"))) {
        // Host
        scr_printf("Booting from host (%s)\n", bootPath);
        boot = B_HOST;
    }
    else {
        // Unknown
        scr_printf("Booting from unrecognized place %s\n", bootPath);
        boot = B_UNKN;
    }

    // System initalisation
    if (boot == B_HOST) {
        if (loadHostModBuffers() < 0) {
            dbgscr_printf("Unable to load modules from host:!\n");
            SleepThread();
	}
    }
	
//    if (boot == B_MC)
//        imgcmd = (char *)eeloadimg;

    cdvdInit(CDVD_EXIT);
    cdvdExit();
    fioExit();
    SifExitIopHeap();
    SifLoadFileExit();
    SifExitRpc();

    dbgscr_printf("reset iop\n");
    SifIopReset(imgcmd, 0);
    while (SifIopSync()) ;

    /* whipe user memory */
    dbgscr_printf("clear mem\n");
#ifdef _LOADHIGHVER
    wipeUserMemLoadHigh();
#else
    if (boot == B_HOST)
        wipeUserMemLoadHigh();
    else
        wipeUserMem();
#endif

    dbgscr_printf("rpc init\n");
    SifInitRpc(0);
    cdvdInit(CDVD_INIT_NOWAIT);

    scr_printf("Initalizing...\n");
    sbv_patch_enable_lmb();
    sbv_patch_disable_prefix_check();

    dbgscr_printf("loading modules\n");
    loadModules();

    dbgscr_printf("init cmdrpc\n");
    initCmdRpc();

    scr_printf("Ready\n");
    printf("Main done\n");

    //    SleepThread();
    ExitDeleteThread();
    return 0;
}
