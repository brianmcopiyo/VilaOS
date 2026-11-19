# VilaOS

Ultra-lightweight Linux feature-phone UI shell for RGB565/XRGB framebuffer targets, designed for a **32 MB** system RAM minimum and **&lt; 4 MB** process RSS.

Pure **C11**, libc only (`musl` or `glibc`). No FreeType, no DOM heap, no runtime deps beyond the C library (optional SDL2 for desktop preview). Display size follows the active framebuffer resolution.

## VirtualBox on Windows

**Step 1 — build the bootable ISO in WSL/Linux:**

```bash
bash scripts/build_vbox_iso.sh
```

**Step 2 — create/start the VM in Windows PowerShell:**

```powershell
.\scripts\run_vbox.ps1
.\scripts\run_vbox.ps1 -MemoryMB 32
```

This produces `build/vilaos.iso`, then idempotently creates VM `VilaOS` (default `--memory 128`, override with `-MemoryMB`), `--vram 8`, `--graphicscontroller vboxvga`, PS/2 keyboard/mouse, and boots the ISO.

WSL packages for ISO creation:

```bash
sudo apt-get install -y build-essential musl-tools curl bison flex libelf-dev libssl-dev bc \
  grub-pc-bin grub-common xorriso genisoimage syslinux isolinux
```

## Layout

```
src/core/     utilities
src/gfx/      framebuffer, software renderer, bitmap fonts
src/xml/      zero-allocation streaming XML parser
src/ui/       widget pool + screen loader/renderer
src/input/    epoll input mux, T9, QWERTY, mouse cursor
assets/screens/  declarative XML UIs
scripts/      QEMU / VirtualBox packaging
```

## Build (Linux / WSL)

```bash
make
make aarch64
make musl
make desktop
./vilaos-desktop --desktop
make vfb
./vilaos-vfb
```

### Run on a real framebuffer TTY

```bash
make
sudo chvt 2
sudo ./vilaos
```

Keys: **arrows** focus, **Enter** OK, **Esc** back, **F1** left softkey, **F2** right softkey, **0–9** T9 (or dial digits on Dialer), **A–Z** QWERTY, mouse moves/clicks the pointer.

## QEMU image

```bash
bash scripts/build_qemu_image.sh
qemu-system-x86_64 -m 32M \
  -kernel build/bzImage \
  -initrd build/rootfs.cpio.gz \
  -append "console=tty0 vga=0x311" \
  -vga std
```

## Screens

| File | Purpose |
|------|---------|
| `home.xml` | Status bar, launcher list, softkeys |
| `dialer.xml` | Numeric entry + Call/Clear |
| `messages.xml` / `compose.xml` | SMS list + T9/QWERTY/mouse compose |
| `sysinfo.xml` | Live `/proc/meminfo`, uptime, self RSS, FB mode |
| `contacts.xml` / `notes.xml` | Extra MVP apps |

## Memory notes

- RGB565 backbuffer sized to the active framebuffer resolution
- Widget pool: fixed `MAX_WIDGETS` (96), no per-parse heap DOM
- Dirty-rectangle blit to `/dev/fb0` filling the full display
