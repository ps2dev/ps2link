
EE_BIN  = ps2link.elf
EE_OBJS = ps2link.o cmdHandler.o excepHandler.o exceptions.o cd.o
EE_INCS += -I../include

# This is for the sbv patch
SBVLITE = $(PS2DEV)/sbv-1.0-lite
EE_INCS += -I$(SBVLITE)/include
EE_LDFLAGS += -L$(SBVLITE)/lib
EE_LIBS += -lsbv_patches

# Set this to 1 to build a highloading version, 0 for normal low version
LOADHIGH = 1

# Normal low loading version
LOADADDR  = 0xc0000
STACKADDR = 0x100000

# This is to build a highloading version
ifeq ($(LOADHIGH),1)
LOADADDR  = 0x1f00000
STACKADDR = 0x1f40000
EE_CFLAGS += -D_LOADHIGHVER
endif

LDPARAMS := -Wl,--defsym -Wl,_stack_size=0x8000 -Wl,--defsym -Wl,_stack=$(STACKADDR)
EE_LDFLAGS += -Wl,-Ttext -Wl,$(LOADADDR) $(LDPARAMS) -s

all: $(EE_BIN)

clean:
	-rm -f $(EE_OBJS) $(EE_BIN)

include $(PS2LIB)/Makefile.pref
include $(PS2LIB)/Makefile.eeglobal
