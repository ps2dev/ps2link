#  _____     ___ ____
#   ____|   |    ____|      PSX2 OpenSource Project
#  |     ___|   |____       (C)2002, David Ryan ( Oobles@hotmail.com )
#  ------------------------------------------------------------------------

# Generated automatically from Makefile.in by configure.
#.SUFFIXES: .S .c .o .s .elf .irx

PS2IP := $(PS2DEV)/ps2ip

IOP_BIN  = ps2link.irx
IOP_OBJS = net_fsys.o net_fio.o ps2link.o cmdHandler.o tty.o nprintf.o

IOP_INCS += -I../include -I$(PS2IP)/include -I$(PS2IP)/iop/ps2ip/include
IOP_LIBS += -liop_ps2ip
IOP_LDFLAGS += -L$(PS2IP)/iop/lib

all: $(IOP_BIN)

clean:
	-rm -f $(IOP_OBJS) $(IOP_BIN)

include $(PS2LIB)/Makefile.pref
include $(PS2LIB)/Makefile.iopglobal
