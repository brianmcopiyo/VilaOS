APP      := vilaos
PKG_APP  := vilaos-pkg
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
	src/ui/route.c \
	src/input/input.c \
	src/input/t9.c \
	src/pkg/pkg.c \
	src/sdk/vsl.c \
	src/sdk/bridge.c \
	src/sdk/apprun.c

PKG_SRC := \
	src/pkg/vilaos_pkg_main.c \
	src/pkg/pkg.c \
	src/xml/parse.c \
	src/core/util.c

OBJ := $(SRC:.c=.o)
PKG_OBJ := $(PKG_SRC:.c=.o)

.PHONY: all clean native aarch64 musl desktop vfb install qemu-help iso vbox-help pkg sdk-pack

all: native pkg

native: $(APP)

$(APP): $(OBJ)
	$(CC) $(CFLAGS) -o $@ $(OBJ) $(LDFLAGS) $(LIBS)

$(PKG_APP): $(PKG_OBJ)
	$(CC) $(CFLAGS) -o $@ $(PKG_OBJ) $(LDFLAGS)

pkg: $(PKG_APP)

%.o: %.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -c -o $@ $<

aarch64:
	$(MAKE) clean
	$(MAKE) CC=aarch64-linux-gnu-gcc APP=$(APP)-aarch64 PKG_APP=$(PKG_APP)-aarch64 all

musl:
	$(MAKE) clean
	$(MAKE) CC=musl-gcc CFLAGS="$(CFLAGS) -static" LDFLAGS="$(LDFLAGS) -static" APP=$(APP)-musl PKG_APP=$(PKG_APP)-musl all

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

sdk-pack:
	bash scripts/pack_apps.sh

install: $(APP) $(PKG_APP)
	install -d $(DESTDIR)/usr/bin
	install -d $(DESTDIR)/usr/share/vilaos/assets/screens
	install -d $(DESTDIR)/apps
	install -m 755 $(APP) $(DESTDIR)/usr/bin/vilaos
	install -m 755 $(PKG_APP) $(DESTDIR)/usr/bin/vilaos-pkg
	cp -a assets/screens/. $(DESTDIR)/usr/share/vilaos/assets/screens/
	cp -a apps/. $(DESTDIR)/apps/

qemu-help:
	@echo "Run: ./scripts/build_qemu_image.sh"
	@echo "Then: qemu-system-x86_64 -m 32M -kernel build/bzImage -initrd build/rootfs.cpio.gz -append \"console=ttyS0\" -nographic"

iso:
	bash scripts/build_vbox_iso.sh

vbox-help:
	@echo "WSL:   bash scripts/build_vbox_iso.sh"
	@echo "WinPS: .\\scripts\\run_vbox.ps1"

clean:
	rm -f $(OBJ) $(PKG_OBJ) $(APP) $(APP)-aarch64 $(APP)-musl $(APP)-desktop $(APP)-vfb
	rm -f $(PKG_APP) $(PKG_APP)-aarch64 $(PKG_APP)-musl
	rm -f phoneos phoneos-aarch64 phoneos-musl phoneos-desktop phoneos-vfb
	find src -name '*.o' -delete 2>/dev/null || true
