/* ps2link.h - Global header for ps2link.
 *
 * Copyright (c) 2003  Marcus R. Brown <mrbrown@0xd6.org>
 *
 * See the LICENSE file included in this distribution for licensing terms.
 */

#ifndef PS2LINK_H
#define PS2LINK_H

#include "tamtypes.h"

#include "hostlink.h"

/* Definitions shared by both portions of ps2link.  */
#ifndef ALIGN
#define ALIGN(x, align) (((x)+((align)-1))&~((align)-1))
#endif

#ifdef DEBUG
#define dbgprintf(args...) printf(args)
#define dbgscr_printf(args...) scr_printf(args)
#else
#define dbgprintf(args...) do { } while(0)
#define dbgscr_printf(args...) do { } while(0)
#endif

#ifdef _EE

#include "string.h"
#include "kernel.h"
#include "sifrpc.h"
#include "iopheap.h"
#include "loadfile.h"
#include "iopcontrol.h"
#include "fileio.h"

#include "cd.h"
#include "hostlink.h"
#include "excepHandler.h"

#define S_SCREEN	0x01
#define S_HOST		0x02

/* Print to the screen, the host, or both.  */
#define S_PRINTF(flags, format, args...)		\
	do {						\
		if ((flags) & S_SCREEN)			\
			scr_printf(format , ## args);	\
		if ((flags) & S_HOST)			\
			printf(format, ## args);	\
	} while (0);

enum _boot { BOOT_UNKNOWN, BOOT_FULL, BOOT_MEM, BOOT_HOST };
extern enum _boot boot;

/* From cmdHandler.c  */
int initCmdRpc(void);

#else	/* !_EE */

#include "loadcore.h"
#include "intrman.h"
#include "modload.h"
#include "thbase.h"
#include "thsemap.h"
#include "sifdma.h"
#include "sifrpc.h"
#include "ioman.h"
#include "stdlib.h"
#include "stdio.h"
#include "cdvd.h"

#include "ps2ip.h"

#define ntohl(x) htonl(x)
#define ntohs(x) htons(x)

/* From iop/net_fsys.c  */
int fsysMount(void);
int fsysUnmount(void);

/* From iop/cmdHandler.c  */
int cmdHandlerInit(void);

/* From tty.c  */
int ttyMount(void);

/* From iop/nprintf.c  */
int naplinkRpcInit(void);

#endif /* _EE */

#endif /* PS2LINK_H */
