EE_BIN  = ps2link.elf
EE_OBJS = ps2link.o cmdHandler.o excepHandler.o exceptions.o cd.o
EE_INCS += -I../include

# This is for the sbv patch
SBVLITE = $(PS2SDK)/sbv
EE_INCS += -I$(SBVLITE)/include
EE_LDFLAGS += -L$(SBVLITE)/lib -s
EE_LIBS += -lpatches -ldebug

# Normal low loading version
ifeq ($(BUILTIN_IRXS),1)
LOADADDR  = 0xb0000
else
LOADADDR  = 0xc0000
endif
STACKADDR = 0x100000

# This is to build a highloading version
ifeq ($(LOADHIGH),1)
ifeq ($(BUILTIN_IRXS),1)
LOADADDR  = 0x1ef0000
else
LOADADDR  = 0x1f00000
endif
STACKADDR = 0x1f40000
EE_CFLAGS += -D_LOADHIGHVER
endif

# This is to builtin the IRXs into ps2link
ifeq ($(BUILTIN_IRXS),1)
EE_CFLAGS += -DBUILTIN_IRXS
EE_LDFLAGS += ps2link_irx.o ps2ip_irx.o ps2smap_irx.o iomanX_irx.o ps2dev9_irx.o ioptrap_irx.o
endif

# This is to enable the debug mode into ps2link
ifeq ($(DEBUG),1)
EE_CFLAGS += -DDEBUG
endif

LDPARAMS := -Wl,--defsym -Wl,_stack_size=0x8000 -Wl,--defsym -Wl,_stack=$(STACKADDR)
EE_LDFLAGS += -Wl,-Ttext -Wl,$(LOADADDR) $(LDPARAMS)

all: $(EE_BIN)

clean:
	-rm -f $(EE_OBJS) $(EE_BIN)

check: $(EE_BIN)
	@ee-readelf -l $(EE_BIN) | awk ' /LOAD/ { if ((spare = (-($$4 + $$6) + ("'$(STACKADDR)'" - "0x8000"))) <= 0) { print "PS2Link is too big."; exit -1; } else { printf("PS2Link has %i (0x%x) spare bytes\n", spare, spare) } } '

include $(PS2SDK)/Defs.make
include Rules.make
