

all:
	$(MAKE) -C ee
	$(MAKE) -C iop

clean:
	$(MAKE) -C ee clean
	$(MAKE) -C iop clean
