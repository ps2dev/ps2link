# Compilation variables

# Set this to enable debug mode
DEBUG = 0

# Set this to 1 to build a highloading version, 0 for normal low version
LOADHIGH = 0

# Set this to 1 to build ps2link with all the needed IRX builtins
BUILTIN_IRXS = 0

# Set this to 1 to enable zero-copy on fileio writes.
ZEROCOPY = 0

include $(PS2SDK)/Defs.make

SHELL=/bin/bash
EEFILES=ee/ps2link.elf
BIN2S=$(PS2SDK)/bin/bin2s
RM=rm -f
IRXFILES=iop/ps2link.irx $(PS2SDK)/iop/irx/ps2ip.irx \
	$(PS2DEV)/ps2eth/smap/ps2smap.irx \
	$(PS2SDK)/iop/irx/iomanX.irx \
	$(PS2SDK)/iop/irx/ioptrap.irx \
	$(PS2SDK)/iop/irx/ps2dev9.irx
VARIABLES=DEBUG=$(DEBUG) LOADHIGH=$(LOADHIGH) BUILTIN_IRXS=$(BUILTIN_IRXS) ZEROCOPY=$(ZEROCOPY)

ifeq ($(BUILTIN_IRXS),1)
TARGETS = iop builtins ee
else
TARGETS = ee iop
endif

all: $(TARGETS)

ee:
	$(VARIABLES) $(MAKE) -C ee

iop:
	$(VARIABLES) $(MAKE) -C iop

clean:
	$(MAKE) -C ee clean
	$(MAKE) -C iop clean

# Creates a zip from what you have
dist: 
	@for file in $(IRXFILES); do \
		new=`echo $${file/*\//}|tr "[:lower:]" "[:upper:]"`; \
		cp $$file bin/$$new; \
	done;
	@for file in $(EEFILES); do \
		new=`echo $${file/*\//}|tr "[:lower:]" "[:upper:]"`; \
		cp $$file bin/$$new; \
	done;
	@cd bin; \
	tar -jcf ps2link.tar.bz2 *.IRX *.ELF system.cnf IPCONFIG.DAT extra.cnf

RELEASE_FILES=bin/*IRX bin/*DAT bin/*cnf bin/*ELF LICENSE README
#
# Creates zip with iso and all necessary files of last release
release:
	@rm -rf RELEASE
	@mkdir -p RELEASE
	@VERSION=`cvs log Makefile | grep -A 1 symbolic | tail -1 | awk '{print substr($$1, 0, length($$1)-1)}'`; \
	cd RELEASE; \
	cvs co -r $$VERSION ps2link; \
	cd ps2link; \
	make; \
	mkdir -p bin; \
	for file in $(IRXFILES); do \
		new=`echo $${file/*\//}|tr "[:lower:]" "[:upper:]"`; \
		cp $$file bin/$$new; \
	done; \
	for file in $(EEFILES); do \
		new=`echo $${file/*\//}|tr "[:lower:]" "[:upper:]"`; \
		cp $$file bin/$$new; \
	done; \
	dd if=/dev/zero of=bin/dummy bs=1024 count=28672; \
	ps2mkisofs -o ps2link_$$VERSION.iso bin/; \
	rm bin/dummy; \
	tar -jcf ps2link_$$VERSION.tbz $(RELEASE_FILES) ps2link_$$VERSION.iso

docs:
	doxygen doxy.conf

builtins:
	@for file in $(IRXFILES); do \
		basefile=$${file/*\//}; \
		basefile=$${basefile/\.*/}; \
		lname=$${basefile}_irx; \
		sfile=$${lname}.s; \
		ofile=$${lname}.o; \
		echo "Embedding IRX file $$basefile"; \
		$(BIN2S) $$file $$sfile $$lname; \
		$(EE_CC) -c -o ee/$$ofile $$sfile; \
		$(RM) $$sfile; \
	done;

.PHONY: iop ee
