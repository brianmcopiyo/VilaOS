# Linux live ISO: Debian + Chromium kiosk loading the shared React UI.
ROOT = ..
OUT  = out/terminal.iso

.PHONY: all ui iso clean

all: iso

ui:
	cd $(ROOT) && cmd /C "set UI_TARGET=iso&& npm run build:ui"

iso: ui
	wsl -e bash -lc 'cd "$$(wslpath -a "$(CURDIR)")" && bash tools/build-live-iso.sh'

clean:
	if exist out\terminal.iso del /Q out\terminal.iso
	wsl -e bash -lc 'sudo rm -rf "$$HOME/.cache/terminal-os-live" "$$(wslpath -a "$(CURDIR)")/iso_root/ui" 2>/dev/null; true'