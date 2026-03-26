PREFIX ?= /usr/local
BINDIR ?= $(PREFIX)/bin
SHAREDIR ?= $(PREFIX)/share/minipro-t76
UDEVDIR ?= /etc/udev/rules.d

CC ?= gcc
CFLAGS ?= -Wall -Wextra -O2 -g
CFLAGS += -Iinclude
LDFLAGS +=
LIBS = -lusb-1.0

SRCS = src/main.c src/usb.c src/protocol.c src/chipdb.c src/fileio.c src/adapter.c src/algorithm.c src/pintest.c
OBJS = $(SRCS:.c=.o)
TARGET = minipro-t76

TOOL_SRCS = tools/extract_chipdb.c
TOOL_TARGET = tools/extract_chipdb

.PHONY: all clean install uninstall udev tools

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $^ $(LIBS)

%.o: %.c include/t76.h
	$(CC) $(CFLAGS) -c -o $@ $<

tools: $(TOOL_TARGET)

$(TOOL_TARGET): $(TOOL_SRCS)
	@mkdir -p tools
	$(CC) $(CFLAGS) -o $@ $<

clean:
	rm -f $(OBJS) $(TARGET) $(TOOL_TARGET)

install: $(TARGET)
	install -d $(DESTDIR)$(BINDIR)
	install -m 755 $(TARGET) $(DESTDIR)$(BINDIR)/
	install -d $(DESTDIR)$(SHAREDIR)
	install -m 644 chipdb.txt $(DESTDIR)$(SHAREDIR)/ 2>/dev/null || true

uninstall:
	rm -f $(DESTDIR)$(BINDIR)/$(TARGET)
	rm -rf $(DESTDIR)$(SHAREDIR)
	rm -f $(DESTDIR)$(UDEVDIR)/60-minipro-t76.rules

udev:
	install -m 644 udev/60-minipro-t76.rules $(DESTDIR)$(UDEVDIR)/
	udevadm control --reload-rules 2>/dev/null || true
	udevadm trigger 2>/dev/null || true
	@echo "Udev rules installed. You may need to re-plug the device."
