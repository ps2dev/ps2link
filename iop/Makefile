#  _____     ___ ____
#   ____|   |    ____|      PSX2 OpenSource Project
#  |     ___|   |____       (C)2002, David Ryan ( Oobles@hotmail.com )
#  ------------------------------------------------------------------------

# Generated automatically from Makefile.in by configure.
#.SUFFIXES: .S .c .o .s .elf .irx

IOP_BIN  = ps2link.irx
IOP_OBJS = net_fsys.o net_fio.o ps2link.o cmdHandler.o tty.o nprintf.o imports.o

IOP_INCS += -I../include
IOP_LIBS += 
IOP_LDFLAGS += 
IOP_CFLAGS += -Wall

# Enable zero-copy on fileio writes.
ifeq ($(ZEROCOPY),1)
IOP_CFLAGS += -DZEROCOPY
endif


all: $(IOP_BIN)

include $(PS2SDK)/Defs.make
include $(PS2SDK)/iop/Rules.make
