/* ps2link.h - Global header for ps2link.
 *
 * Copyright (c) 2003  Marcus R. Brown <mrbrown@0xd6.org>
 *
 * See the LICENSE file included in this distribution for licensing terms.
 */

#ifndef PS2LINK_H
#define PS2LINK_H

#include "hostlink.h"

/* Definitions shared by both portions of ps2link.  */
#ifndef ALIGN
#define ALIGN(x, align) (((x)+((align)-1))&~((align)-1))
#endif

#ifdef _EE

#include "tamtypes.h"

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

#ifdef DEBUG
#define D_PRINTF(format, args...)			\
	scr_printf(__FUNCTION__ ": " format, ## args)
#else
#define D_PRINTF(format, args...)
#endif

#define S_SCREEN	0x01
#define S_HOST		0x02

/* Print to the screen, the host, or both.  */
#define S_PRINTF(flags, format, args...)		\
	do {						\
		if ((flags) & S_SCREEN)			\
			scr_printf(format , ## args);	\
		if ((flags) & S_HOST)			\
			printf(format, ## args);	\
	} while (0)

enum _boot { BOOT_UNKNOWN, BOOT_FULL, BOOT_MEM, BOOT_HOST };

typedef struct {
	const char *prefix;
	enum _boot boot;
} boot_info_t;

extern boot_info_t *cur_boot_info;

/* From ee/ps2link.c  */
int full_reset(void);

/* From ee/cmdHandler.c  */
int initCmdRpc(void);

#else	/* !_EE */

#include "defs.h"
#include "types.h"
#include "errno.h"

#include "intrman.h"
#include "modload.h"
#include "thbase.h"
#include "thsemap.h"
#include "sifman.h"
#include "sifcmd.h"
#include "ioman.h"
#include "sysclib.h"
#include "stdio.h"
#include "cdvdman.h"

#include "ps2ip.h"

#ifdef DEBUG
#define dbgprintf(args...) printf(args)
#else
#define dbgprintf(args...) do { } while(0)
#endif

/*
 * Don't want printfs to broadcast in case more than 1 ps2 on the same network, so at
 * connect time, the remote PC's IP is stored here and used as destination for printfs.
 */
extern unsigned int remote_pc_addr;

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

#undef htons
#undef htonl
#undef ntohs
#undef ntohl

#define ntohl(x) htonl(x)
#define ntohs(x) htons(x)

static inline u16 htons(u16 x)
{
	return ((x & 0xff) << 8 ) | ((x & 0xff00) >> 8);
}

static inline u32 htonl(u32 x)
{
	return ((x & 0xff) << 24 ) | ((x & 0xff00) << 8) | 
		((x & 0xff0000) >> 8) | ((x & 0xff000000) >> 24);
}

#endif /* PS2LINK_H */
