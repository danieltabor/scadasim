CC=gcc
CFLAGS=-Os
DST=../../pc/obj/
CONSOLE_EXE=client.console
CURSES_EXE=client.curses
SIXEL_EXE=client.sixel
LIBIEC61850A=$(DST)libiec61850-1.5.1/build/libiec61850.a
LDFLAGS=-lncurses -pthread -L$(DST)/libiec61850-1.5.1/build -liec61850
LIBFLAGS=-I$(DST)libiec61850-1.5.1/src/iec61850/inc -I$(DST)libiec61850-1.5.1/src/mms/inc -I$(DST)libiec61850-1.5.1/src/common/inc -I$(DST)libiec61850-1.5.1/hal/inc -I$(DST)libiec61850-1.5.1/src/logging -I$(DST)libiec61850-1.5.1/src/goose

help:
	@echo "make [target]"
	@echo ""
	@echo "Available targets:"
	@echo "  all        Build console, curses, and sixel"
	@echo "  console    Simple text interactive interface"
	@echo "  curses     Curese interactive interface"
	@echo "  gfx        Sixel based interactive interface"
	@echo "  clean      Remove object files, but not executable"
	@echo "  distclean  Remove all build artifacts"
	@echo "  help       Prints this message"

all: $(CONSOLE_EXE) $(CURSES_EXE) $(SIXEL_EXE)

console: $(CONSOLE_EXE)

$(CONSOLE_EXE): $(LIBIEC61850A) client.c
	$(CC) -o $(CONSOLE_EXE) client.c $(LIBFLAGS) -lncurses -pthread -L$(DST)/libiec61850-1.5.1/build -liec61850

curses: $(CURSES_EXE)

$(CURSES_EXE): $(LIBIEC61850A) client.c
	$(CC) -o $(CURSES_EXE) -DUSE_CURSES client.c $(LIBFLAGS) -lncurses -pthread -L$(DST)/libiec61850-1.5.1/build -liec61850

gfx: $(SIXEL_EXE)

$(SIXEL_EXE): $(LIBIEC61850A) client.c sixel.c
	$(CC) -o $(SIXEL_EXE) -DUSE_SIXEL client.c sixel.c $(LIBFLAGS) -lncurses -pthread -L$(DST)/libiec61850-1.5.1/build -liec61850 -lsixel

$(LIBIEC61850A):
	unzip -qo libiec61850-1.5.1.zip -d $(DST)
	$(MAKE) -C $(DST)libiec61850-1.5.1

clean:
	rm -rf $(CONSOLE_EXE)
	rm -rf $(CURSES_EXE)
	rm -rf $(SIXEL_EXE)

distclean: clean
	rm -rf $(DST)

