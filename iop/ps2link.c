/*********************************************************************
 * Copyright (C) 2003 Tord Lindstrom (pukko@home.se)
 * This file is subject to the terms and conditions of the PS2Link License.
 * See the file LICENSE in the main directory of this distribution for more
 * details.
 */

#include "ps2link.h"

////////////////////////////////////////////////////////////////////////
// main
//   start threads & init rpc & filesys
int
_start( int argc, char **argv)
{
    sceCdInit(1);
    sceCdStop();

    SifInitRpc(0);

    if ((argc < 2) || (strncmp(argv[1], "-notty", 6))) {
        ttyMount();
        // Oh well.. There's a bug in either smapif or lwip's etharp
        // that thrashes udp msgs which are queued while waiting for arp
        // request
        // alas, this msg will probably not be displayed
        printf("tty mounted\n");
    }

    fsysMount();
    cmdHandlerInit();
    naplinkRpcInit();
    return 0;
}

