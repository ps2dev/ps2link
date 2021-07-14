EE_BIN  = ps2link.elf
EE_OBJS = ps2link.o cmdHandler.o excepHandler.o exceptions.o
EE_INCS += -I../include

# This is for the sbv patch
EE_LIBS += -lpatches -ldebug

# This is to builtin the IRXs into ps2link
EE_LDFLAGS += ps2link_irx.o ps2ip_nm_irx.o netman_irx.o smap_irx.o ioptrap_irx.o ps2dev9_irx.o poweroff_irx.o

# This is to enable the debug mode into ps2link
ifeq ($(DEBUG),1)
EE_CFLAGS += -DDEBUG -g
endif

# This is to read the closest tag version
APP_VERSION := $(shell git describe --tags --abbrev=0)
EE_CFLAGS += -DAPP_VERSION=\"$(APP_VERSION)\"

# Link with following libraries.  This is a special case, and instead of
# allowing the user to override the library order, we always make sure
# libkernel is the last library to be linked.
EE_LIBS += -lc -lkernel-nopatch

# Use custom linkfile
EE_LINKFILE = linkfile

ifeq ($(DEBUG),1)
EE_LDFLAGS += -g
else
EE_LDFLAGS += -s
endif

all: $(EE_BIN)

clean:
	-rm -f $(EE_OBJS) $(EE_BIN)

include $(PS2SDK)/Defs.make
include $(PS2SDK)/samples/Makefile.eeglobal_cpp
