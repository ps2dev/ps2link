# Compilation variables

# Set this to 1 to enable debug mode
DEBUG = 0

# Set this to 1 to build a highloading version, 0 for normal low version
LOADHIGH ?= 0

# Set this to 1 to enable zero-copy on fileio writes.
ZEROCOPY = 0

# Set this to 1 to power off the ps2 when the reset button is tapped
# otherwise it will try and reset ps2link
PWOFFONRESET = 1

# Set to the path where ps2eth is located
PS2ETH = $(PS2DEV)/ps2eth

BIN2C=bin2c
RM=rm -f

#
# You shouldn't need to modify anything below this point
#

include $(PS2SDK)/Defs.make

EEFILES=ee/ps2link.elf

IRXFILES=\
	ps2link.irx \
	ioptrap.irx \
	ps2dev9.irx \
	poweroff.irx \
	ps2ip.irx \
	netman.irx \
	smap.irx
EE_IRX_OBJS = $(addprefix ee/, $(addsuffix _irx.o, $(basename $(IRXFILES))))
EE_IRX_OBJS += ps2ip_nm_irx.o
# Where to find the IRX files
vpath %.irx $(PS2SDK)/iop/irx/
vpath %.irx iop/

# Rule to generate them
ee/%_irx.o: %.irx
	$(BIN2C) $< $*_irx.c $*_irx
	$(EE_CC) -c $*_irx.c -o ee/$*_irx.o
	rm $*_irx.c

# The 'minus' sign is not handled well...
ps2ip_nm_irx.o: ps2ip-nm.irx
	$(BIN2C) $< $*.c $*
	$(EE_CC) -c $*.c -o ee/$*.o
	rm $*.c

export DEBUG LOADHIGH ZEROCOPY PWOFFONRESET

TARGETS = iop builtins ee

all: $(TARGETS)
	@for file in $(EEFILES); do \
		new=`echo $${file/*\//}|tr "[:lower:]" "[:upper:]"`; \
		ps2-packer $$file bin/$$new; \
	done;

ee:
	$(MAKE) -C ee

iop:
	$(MAKE) -C iop

clean:
	$(MAKE) -C ee clean
	$(MAKE) -C iop clean
	@rm -f ee/*_irx.o bin/*.ELF bin/*.IRX

docs:
	doxygen doxy.conf

builtins: $(EE_IRX_OBJS)

.PHONY: iop ee
