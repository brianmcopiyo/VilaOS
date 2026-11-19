APP      := vilaos
PKG_APP  := vilaos-pkg
MUSICD   := vilaos-musicd
CC       ?= gcc
CFLAGS   ?= -std=c11 -O2 -Wall -Wextra -Wpedantic -ffunction-sections -fdata-sections -D_DEFAULT_SOURCE
LDFLAGS  ?= -Wl,--gc-sections
CPPFLAGS := -Iinclude
LIBS     :=
ifneq ($(OS),Windows_NT)
LIBS     := -lm -ldl -lpthread
endif

SRC := \
	src/main.c \
	src/core/util.c \
	src/core/syspower.c \
	src/core/contacts.c \
	src/gfx/fb.c \
	src/gfx/draw.c \
	src/gfx/font.c \
	src/gfx/ttfont.c \
	src/gfx/compose.c \
	src/gfx/pool.c \
	src/gfx/hwinfo.c \
	src/driver/gfx/loader.c \
	src/driver/gfx/builtin.c \
	src/driver/gfx/backend_drm.c \
	src/proc/proctier.c \
	src/proc/ipc.c \
	src/proc/procman.c \
	src/xml/parse.c \
	src/xml_ui/res.c \
	src/xml_ui/style.c \
	src/xml_ui/attrs.c \
	src/xml_ui/layout.c \
	src/xml_ui/parse.c \
	src/ui/screen.c \
	src/ui/touch.c \
	src/ui/route.c \
	src/input/input.c \
	src/input/t9.c \
	src/pkg/pkg.c \
	src/api/vapi.c \
	src/api/vapi_ui.c \
	src/api/vapi_storage.c \
	src/api/vapi_service.c \
	src/api/vapi_phone.c \
	src/api/vapi_system.c \
	src/sdk/vsl.c \
	src/sdk/bridge.c \
	src/sdk/apprun.c

PKG_SRC := \
	src/pkg/vilaos_pkg_main.c \
	src/pkg/pkg.c \
	src/xml/parse.c \
	src/core/util.c

MUSICD_SRC := \
	src/proc/musicd.c \
	src/proc/ipc.c \
	src/core/util.c

GFX_SO_SRC := \
	src/driver/gfx/shared_entry.c \
	src/driver/gfx/builtin.c \
	src/driver/gfx/backend_drm.c \
	src/gfx/fb.c \
	src/gfx/compose.c \
	src/gfx/pool.c \
	src/gfx/hwinfo.c \
	src/core/util.c

OBJ := $(SRC:.c=.o)
PKG_OBJ := $(PKG_SRC:.c=.o)
MUSICD_OBJ := $(MUSICD_SRC:.c=.o)

src/gfx/ttfont.o: CFLAGS := $(filter-out -Wpedantic,$(CFLAGS)) -Wno-unused-function -Wno-unused-parameter

.PHONY: all clean native aarch64 musl desktop vfb install qemu-help iso vbox-help pkg sdk-pack gfx-drive

all: native pkg $(MUSICD)

native: $(APP)

$(APP): $(OBJ)
	$(CC) $(CFLAGS) -o $@ $(OBJ) $(LDFLAGS) $(LIBS)

$(PKG_APP): $(PKG_OBJ)
	$(CC) $(CFLAGS) -o $@ $(PKG_OBJ) $(LDFLAGS)

$(MUSICD): $(MUSICD_OBJ)
	$(CC) $(CFLAGS) -o $@ $(MUSICD_OBJ) $(LDFLAGS) $(LIBS)

pkg: $(PKG_APP)

%.o: %.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -c -o $@ $<

gfx-driver:
	mkdir -p build
	$(CC) $(CPPFLAGS) $(CFLAGS) -fPIC -shared -DVILAOS_GFX_SO \
		-o build/libvilaos_gfx.so $(GFX_SO_SRC) $(LDFLAGS) -lm -ldl -lpthread
	bash scripts/pack_gfx_driver.sh

aarch64:
	$(MAKE) clean
	$(MAKE) CC=aarch64-linux-gnu-gcc APP=$(APP)-aarch64 PKG_APP=$(PKG_APP)-aarch64 MUSICD=$(MUSICD)-aarch64 all

musl:
	$(MAKE) clean
	$(MAKE) CC=musl-gcc \
		CFLAGS="$(CFLAGS)" \
		LDFLAGS="$(LDFLAGS)" \
		LIBS="-lm -ldl -lpthread" \
		APP=$(APP)-musl PKG_APP=$(PKG_APP)-musl MUSICD=$(MUSICD)-musl all
	$(MAKE) CC=musl-gcc \
		CFLAGS="$(CFLAGS)" \
		LDFLAGS="$(LDFLAGS)" \
		gfx-drive

desktop:
	$(MAKE) clean
	$(MAKE) CPPFLAGS="$(CPPFLAGS) -DPHONE_DESKTOP" \
		CFLAGS="$(CFLAGS)" \
		LIBS="-lSDL2 -lm -ldl -lpthread" \
		APP=$(APP)-desktop MUSICD=$(MUSICD)-desktop

vfb:
	$(MAKE) clean
	$(MAKE) CPPFLAGS="$(CPPFLAGS) -DPHONE_VFB" \
		CFLAGS="$(CFLAGS)" \
		LIBS="-lm -ldl -lpthread" \
		APP=$(APP)-vfb MUSICD=$(MUSICD)-vfb

sdk-pack:
	bash scripts/pack_apps.sh

install: $(APP) $(PKG_APP) $(MUSICD)
	install -d $(DESTDIR)/usr/bin
	install -d $(DESTDIR)/usr/share/vilaos/assets/screens
	install -d $(DESTDIR)/usr/share/vilaos/assets/values
	install -d $(DESTDIR)/usr/share/vilaos/assets/components
	install -d $(DESTDIR)/usr/share/vilaos/assets/styles
	install -d $(DESTDIR)/usr/share/vilaos/assets/ui
	install -d $(DESTDIR)/apps
	install -d $(DESTDIR)/drivers/gfx/builtin
	install -d $(DESTDIR)/run/vilaos
	install -m 755 $(APP) $(DESTDIR)/usr/bin/vilaos
	install -m 755 $(PKG_APP) $(DESTDIR)/usr/bin/vilaos-pkg
	install -m 755 $(MUSICD) $(DESTDIR)/usr/bin/vilaos-musicd
	cp -a assets/screens/. $(DESTDIR)/usr/share/vilaos/assets/screens/
	cp -a assets/values/. $(DESTDIR)/usr/share/vilaos/assets/values/ 2>/dev/null || true
	cp -a assets/components/. $(DESTDIR)/usr/share/vilaos/assets/components/ 2>/dev/null || true
	cp -a assets/styles/. $(DESTDIR)/usr/share/vilaos/assets/styles/ 2>/dev/null || true
	cp -a assets/ui/. $(DESTDIR)/usr/share/vilaos/assets/ui/ 2>/dev/null || true
	cp -a apps/. $(DESTDIR)/apps/
	if [ -f build/libvilaos_gfx.so ]; then install -m 755 build/libvilaos_gfx.so $(DESTDIR)/drivers/gfx/builtin/; fi
	if [ -d build/packages/vilaos-gfx-staging/builtin ]; then cp -a build/packages/vilaos-gfx-staging/. $(DESTDIR)/drivers/gfx/; fi

qemu-help:
	@echo "Run: ./scripts/build_qemu_image.sh"
	@echo "Then: qemu-system-x86_64 -m 256M -kernel build/bzImage -initrd build/rootfs.cpio.gz -append \"console=ttyS0\" -nographic"

iso:
	bash scripts/build_vbox_iso.sh

vbox-help:
	@echo "WSL:   bash scripts/build_vbox_iso.sh"
	@echo "WinPS: .\\scripts\\run_vbox.ps1"

clean:
	rm -f $(OBJ) $(PKG_OBJ) $(MUSICD_OBJ) $(APP) $(APP)-aarch64 $(APP)-musl $(APP)-desktop $(APP)-vfb
	rm -f $(PKG_APP) $(PKG_APP)-aarch64 $(PKG_APP)-musl
	rm -f $(MUSICD) $(MUSICD)-aarch64 $(MUSICD)-musl $(MUSICD)-desktop $(MUSICD)-vfb
	rm -f phoneos phoneos-aarch64 phoneos-musl phoneos-desktop phoneos-vfb
	rm -f build/libvilaos_gfx.so libvilaos_gfx.so
	find src -name '*.o' -delete 2>/dev/null || true
