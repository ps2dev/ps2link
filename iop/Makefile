#  _____     ___ ____
#   ____|   |    ____|      PSX2 OpenSource Project
#  |     ___|   |____       (C)2002, David Ryan ( Oobles@hotmail.com )
#  ------------------------------------------------------------------------

# Generated automatically from Makefile.in by configure.
#.SUFFIXES: .S .c .o .s .elf .irx

include ../Makefile.env

#  ------------------------------------------------------------------------
#  PROJECTS TO BUILD

all: ps2link.irx
#	cp ps2link.irx $(HOME)/ipirxs/

clean:
	rm -f *.o *.irx *.a *.elf


DEFINES +=

#  ------------------------------------------------------------------------
#  PS2IP.IRX BUILD INSTRUCTIONS

ps2link_LDFLAGS = -L$(PS2LIB)/iop/lib -L$(PS2IP)/iop/lib -s
ps2link_LDADD = -lkernel -liop_ps2ip
ps2link_OBJECTS = net_fsys.o net_fio.o ps2link.o cmdHandler.o tty.o nprintf.o

ps2link.irx: $(ps2link_OBJECTS) 
	$(IOPLINK) $(ps2link_OBJECTS) $(ps2link_LDFLAGS) $(ps2link_LDADD) -o $@

net_fsys.o: net_fsys.c net_fio.h
	$(IOPCOMPILE) $(DEFINES) -o $@ -c $<

net_fio.o: net_fio.c net_fio.h
	$(IOPCOMPILE) $(DEFINES) -o $@ -c $<

ps2link.o: ps2link.c
	$(IOPCOMPILE) $(DEFINES) -o $@ -c $<

cmdHandler.o: cmdHandler.c ../include/hostlink.h
	$(IOPCOMPILE) $(DEFINES) -o $@ -c $<

tty.o: tty.c ../include/hostlink.h
	$(IOPCOMPILE) $(DEFINES) -o $@ -c $<

nprintf.o: nprintf.c
	$(IOPCOMPILE) $(DEFINES) -o $@ -c $<
