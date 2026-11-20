# Bootable ISO. Shared UI from ../web; freestanding kernel in src/.
# Host MinGW cannot emit Multiboot ELF — tools/build-kernel.ps1 uses WSL when available.

ROOT      = ..
OUT_DIR   = out
ISO_ROOT  = iso_root
KERNEL    = $(ISO_ROOT)/boot/kernel.bin
ISO       = $(OUT_DIR)/terminal.iso

.PHONY: all stage ui kernel iso clean

all: iso

ui:
	cd $(ROOT) && cmd /C "set UI_TARGET=iso&& npm run build:ui"

$(OUT_DIR):
	if not exist $(OUT_DIR) mkdir $(OUT_DIR)
	if not exist $(ISO_ROOT)\boot mkdir $(ISO_ROOT)\boot

kernel: $(OUT_DIR)
	powershell -NoProfile -ExecutionPolicy Bypass -File tools\build-kernel.ps1

stage: ui kernel
	@echo Staged ISO root at $(ISO_ROOT)

iso: stage
	powershell -NoProfile -ExecutionPolicy Bypass -File tools\make-iso.ps1

clean:
	if exist $(OUT_DIR)\*.o del /Q $(OUT_DIR)\*.o
	if exist $(KERNEL) del /Q $(KERNEL)
	if exist $(ISO) del /Q $(ISO)
	if exist $(OUT_DIR)\terminal-iso-root.zip del /Q $(OUT_DIR)\terminal-iso-root.zip
	if exist $(ISO_ROOT)\ui rmdir /S /Q $(ISO_ROOT)\ui
