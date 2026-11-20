# Root orchestrator. Shared kernel: src/  Shared UI: web/
# Windows host: winbuild/   Bootable ISO: kernelbuild/

.PHONY: all win iso ui-win ui-iso clean clean-win clean-iso

all: win

win:
	$(MAKE) -C winbuild

iso:
	$(MAKE) -C kernelbuild

ui-win:
	cmd /C "set UI_TARGET=win&& npm run build:ui"

ui-iso:
	cmd /C "set UI_TARGET=iso&& npm run build:ui"

clean: clean-win clean-iso

clean-win:
	$(MAKE) -C winbuild clean

clean-iso:
	$(MAKE) -C kernelbuild clean
