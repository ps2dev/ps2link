/*********************************************************************
 * Copyright (C) 2003 Tord Lindstrom (pukko@home.se)
 * Copyright (C) 2003,2004 adresd (adresd_ps2dev@yahoo.com)
 * This file is subject to the terms and conditions of the PS2Link License.
 * See the file LICENSE in the main directory of this distribution for more
 * details.
 */

#include <limits.h>
#include <tamtypes.h>
#include <kernel.h>
#include <sifrpc.h>
#include <iopheap.h>
#include <loadfile.h>
#include <iopcontrol.h>
#include <fcntl.h>
#include <unistd.h>
#include <malloc.h>
#include <stdio.h>
#include <string.h>
#include "debug.h"
#include "irx_variables.h"
#include "hostlink.h"
#include "excepHandler.h"

#include <sbv_patches.h>

extern int initCmdRpc(void);
extern void pkoReset(void);

#define WELCOME_STRING "Welcome to ps2link %s\n"

#ifdef DEBUG
#define dbgprintf(args...) printf(args)
#define dbgscr_printf(args...) scr_printf(args)
#else
#define dbgprintf(args...) do { } while(0)
#define dbgscr_printf(args...) do { } while(0)
#endif

////////////////////////////////////////////////////////////////////////
// Globals
extern int userThreadID;
extern void __start(void);
extern int _end;

// Argv name+path & just path
char elfName[256] __attribute__((aligned(16)));
char elfPath[241]; // It isn't 256 because elfPath will add subpaths

////////////////////////////////////////////////////////////////////////
// Prototypes
static void loadModules(void);
static void getIpConfig(void);

////////////////////////////////////////////////////////////////////////
#define IPCONF_MAX_LEN 1024

#define DEFAULT_IP "192.168.0.10"
#define DEFAULT_NETMASK "255.255.255.0"
#define DEFAULT_GW "192.168.0.1"
#define SEPARATOR " "
// DEFAULT_IP_CONFIG should be something as "192.168.0.10 255.0.0.0 192.168.0.1"
#define DEFAULT_IP_CONFIG DEFAULT_IP SEPARATOR DEFAULT_NETMASK SEPARATOR DEFAULT_GW

// Make sure the "cached config" is in the data section
// To prevent it from being "zeroed" on a restart of ps2link
char if_conf[IPCONF_MAX_LEN] __attribute__ ((section (".data"))) = "";
int if_conf_len __attribute__ ((section (".data"))) = 0;

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
    char buf[IPCONF_MAX_LEN+256];
    static char path[256];

    sprintf(path, "%s%s", elfPath, "IPCONFIG.DAT");
    fd = open(path, O_RDONLY);

    if (fd < 0)
    {
        scr_printf("Could not find IPCONFIG.DAT, using defaults\n"
                   "Net config: %s\n", DEFAULT_IP_CONFIG);
        // Set defaults
        memset(if_conf, 0x00, IPCONF_MAX_LEN);
        strcpy(if_conf, DEFAULT_IP_CONFIG);
        i = strlen(DEFAULT_IP_CONFIG);
        dbgscr_printf("conf len %d\n", if_conf_len);
        return;
    }

    memset(if_conf, 0x00, IPCONF_MAX_LEN);
    memset(buf, 0x00, IPCONF_MAX_LEN+256);

    len = read(fd, buf, IPCONF_MAX_LEN + 256 - 1); // Let the last byte be '\0'
    close(fd);

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
	dbgscr_printf("[%d] returned\n", ret);
}

////////////////////////////////////////////////////////////////////////
// Load all the irx modules we need
static void
loadModules(void)
{
	int ret;
	int i,t;

    dbgscr_printf("loadModules \n");

    if(if_conf_len==0)
    {
        pkoLoadModule("rom0:SIO2MAN", 0, NULL);
        pkoLoadModule("rom0:MCMAN", 0, NULL);
        pkoLoadModule("rom0:MCSERV", 0, NULL);
	    return;
    }

    if(if_conf_len==0)
	{
	    getIpConfig();
    }
    else
	{
	    i=0;
	    scr_printf("Net config: ");
       for (t = 0, i = 0; t < 3; t++) {
          scr_printf("%s  ", &if_conf[i]);
          i += strlen(&if_conf[i]) + 1;
       }
       scr_printf("\n");
    }

	    dbgscr_printf("Exec poweroff module. (%x,%d) ", (unsigned int)poweroff_irx, size_poweroff_irx);
    SifExecModuleBuffer(poweroff_irx, size_poweroff_irx, 0, NULL,&ret);
	    dbgscr_printf("[%d] returned\n", ret);
	    dbgscr_printf("Exec ps2dev9 module. (%x,%d) ", (unsigned int)ps2dev9_irx, size_ps2dev9_irx);
    SifExecModuleBuffer(ps2dev9_irx, size_ps2dev9_irx, 0, NULL,&ret);
	    dbgscr_printf("[%d] returned\n", ret);
	    dbgscr_printf("Exec ps2ip module. (%x,%d) ", (unsigned int)ps2ip_irx, size_ps2ip_irx);
    SifExecModuleBuffer(ps2ip_irx, size_ps2ip_irx, 0, NULL,&ret);
	    dbgscr_printf("[%d] returned\n", ret);
	    dbgscr_printf("Exec ps2smap module. (%x,%d) ", (unsigned int)ps2smap_irx, size_ps2smap_irx);
    SifExecModuleBuffer(ps2smap_irx, size_ps2smap_irx, if_conf_len, &if_conf[0],&ret);
	    dbgscr_printf("[%d] returned\n", ret);
	    dbgscr_printf("Exec ioptrap module. (%x,%d) ", (unsigned int)ioptrap_irx, size_ioptrap_irx);
    SifExecModuleBuffer(ioptrap_irx, size_ioptrap_irx, 0, NULL,&ret);
	    dbgscr_printf("[%d] returned\n", ret);
		dbgscr_printf("Exec ps2link module. (%x,%d) ", (unsigned int)ps2link_irx, size_ps2link_irx);
    SifExecModuleBuffer(ps2link_irx, size_ps2link_irx, 0, NULL,&ret);
	    dbgscr_printf("[%d] returned\n", ret);
	    dbgscr_printf("All modules loaded on IOP.\n");
}

////////////////////////////////////////////////////////////////////////
// Split path (argv[0]) at the last '/', '\' or ':' and initialise
// elfName (whole path & name to the elf, for example 'cdrom:\pukklink.elf')
// elfPath (path to where the elf was started, for example 'cdrom:\')
static void
setPathInfo(char *path)
{
    char *ptr;

    strcpy(elfName, path);
    strcpy(elfPath, path);


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

    dbgscr_printf("path is %s\n", elfPath);
}

void printWelcomeInfo()
{
    scr_printf(WELCOME_STRING, APP_VERSION);
    scr_printf("ps2link loaded at 0x%08X-0x%08X\n", ((u32) __start) - 8, (u32) &_end);
    scr_printf("Initializing...\n");
}


void
restartIOP()
{
//    fioExit();
    SifExitIopHeap();
    SifLoadFileExit();
    SifExitRpc();

    dbgscr_printf("reset iop\n");
    SifIopReset(NULL, 0);
    while (!SifIopSync()) ;

    dbgscr_printf("rpc init\n");
    SifInitRpc(0);

    printWelcomeInfo();
//    sio_printf("Initializing...\n");
    sbv_patch_enable_lmb();
    sbv_patch_disable_prefix_check();

//	SifLoadFileReset();
    dbgscr_printf("loading modules\n");
    loadModules();
}

////////////////////////////////////////////////////////////////////////

// We are not using time zone, so we can safe some KB
void _ps2sdk_timezone_update() {}

int
main(int argc, char *argv[])
{
    char cwd[NAME_MAX];

    SifInitRpc(0);

    init_scr();
    printWelcomeInfo();

    installExceptionHandlers();

    dbgscr_printf("Checking argv\n");

    getcwd(cwd, sizeof(cwd));
    scr_printf("Booting from %s\n", cwd);
    setPathInfo(cwd);

    if(if_conf_len==0)
    {
        scr_printf("Initial boot, will load config then reset\n");
        restartIOP();
        getIpConfig();
        pkoReset();
 	}
	else
	{
        scr_printf("Booting from %s\n", cwd);
        scr_printf("Using cached config\n");
	}

    // System initalisation
	restartIOP();

    dbgscr_printf("init cmdrpc\n");
    initCmdRpc();
    scr_printf("Ready\n");

//    SleepThread();
    ExitDeleteThread();
    return 0;
}
