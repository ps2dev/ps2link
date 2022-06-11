/*********************************************************************
 * Copyright (C) 2003 Tord Lindstrom (pukko@home.se)
 * Copyright (C) 2003,2004 adresd (adresd_ps2dev@yahoo.com)
 * This file is subject to the terms and conditions of the PS2Link License.
 * See the file LICENSE in the main directory of this distribution for more
 * details.
 */

#include <stdio.h>
#include <string.h>
#include <limits.h>
#include <fcntl.h>
#include <unistd.h>

#include <kernel.h>
#include <sifrpc.h>
#include <iopheap.h>
#include <loadfile.h>
#include <iopcontrol.h>
#include <sbv_patches.h>
#include <debug.h>


#include "irx_variables.h"
#include "hostlink.h"
#include "excepHandler.h"
#include "cmdHandler.h"
#include "globals.h"

////////////////////////////////////////////////////////////////////////

// Argv name+path & just path
char elfName[NAME_MAX] __attribute__((aligned(16)));
static char elfPath[NAME_MAX - 14]; // It isn't 256 because elfPath will add subpaths

////////////////////////////////////////////////////////////////////////
#define IPCONF_MAX_LEN 64 // Don't reduce even more this value

// Make sure the "cached config" is in the data section
// To prevent it from being "zeroed" on a restart of ps2link
char if_conf[IPCONF_MAX_LEN] __attribute__((section(".data"))) = "";
int if_conf_len __attribute__((section(".data"))) = 0;

////////////////////////////////////////////////////////////////////////

static void printIpConfig(void)
{
    int i;
    int t;

    scr_printf("Net config: ");
    for (t = 0, i = 0; t < 3; t++) {
        scr_printf("%s  ", &if_conf[i]);
        i += strlen(&if_conf[i]) + 1;
    }
    scr_printf("\n");
}

// Parse network configuration from IPCONFIG.DAT
// Note: parsing really should be made more robust...
static int readIpConfigFromFile(char *buf)
{
    int fd;
    int len;
    char path[256];

    sprintf(path, "%s%s", elfPath, "IPCONFIG.DAT");
    fd = open(path, O_RDONLY);

    if (fd < 0) {
        dbgprintf("Error reading ipconfig.dat\n");
        return -1;
    }

    len = read(fd, buf, IPCONF_MAX_LEN - 1); // Let the last byte be '\0'
    close(fd);

    if (len < 0) {
        dbgprintf("Error reading ipconfig.dat\n");
        return -1;
    }

    dbgscr_printf("ipconfig: Read %d bytes\n", len);
    return 0;
}

static int readDefaultIpConfig(char *buf)
{
    int i = 0;

    strncpy(&buf[i], DEFAULT_IP, 15);
    i += strlen(&buf[i]) + 1;
    strncpy(&buf[i], DEFAULT_NETMASK, 15);
    i += strlen(&buf[i]) + 1;
    strncpy(&buf[i], DEFAULT_GW, 15);

    return 0;
}

static void getIpConfig(void)
{
    int i;
    int t;
    char c;
    char buf[IPCONF_MAX_LEN];

    // Clean buffers
    memset(buf, 0x00, IPCONF_MAX_LEN);
    memset(buf, 0x00, IPCONF_MAX_LEN);

    if (readIpConfigFromFile(buf))
        readDefaultIpConfig(buf);

    i = 0;
    // Clear out spaces (and potential ending CR/LF)
    while ((c = buf[i]) != '\0') {
        if ((c == ' ') || (c == '\r') || (c == '\n'))
            buf[i] = '\0';
        i++;
    }

    scr_printf("Saving IP config...\n");
    for (t = 0, i = 0; t < 3; t++) {
        strncpy(&if_conf[i], &buf[i], 15);
        i += strlen(&if_conf[i]) + 1;
    }

    if_conf_len = i;
}

////////////////////////////////////////////////////////////////////////
// Wrapper to load irx module from disc/rom
static void pkoLoadModule(char *path, int argc, char *argv)
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
static void loadModules(void)
{
    int ret;

    if (if_conf_len == 0) {
        pkoLoadModule("rom0:SIO2MAN", 0, NULL);
        pkoLoadModule("rom0:MCMAN", 0, NULL);
        pkoLoadModule("rom0:MCSERV", 0, NULL);
        return;
    }

    dbgscr_printf("Exec poweroff module. (%x,%d) ", (unsigned int)poweroff_irx, size_poweroff_irx);
    SifExecModuleBuffer(poweroff_irx, size_poweroff_irx, 0, NULL, &ret);
    dbgscr_printf("[%d] returned\n", ret);
    dbgscr_printf("Exec ps2dev9 module. (%x,%d) ", (unsigned int)ps2dev9_irx, size_ps2dev9_irx);
    SifExecModuleBuffer(ps2dev9_irx, size_ps2dev9_irx, 0, NULL, &ret);
    dbgscr_printf("[%d] returned\n", ret);
    dbgscr_printf("Exec netman module. (%x,%d) ", (unsigned int)netman_irx, size_netman_irx);
    SifExecModuleBuffer(netman_irx, size_netman_irx, 0, NULL, &ret);
    dbgscr_printf("[%d] returned\n", ret);
    dbgscr_printf("Exec smap module. (%x,%d) ", (unsigned int)smap_irx, size_smap_irx);
    SifExecModuleBuffer(smap_irx, size_smap_irx, 0, NULL, &ret);
    dbgscr_printf("[%d] returned\n", ret);
    dbgscr_printf("Exec ps2ip module. (%x,%d) ", (unsigned int)ps2ip_nm_irx, size_ps2ip_nm_irx);
    SifExecModuleBuffer(ps2ip_nm_irx, size_ps2ip_nm_irx, if_conf_len, &if_conf[0], &ret);
    dbgscr_printf("[%d] returned\n", ret);
    dbgscr_printf("Exec udptty module. (%x,%d) ", (unsigned int)udptty_irx, size_udptty_irx);
    SifExecModuleBuffer(&udptty_irx, size_udptty_irx, 0, NULL, &ret);
    dbgscr_printf("[%d] returned\n", ret);
    dbgscr_printf("Exec ioptrap module. (%x,%d) ", (unsigned int)ioptrap_irx, size_ioptrap_irx);
    SifExecModuleBuffer(ioptrap_irx, size_ioptrap_irx, 0, NULL, &ret);
    dbgscr_printf("[%d] returned\n", ret);
    dbgscr_printf("Exec ps2link module. (%x,%d) ", (unsigned int)ps2link_irx, size_ps2link_irx);
    SifExecModuleBuffer(ps2link_irx, size_ps2link_irx, 0, NULL, &ret);
    dbgscr_printf("[%d] returned\n", ret);
    dbgscr_printf("All modules loaded on IOP.\n");
}

////////////////////////////////////////////////////////////////////////
// Split path (argv[0]) at the last '/', '\' or ':' and initialise
// elfName (whole path & name to the elf, for example 'cdrom:\pukklink.elf')
// elfPath (path to where the elf was started, for example 'cdrom:\')
static void setPathInfo(char *path)
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

static void printWelcomeInfo()
{
    scr_printf("Welcome to ps2link %s\n", APP_VERSION);
    scr_printf("ps2link loaded at 0x%08X-0x%08X\n", ((u32)__start) - 8, (u32)&_end);
    scr_printf("Initializing...\n");
}

static void restartIOP()
{
    SifExitIopHeap();
    SifLoadFileExit();
    SifExitRpc();

    dbgscr_printf("reset iop\n");

    SifInitRpc(0);

    //Reboot IOP
    while (!SifIopReset("", 0)) {};
    while (!SifIopSync()) {};

    //Initialize SIF services
    SifInitRpc(0);
    SifLoadFileInit();
    SifInitIopHeap();
    sbv_patch_enable_lmb();
    sbv_patch_disable_prefix_check();

    dbgscr_printf("reseted iop\n");
}

////////////////////////////////////////////////////////////////////////

// This function is defined as weak in ps2sdkc, so how
// we are not using time zone, so we can safe some KB
void _ps2sdk_timezone_update() {}

int main(int argc, char *argv[])
{
    char cwd[NAME_MAX];

    SifInitRpc(0);
    init_scr();

    installExceptionHandlers();
    restartIOP();
    printWelcomeInfo();

    getcwd(cwd, sizeof(cwd));
    scr_printf("Booting from %s\n", cwd);
    setPathInfo(cwd);

    dbgscr_printf("loading modules\n");
    loadModules();

    if (if_conf_len == 0) {
        scr_printf("Initial boot, will load config then reset\n");
        getIpConfig();
        pkoReset(); // Will restart execution
    }

    scr_printf("Using cached config\n");
    printIpConfig();

    dbgscr_printf("init cmdrpc\n");
    initCmdRpc();
    scr_printf("Ready with %i memory bytes\n", GetMemorySize());

    ExitDeleteThread();
    return 0;
}
