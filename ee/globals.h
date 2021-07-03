/*********************************************************************
 * Copyright (C) 2003 Tord Lindstrom (pukko@home.se)
 * Copyright (C) 2004 adresd (adresd_ps2dev@yahoo.com)
 * Copyright (C) 2021 fjtrujy (fjtrujy@gmail.com)
 * This file is subject to the terms and conditions of the PS2Link License.
 * See the file LICENSE in the main directory of this distribution for more
 * details.
 */

#ifndef GLOBALS_H
#define GLOBALS_H

#ifdef DEBUG
#define dbgprintf(args...)     printf(args)
#define dbgscr_printf(args...) scr_printf(args)
#else
#define dbgprintf(args...) \
    do {                   \
    } while (0)
#define dbgscr_printf(args...) \
    do {                       \
    } while (0)
#endif

// DEFAULT IPCONFIG IF FILE CONFIG CAN NOT BE READ
////////////////////////////////////////////////////////////////////////

#define DEFAULT_IP      "192.168.1.10"
#define DEFAULT_NETMASK "255.255.255.0"
#define DEFAULT_GW      "192.168.1.0"
////////////////////////////////////////////////////////////////////////

// Globals
extern void __start(void);
extern int _end;

#endif /* GLOBALS_H */
