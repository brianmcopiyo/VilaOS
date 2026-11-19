APP      := vilaos
CC       ?= gcc
CFLAGS   ?= -std=c11 -O2 -Wall -Wextra -Wpedantic -ffunction-sections -fdata-sections -D_DEFAULT_SOURCE
LDFLAGS  ?= -Wl,--gc-sections
CPPFLAGS := -Iinclude
LIBS     :=

SRC := \
	src/main.c \
	src/core/util.c \
	src/gfx/fb.c \
	src/gfx/draw.c \
	src/gfx/font.c \
	src/xml/parse.c \
	src/ui/screen.c \
	src/input/input.c \
	src/input/t9.c

OBJ := $(SRC:.c=.o)

.PHONY: all clean native aarch64 musl desktop vfb install qemu-help iso vbox-help

all: native

native: $(APP)

$(APP): $(OBJ)
	$(CC) $(CFLAGS) -o $@ $(OBJ) $(LDFLAGS) $(LIBS)

%.o: %.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -c -o $@ $<

aarch64:
	$(MAKE) clean
	$(MAKE) CC=aarch64-linux-gnu-gcc APP=$(APP)-aarch64

musl:
	$(MAKE) clean
	$(MAKE) CC=musl-gcc CFLAGS="$(CFLAGS) -static" LDFLAGS="$(LDFLAGS) -static" APP=$(APP)-musl

desktop:
	$(MAKE) clean
	$(MAKE) CPPFLAGS="$(CPPFLAGS) -DPHONE_DESKTOP" \
		CFLAGS="$(CFLAGS)" \
		LIBS="-lSDL2" \
		APP=$(APP)-desktop

vfb:
	$(MAKE) clean
	$(MAKE) CPPFLAGS="$(CPPFLAGS) -DPHONE_VFB" \
		CFLAGS="$(CFLAGS)" \
		APP=$(APP)-vfb

install: $(APP)
	install -d $(DESTDIR)/usr/bin
	install -d $(DESTDIR)/usr/share/vilaos/assets/screens
	install -m 755 $(APP) $(DESTDIR)/usr/bin/vilaos
	cp -a assets/screens/. $(DESTDIR)/usr/share/vilaos/assets/screens/

qemu-help:
	@echo "Run: ./scripts/build_qemu_image.sh"
	@echo "Then: qemu-system-x86_64 -m 32M -kernel build/bzImage -initrd build/rootfs.cpio.gz -append \"console=ttyS0\" -nographic"

iso:
	bash scripts/build_vbox_iso.sh

vbox-help:
	@echo "WSL:   bash scripts/build_vbox_iso.sh"
	@echo "WinPS: .\\scripts\\run_vbox.ps1"

clean:
	rm -f $(OBJ) $(APP) $(APP)-aarch64 $(APP)-musl $(APP)-desktop $(APP)-vfb
	rm -f phoneos phoneos-aarch64 phoneos-musl phoneos-desktop phoneos-vfb
	find src -name '*.o' -delete 2>/dev/null || true
