#  _____     ___ ____
#   ____|   |    ____|      PSX2 OpenSource Project
#  |     ___|   |____       (C)2002, David Ryan ( Oobles@hotmail.com )
#  ------------------------------------------------------------------------

# Generated automatically from Makefile.in by configure.
#.SUFFIXES: .S .c .o .s .elf .irx

IOP_BIN  = ps2link.irx
IOP_OBJS = net_fsys.o net_fio.o ps2link.o cmdHandler.o tty.o nprintf.o \
	imports.o

IOP_INCS += -I../include

ifeq ($(DEBUG),1)
IOP_CFLAGS += -DDEBUG
else
IOP_LDFLAGS += -s
endif

all: $(IOP_BIN)

include $(PS2DRV)/Defs.make
include $(PS2DRV)/iop/Rules.make
