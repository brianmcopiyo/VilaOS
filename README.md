# Terminal

Shared **React UI** (`web/`) and **shell kernel** (`src/`), with two build products:

| Target | Folder | Output |
|--------|--------|--------|
| Windows app | `winbuild/` | `winbuild/bin/shell.exe` + `ui/` |
| Bootable ISO | `kernelbuild/` | `kernelbuild/out/terminal.iso` |

## Layout

```
src/                 Shared C shell (parser, builtins, exec, util)
web/                 Shared React UI
winbuild/            Windows WebView2 host + vendored libs
  src/               main.c, native.cpp
  libs/              webview + WebView2
  bin/               shell.exe, ui/, WebView2Loader.dll
kernelbuild/         Freestanding kernel + ISO staging
  src/               boot.S, kmain.c
  iso_root/          GRUB config, kernel.bin, ui/
  out/terminal.iso
Makefile             Root: win / iso
```

## Windows app

```powershell
npm install --prefix web
mingw32-make -C winbuild
.\winbuild\bin\shell.exe
```

Or from the repo root: `mingw32-make win`

## ISO

```powershell
mingw32-make -C kernelbuild
```

This builds the freestanding kernel, stages React into `kernelbuild/iso_root/ui`, then packs `kernelbuild/out/terminal.iso`.

ISO packing prefers **WSL `grub-mkrescue`**, then **Windows ADK `oscdimg`**. Without either, a zip-backed staging file is written so the tree is still buildable.

A 32-bit freestanding ELF toolchain (`i686-elf-gcc` / `x86_64-elf-gcc`) produces a cleaner Multiboot2 kernel; otherwise the makefile uses host `gcc -m32 -ffreestanding`.

## Bridge (Windows host)

| JS | C |
|----|---|
| `nativePwd()` | working directory |
| `nativeCd(path)` | change directory |
| `nativeRun(cmd)` | run command / builtin |
| `nativeQuit()` | terminate app |
