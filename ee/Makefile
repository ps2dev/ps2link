
EE_BIN  = ps2link.elf
EE_OBJS = ps2link.o cmdHandler.o excepHandler.o exceptions.o cd.o

EE_INCS += -I../include

LOADADDR  = 0xc0000
STACKADDR = 0x100000
ifeq ($(DEBUG),1)
LOADADDR  = 0xd00000
STACKADDR = 0xd40000
endif
LDPARAMS := -Wl,--defsym -Wl,_stack_size=0x8000 -Wl,--defsym -Wl,_stack=$(STACKADDR)
EE_LDFLAGS += -Wl,-Ttext -Wl,$(LOADADDR) $(LDPARAMS) -s

all: $(EE_BIN)

clean:
	-rm -f $(EE_OBJS) $(EE_BIN)

include $(PS2LIB)/Makefile.pref
include $(PS2LIB)/Makefile.eeglobal
