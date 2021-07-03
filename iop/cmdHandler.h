/*********************************************************************
 * Copyright (C) 2003 Tord Lindstrom (pukko@home.se)
 * Copyright (C) 2004 adresd (adresd_ps2dev@yahoo.com)
 * Copyright (C) 2021 fjtrujy (fjtrujy@gmail.com)
 * This file is subject to the terms and conditions of the PS2Link License.
 * See the file LICENSE in the main directory of this distribution for more
 * details.
 */

#ifndef CMD_HANDLER_H
#define CMD_HANDLER

extern int excepscrdump;

unsigned int pkoSendSifCmd(unsigned int cmd, void *src, unsigned int len);
int cmdHandlerInit(void);

#endif /* CMD_HANDLER */