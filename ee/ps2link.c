/*********************************************************************
 * Copyright (C) 2003 Tord Lindstrom (pukko@home.se)
 * Copyright (c) 2003 Marcus R. Brown <mrbrown@0xd6.org>
 *
 * This file is subject to the terms and conditions of the PS2Link License.
 * See the file LICENSE in the main directory of this distribution for more
 * details.
 */

#include "ps2link.h"

////////////////////////////////////////////////////////////////////////
// Globals

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

static boot_info_t boot_info[] =
{ {"cdrom", BOOT_FULL}, {"mc", BOOT_MEM}, {"host", BOOT_HOST},
 {"unknown", BOOT_UNKNOWN} };
#define BOOT_INFO_LEN	((sizeof boot_info)/(sizeof(boot_info_t)))

boot_info_t *cur_boot_info;

////////////////////////////////////////////////////////////////////////
// Prototypes
static int load_modules(void);
static int pkoLoadModule(const char *path, int arglen, void *args);
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

    sprintf(path, "%s%s", elfPath, "IPCONFIG.DAT");
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
        D_PRINTF("conf len %d\n", if_conf_len);
        return;
    }

    memset(if_conf, 0x00, IPCONF_MAX_LEN);
    memset(buf, 0x00, IPCONF_MAX_LEN);

    len = fioRead(fd, buf, IPCONF_MAX_LEN - 1); // Let the last byte be '\0'
    fioClose(fd);

    if (len < 0) {
        D_PRINTF("Error reading ipconfig.dat\n");
        return;
    }

    D_PRINTF("ipconfig: Read %d bytes\n", len);

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
    scr_printf(" \n");

    if_conf_len = i;
}

////////////////////////////////////////////////////////////////////////
// Wrapper to load irx module from disc/rom
static int pkoLoadModule(const char *path, int arglen, void *args)
{
    int ret;

    if ((ret = SifLoadModule(path, arglen, args)) < 0)
	    S_PRINTF(S_SCREEN, "Could not load module '%s': error %d.\n", path, ret);

    return ret;
}

////////////////////////////////////////////////////////////////////////
// Wrapper to load module from disc/rom/mc
// Max irx size hardcoded to 300kb atm..
static int pkoLoadMcModule(char *path, int arglen, void *args)
{
	void *iopmem;
	int res = -1;
	
	if (!(iopmem = SifAllocIopHeap(1024*300))) {
		S_PRINTF(S_SCREEN, "Unable to reserve memory on the IOP (LMM).");
		return res;
	}

	if ((res = SifLoadIopHeap(path, iopmem)) < 0) {
		S_PRINTF(S_SCREEN, "Unable to load module '%s' into IOP RAM (%d).\n",
				path, res);
		SifFreeIopHeap(iopmem);
		return res;
	}

        res = SifLoadModuleBuffer(iopmem, arglen, args);

	SifFreeIopHeap(iopmem);
	return res;
}

static int pkoLoadHostModule(void *module, u32 size, int arglen, void *args)
{
	SifDmaTransfer_t dmat;
	void *iopmem;
	int res = -1;

	if (!(iopmem = SifAllocIopHeap(size))) {
		S_PRINTF(S_SCREEN, "Unable to reserve memory on the IOP (LHM).");
		return res;
	}

	dmat.src = module;
	dmat.dest = iopmem;
	dmat.size = size;
	dmat.attr = 0;

	while (!SifSetDma(&dmat, 1))
		nopdelay();

	res = SifLoadModuleBuffer(iopmem, arglen, args);

	SifFreeIopHeap(iopmem);
	return res;
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

static int preload_host_modules()
{
    if (!(ps2ip_mod = modbuf_load(ps2ip_path, &ps2ip_size)))
        return -1;

    if (!(ps2smap_mod = modbuf_load(ps2smap_path, &ps2smap_size)))
        return -1;

    if (!(ps2link_mod = modbuf_load(ps2link_path, &ps2link_size)))
        return -1;

    return 0;
}

static int preload_mc_modules()
{
	if (pkoLoadModule("rom0:SECRMAN", 0, NULL) < 0)
		return -1;
	if (pkoLoadModule("rom0:SIO2MAN", 0, NULL) < 0)
		return -1;
	if (pkoLoadModule("rom0:MCMAN", 0, NULL) < 0)
		return -1;

	return 0;
}

////////////////////////////////////////////////////////////////////////
// Load all the irx modules we need, according to 'boot mode'
static int load_modules(void)
{
	int boot = cur_boot_info->boot;

	if (boot == BOOT_MEM && preload_mc_modules() != 0)
		return -1;

	switch (boot) {
		case BOOT_FULL:
			if (pkoLoadModule(ps2ip_path, 0, NULL) < 0)
				return -1;
			if (pkoLoadModule(ps2smap_path, if_conf_len, &if_conf[0]) < 0)
				return -1;
			if (pkoLoadModule(ps2link_path, 0, NULL) < 0)
				return -1;
			break;
		case BOOT_MEM:
        		if (pkoLoadMcModule(ps2ip_path, 0, NULL) < 0)
				return -1;
        		if (pkoLoadMcModule(ps2smap_path, if_conf_len, &if_conf[0]) < 0)
				return -1;
			if (pkoLoadMcModule(ps2link_path, 0, NULL) < 0)
				return -1;
			break;
		case BOOT_HOST:
			if (pkoLoadHostModule(ps2ip_mod, ps2ip_size, 0, NULL) < 0)
				return -1;
			if (pkoLoadHostModule(ps2smap_mod, ps2smap_size, if_conf_len, &if_conf[0]) < 0)
				return -1;
			if (pkoLoadHostModule(ps2link_mod, ps2link_size, 0, NULL) < 0)
				return -1;
			break;
		default:
			S_PRINTF(S_SCREEN, "ps2link doesn't know how to load 'unknown' modules.\n");
			return -1;
	}

	return 0;
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

    D_PRINTF("path is %s\n", elfPath);
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
    int i, fd;

    init_scr();
    installExceptionHandlers();
    S_PRINTF(S_SCREEN, "Welcome to ps2link v1.1\n");

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
    D_PRINTF("Checking argv\n");

    setPathInfo(bootPath);

    /* Find our boot prefix and determine our current boot method.  */
    for (i = 0; i < BOOT_INFO_LEN; i++) {
	    if (!strncmp(bootPath, boot_info[i].prefix, strlen(boot_info[i].prefix))) {
		    cur_boot_info = &boot_info[i];
		    S_PRINTF(S_SCREEN, "Booting from '%s'.\n", cur_boot_info->prefix);
		    break;
	    }
    }

    if (cur_boot_info == NULL) {
	    cur_boot_info = &boot_info[BOOT_INFO_LEN - 1];
	    S_PRINTF(S_SCREEN, "Booting from unknown location '%s'.\n", bootPath);
    }

    /* System initialization: if we're booting from the memory card, we'll need the
       appropiate modules in place to read the IPCONFIG.DAT file.  */
    if (cur_boot_info->boot == BOOT_MEM) {
	fd = fioOpen("mc:bar", O_RDONLY);
	/* If the filesystem doesn't exist, try to load the MC modules.  */
	if (fd == -19 && preload_mc_modules() != 0) {
		S_PRINTF(S_SCREEN, "Unable to load required MC modules, exiting.\n");
		goto out;
	}

	if (fd >= 0)
		fioClose(fd);
    }

    getIpConfig();

    /* Modules from the host filesystem must be loaded into RAM before we
       reboot, because the host: fs is inaccessible after the IOP is reset.  */
    if (cur_boot_info->boot == BOOT_HOST && preload_host_modules() != 0) {
	S_PRINTF(S_SCREEN, "Unable to load required modules, exiting.\n");
	goto out;
    }

    /* TODO: Check that eeloadcnf exists in ROM.  */
    imgcmd = (char *)eeloadimg;

    D_PRINTF("Shutting down subsystems.\n");
    cdvdInit(CDVD_EXIT);
    cdvdExit();
    fioExit();
    SifExitIopHeap();
    SifLoadFileExit();
    SifExitRpc();

    D_PRINTF("reset iop\n");
    SifIopReset(imgcmd, 0);
    while (!SifIopSync()) ;

    /* If loading from host:, we might be loaded high, so don't wipe ourselves!  */
    if (cur_boot_info->boot != BOOT_HOST)
        wipeUserMem();

    D_PRINTF("rpc init\n");
    SifInitRpc(0);
    cdvdInit(CDVD_INIT_NOWAIT);

    S_PRINTF(S_SCREEN, "ps2link initializing... ");
    D_PRINTF("loading modules\n");
    if (load_modules() < 0) {
	    S_PRINTF(S_SCREEN, "Unable to load required boot modules, exiting.\n");
	    goto out;
    }

    D_PRINTF("init cmdrpc\n");
    initCmdRpc();

    S_PRINTF(S_SCREEN|S_HOST, "Ready.\n\n");

out:
    ExitDeleteThread();
    return 0;
}
