SHELL=/bin/bash
EEFILES=ee/ps2link.elf
IRXFILES=iop/ps2link.irx $(PS2SDK)/iop/irx/ps2ip.irx \
	$(PS2DEV)/ps2eth/smap/ps2smap.irx \
	$(PS2SDK)/iop/irx/iomanX.irx \
	$(PS2SDK)/iop/irx/ps2dev9.irx

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
	tar -jcf ps2link.tar.bz2 *.IRX *.ELF system.cnf IPCONFIG.DAT

docs:
	doxygen doxy.conf
