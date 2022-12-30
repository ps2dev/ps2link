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

RM=rm -f

#
# You shouldn't need to modify anything below this point
#
include $(PS2SDK)/Defs.make

EE_BIN_PKD = bin/PS2LINK.ELF
EE_BIN = ee/ps2link.elf

all: $(EE_BIN_PKD)

$(EE_BIN): ee
$(EE_BIN_PKD): $(EE_BIN)
	ps2-packer $< $@ > /dev/null

export DEBUG LOADHIGH ZEROCOPY PWOFFONRESET

ee: iop
	$(MAKE) -C ee

iop:
	$(MAKE) -C iop

clean:
	$(MAKE) -C ee clean
	$(MAKE) -C iop clean
	@rm -f ee/*_irx.o ee/*_irx.c bin/*.ELF bin/*.IRX

docs:
	doxygen doxy.conf

.PHONY: iop ee
