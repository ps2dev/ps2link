SHELL=/bin/bash
EEFILES=ee/ps2link.elf
IRXFILES=iop/ps2link.irx $(PS2DEV)/ps2ip/iop/bin/ps2ip.irx \
	$(PS2DEV)/ps2eth/bin/ps2smap.irx \
	$(PS2DEV)/ps2drv/iop/iomanX/iomanX.irx \
	$(PS2DEV)/ps2drv/iop/ps2dev9/ps2dev9.irx

all:
	$(MAKE) -C ee
	$(MAKE) -C iop

clean:
	$(MAKE) -C ee clean
	$(MAKE) -C iop clean

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
	tar -jcf ps2link.tar.bz2 *.IRX *.ELF

docs:
	doxygen doxy.conf
