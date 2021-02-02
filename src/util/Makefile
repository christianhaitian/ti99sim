# TI-99/sim src/util makefile

include ../../rules.mak

ifeq ($(OS),OS_LINUX)
CFLAGS	+= `sdl2-config --cflags | sed 's|-I/|-isystem/|g'`
XLIBS	+= `sdl2-config --libs`
endif

ifeq ($(OS),OS_MACOSX)
CFLAGS	+= -I ~/Library/Frameworks/SDL.framework/Headers
XLIBS	+= -F ~/Library/Frameworks -framework SDL
XLIBS	+= -lobjc -framework Cocoa
SDLLIBS	:= SDLMain.o
endif

XLIBS	+= -lrt -lpthread
XLIBS	+= `pkg-config --libs openssl`

FILES	+= catalog.cpp
FILES	+= convert.cpp
FILES	+= decode.cpp
FILES	+= disk.cpp
FILES	+= dumpcpu.cpp
FILES	+= dumpgrom.cpp
FILES	+= dumpspch.cpp
FILES	+= list.cpp
FILES	+= mkspch.cpp
FILES	+= say.cpp

LIBS	+= ti-core.a

TARGET	+= catalog
TARGET	+= convert-ctg
TARGET	+= decode
TARGET	+= disk
TARGET	+= dumpcpu
TARGET	+= dumpgrom
TARGET	+= dumpspch
TARGET	+= list
TARGET	+= mkcart
TARGET	+= mkspch
TARGET	+= say

vpath %.a ../core/$(CFG)
vpath %.o ../console/$(CFG):../sdl/$(CFG)

BINDIR	:= ../../bin

TARGETS	:= $(TARGET:%=$(BINDIR)/%)

all: $(CFG) $(BINDIR) $(TARGETS)

clean:
	@-rm -Rf *~ $(CFG) $(TARGETS)

$(BINDIR):
	mkdir -p $@

$(BINDIR)/catalog: $(CFG)/catalog.o $(LIBS)
	$(CXX) -o $@ $(LFLAGS) $^ $(XLIBS)

$(BINDIR)/convert-ctg: $(CFG)/convert.o $(LIBS)
	$(CXX) -o $@ $(LFLAGS) $^ $(XLIBS)

$(BINDIR)/decode: $(CFG)/decode.o $(LIBS)
	$(CXX) -o $@ $(LFLAGS) $^ $(XLIBS)

$(BINDIR)/disk: $(CFG)/disk.o $(LIBS)
	$(CXX) -o $@ $(LFLAGS) $^ $(XLIBS)

$(BINDIR)/dumpcpu: $(CFG)/dumpcpu.o $(LIBS)
	$(CXX) -o $@ $(LFLAGS) $^ $(XLIBS)

$(BINDIR)/dumpgrom: $(CFG)/dumpgrom.o gpl.o $(LIBS)
	$(CXX) -o $@ $(LFLAGS) $^ $(XLIBS)

$(BINDIR)/dumpspch: $(CFG)/dumpspch.o $(LIBS)
	$(CXX) -o $@ $(LFLAGS) $^ $(XLIBS)

$(BINDIR)/list: $(CFG)/list.o $(LIBS)
	$(CXX) -o $@ $(LFLAGS) $^ $(XLIBS)

$(BINDIR)/mkcart: $(CFG)/mkcart.o $(LIBS)
	$(CXX) -o $@ $(LFLAGS) $^ $(XLIBS)

$(BINDIR)/mkspch: $(CFG)/mkspch.o $(LIBS)
	$(CXX) -o $@ $(LFLAGS) $^ $(XLIBS)

$(BINDIR)/say: $(CFG)/say.o tms9919-sdl.o $(LIBS) $(SDLLIBS)
	$(CXX) -o $@ $(LFLAGS) $^ $(XLIBS)

-include $(FILES:%.cpp=$(CFG)/%.dep)
