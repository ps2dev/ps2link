
include ../Makefile.env


######################################################################

HEADERS = 	../include/hostlink.h Makefile
OFILES = 	crt0.o \
			ps2link.o \
			cmdHandler.o \
			excepHandler.o \
			exceptions.o \
			cd.o

TARGET = ps2link

######################################################################
DEFINES +=

ASFLAGS	+= -mcpu=5900 -EL -G0
CFLAGS	+=	
LDFLAGS +=
LOCAL_LIBS = -lkernel

######################################################################
$(TARGET).elf: $(OFILES)
		$(EELINK) $(EELIBDIR) -o $(TARGET).elf $(LDFLAGS) $(OFILES) $(LOCAL_LIBS)
#		$(STRIP) --strip-all $(TARGET).elf
		@echo "done"

all:	$(TARGET).elf  

dbg:	
		$(MAKE) 'LDFLAGS+=-Ttext 0xd00000' $(TARGET).elf

%.o: %.c $(HEADERS)
		$(EECOMPILE) $(DEFINES) -c $(CFLAGS) $< -o $@

%.o: %.s
		$(EECOMPILE) $(DEFINES) -xassembler-with-cpp -c $(ASFLAGS) $< -o $@

clean:
	rm -f $(OFILES) $(TARGET).elf 
