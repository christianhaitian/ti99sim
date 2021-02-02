# TI-99/sim src/core makefile

include ../../rules.mak

FILES	+= cBaseObject.cpp

FILES	+= bitstream.cpp
FILES	+= cartridge.cpp
FILES	+= cf7+.cpp
FILES	+= compress.cpp
FILES	+= decode-fm.cpp
FILES	+= decode-mfm.cpp
FILES	+= decode-lzw.cpp
FILES	+= encode-fm.cpp
FILES	+= encode-mfm.cpp
FILES	+= device.cpp
FILES	+= device-support.cpp
FILES	+= disassemble.cpp
FILES	+= disk-sector.cpp
FILES	+= disk-track.cpp
FILES	+= disk-image.cpp
FILES	+= disk-media.cpp
FILES	+= disk-serializer.cpp
FILES	+= disk-serializer-anadisk.cpp
FILES	+= disk-serializer-cf7+.cpp
FILES	+= disk-serializer-hfe.cpp
FILES	+= disk-serializer-pc99.cpp
FILES	+= disk-serializer-v9t9.cpp
FILES	+= file-system.cpp
FILES	+= file-system-arc.cpp
FILES	+= file-system-disk.cpp
FILES	+= file-system-pseudo.cpp
FILES	+= fileio.cpp
FILES	+= encode-lzw.cpp
FILES	+= opcodes.cpp
FILES	+= option.cpp
FILES	+= stateobject.cpp
FILES	+= support.cpp
FILES	+= ti-disk.cpp
FILES	+= ti-pcard.cpp
FILES	+= ti994a.cpp
FILES	+= ti994a-gk.cpp
FILES	+= tms5220.cpp
FILES	+= tms9900.cpp
FILES	+= tms9901.cpp
FILES	+= tms9918a.cpp
FILES	+= tms9919.cpp

OBJS	+= $(FILES:%.cpp=$(CFG)/%.o)

TARGET  := $(CFG)/ti-core.a

all: $(CFG) $(TARGET)

clean:
	@-rm -Rf *~ $(CFG) $(TARGETS)

$(CFG)/ti-core.a: $(OBJS)
	$(AR) rc $@ $?
	$(RANLIB) $@

-include $(FILES:%.cpp=$(CFG)/%.dep)
