/*********************************************************************
 * Copyright (C) 2003 Tord Lindstrom (pukko@home.se)
 * This file is subject to the terms and conditions of the PS2Link License.
 * See the file LICENSE in the main directory of this distribution for more
 * details.
 */

#include "ps2link.h"

////////////////////////////////////////////////////////////////////////
// Globals

enum _boot boot;

// Argv name+path & just path
char elfName[256] __attribute__((aligned(16)));
char elfPath[256];

char ps2ip_path[PKO_MAX_PATH];
char ps2smap_path[PKO_MAX_PATH];
char ps2link_path[PKO_MAX_PATH];

void *ps2ip_mod, *ps2smap_mod, *ps2link_mod;
int ps2ip_size, ps2smap_size, ps2link_size;

const char *eeloadimg = "rom0:UDNL rom0:EELOADCNF";
char *imgcmd;

// Flags for which type of boot
#define B_CD 1
#define B_MC 2
#define B_HOST 3
#define B_DMS3 4
#define B_UNKN 5


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

////////////////////////////////////////////////////////////////////////
// Wrapper to load module from disc/rom/mc
// Max irx size hardcoded to 300kb atm..
static void
pkoLoadMcModule(char *path, int argc, char *argv)
{
    void *iop_mem;
    int ret;

    dbgscr_printf("LoadMcModule %s\n", path);
    iop_mem = SifAllocIopHeap(1024*300);
    if (iop_mem == NULL) {
        scr_printf("allocIopHeap failed\n");
        SleepThread();
    }
    ret = SifLoadIopHeap(path, iop_mem);
    if (ret < 0) {
        scr_printf("loadIopHeap %s ret %d\n", path, ret);
        SleepThread();
    }
    else {
        ret = SifLoadModuleBuffer(iop_mem, argc, argv);
        if (ret < 0) {
            scr_printf("loadModuleBuffer %s ret %d\n", path, ret);
            SleepThread();
        }
    }
    SifFreeIopHeap(iop_mem);
}

static void pkoLoadHostModule(void *module, u32 size, int arglen, void *args)
{
	SifDmaTransfer_t dmat;
	void *iopmem;
	int res;

	if (!(iopmem = SifAllocIopHeap(size))) {
		scr_printf("allocIopHeap failed\n");
		SleepThread();
	}

	dmat.src = module;
	dmat.dest = iopmem;
	dmat.size = size;
	dmat.attr = 0;

	while (!SifSetDma(&dmat, 1))
		nopdelay();

	if ((res = SifLoadModuleBuffer(iopmem, arglen, args)) < 0) {
		SifFreeIopHeap(iopmem);
		SleepThread();
	}

	SifFreeIopHeap(iopmem);
}

static void *morecore(u32 size)
{
	extern void *_end;
	static void *coreptr = &_end;
	static void *res;

	/* Align the current core pointer.  */
	coreptr = (void *)ALIGN((u32)coreptr, 16);
	res = coreptr;

	/* Allocate this buffer.  */
	coreptr += size;

	return res;
}

/* Load a module into RAM.  */
void * modbuf_load(const char *filename, int *filesize)
{
	void *res = NULL;
	int fd = -1, size, ret;

	if ((fd = fioOpen(filename, O_RDONLY)) < 0)
		goto out;

	if ((size = fioLseek(fd, 0, SEEK_END)) < 0)
		goto out;

	fioLseek(fd, 0, SEEK_SET);
	if (!size) {
		S_PRINTF(S_SCREEN, "Module '%s' has zero size, unable to preload.\n", filename);
		goto out;
	}

	if ((res = morecore(size)) == NULL)
		goto out;

	if ((ret = fioRead(fd, res, size)) < 0) {
		res = NULL;
	}

	if (filesize)
		*filesize = size;

out:
	if (fd >= 0)
		fioClose(fd);

	return res;
}

static int loadHostModBuffers()
{
    if (!(ps2ip_mod = modbuf_load(ps2ip_path, &ps2ip_size)))
        return -1;

    if (!(ps2smap_mod = modbuf_load(ps2smap_path, &ps2smap_size)))
        return -1;

    if (!(ps2link_mod = modbuf_load(ps2link_path, &ps2link_size)))
        return -1;

    return 0;
}

////////////////////////////////////////////////////////////////////////
// Load all the irx modules we need, according to 'boot mode'
static void
loadModules(void)
{
    if ((boot == B_MC)  || (boot == B_DMS3))
    {
        pkoLoadModule("rom0:SIO2MAN", 0, NULL);
        pkoLoadModule("rom0:MCMAN", 0, NULL);  	
        pkoLoadModule("rom0:MCSERV", 0, NULL); 
    }

    if (boot == B_MC) {
        pkoLoadMcModule(ps2ip_path, 0, NULL);
        pkoLoadMcModule(ps2smap_path, if_conf_len, &if_conf[0]);
        pkoLoadMcModule(ps2link_path, 0, NULL);
        //    pkoLoadMcModule(FSYS "PS2LINK.IRX" FSVER, strlen("-notty") + 1, "-notty");
    } else if (boot == B_HOST) {
        pkoLoadHostModule(ps2ip_mod, ps2ip_size, 0, NULL);
	pkoLoadHostModule(ps2smap_mod, ps2smap_size, if_conf_len, &if_conf[0]);
	pkoLoadHostModule(ps2link_mod, ps2link_size, 0, NULL);
    } else {
        pkoLoadModule(ps2ip_path, 0, NULL);
        pkoLoadModule(ps2smap_path, if_conf_len, &if_conf[0]);
        pkoLoadModule(ps2link_path, 0, NULL);
    }
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
    sprintf(ps2ip_path, "%s%s", elfPath, "PS2IP.IRX");
    sprintf(ps2smap_path, "%s%s", elfPath, "PS2SMAP.IRX");
    sprintf(ps2link_path, "%s%s", elfPath, "PS2LINK.IRX");
    if (boot == B_CD) {
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
int
main(int argc, char *argv[])
{
    //    int ret;
    char *bootPath;

    init_scr();
    installExceptionHandlers();
    scr_printf("Welcome to ps2link v1.1\n");

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
    else if(!strncmp(bootPath, "mc0:/PUKK", strlen("mc0:/PUKK"))) {
        // Booting from my mc
        scr_printf("Booting from mc dir (%s)\n", bootPath);
        boot = B_MC;
    }
    else if(!strncmp(bootPath, "mc0:/BWLINUX", strlen("mc0:/BWLINUX"))) {
        // Booting from linux mc
        scr_printf("Booting from rte\n", bootPath);
        boot = B_MC;
    }
    else if(!strncmp(bootPath, "mc", strlen("mc"))) {
        // DMS3 boot?
        scr_printf("Independence boot mode (%s)\n", bootPath);
        boot = B_DMS3;
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

    getIpConfig();

    // System initalisation
    if (boot == B_HOST) {
        if (loadHostModBuffers() < 0) {
            dbgscr_printf("Unable to load modules from host:!\n");
            SleepThread();
	}
    }

    /* TODO: Check that eeloadcnf exists in ROM.  */
    imgcmd = (char *)eeloadimg;

    cdvdInit(CDVD_EXIT);
    cdvdExit();
    fioExit();
    SifExitIopHeap();
    SifLoadFileExit();
    SifExitRpc();

    dbgscr_printf("reset iop\n");
    SifIopReset(imgcmd, 0);
    while (!SifIopSync()) ;

    /* If loading from host:, we might be loaded high, so don't wipe ourselves!  */
    if (boot != B_HOST)
        wipeUserMem();

    dbgscr_printf("rpc init\n");
    SifInitRpc(0);
    cdvdInit(CDVD_INIT_NOWAIT);

    scr_printf("Initalizing...\n");
    dbgscr_printf("loading modules\n");
    loadModules();

    dbgscr_printf("init cmdrpc\n");
    initCmdRpc();

    S_PRINTF(S_SCREEN|S_HOST, "ps2link ready.\n");

    //    SleepThread();
    ExitDeleteThread();
    return 0;
}
