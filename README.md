# ps2link

[![CI](https://github.com/ps2dev/ps2link/workflows/CI/badge.svg)](https://github.com/ps2dev/ps2link/actions?query=workflow%3ACI)

    PS2Link (C) 2003 Tord Lindstrom (pukko@home.se)
            (C) 2003,2004 adresd (adresd_ps2dev@yahoo.com)
            (C) 2003,2004,2005 Khaled (khaled@w-arts.com)
            (C) 2019,2020,2021 fjtrujy (fjtrujy@gmail.com)

Please read the file LICENSE regarding PS2Link licensing.

PS2Link is a 'bootloader' which, used together with an Ethernet driver and
a TCP/IP stack, enables you to download and execute software on your PS2.

It is designed to run from memory card, cdrom or host drives.

It loads all IRX's at startup and IPCONFIG.DAT for the network settings.
The IRX's and the IPCONFIG.DAT should be in the directory which PS2LINK is loaded from.

## Required modules

`PS2Link` requires the following IRX modules:

    PS2LINK.IRX               from: ps2link
    PS2DEV9.IRX                     ps2sdk
    PS2IP-NM.IRX                    ps2sdk
    NETMAN.IRX                      ps2sdk
    SMAP.IRX                        ps2sdk
    IOPTRAP.IRX                     ps2sdk
    POWEROFF.IRX                    ps2sdk
    UDPTTY.IRX                      ps2sdk

## Compilation

Building `ps2link` just requires project `PS2SDK`.

For building against ps2sdk make sure `PS2SDK` is set to your ps2sdk release
dir.

    make clean all

Credit for the icon logo goes to Revolt from `ps2dev`.

NOTES + WARNINGS:
IPCONFIG.DAT FILENAME SHOULD BE UPPERCASE.

IPCONFIG.DAT uses the following format:
PS2IPADDRESS NETMASK GATEWAYIP
seperated by a single space.

If you have any questions or bugreports about ps2link go to forums.ps2dev.org.
