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
kernelbuild/         Debian live ISO + Chromium kiosk
  tools/             build-live-iso.sh (runs in WSL)
  overlay/           Xorg / live tweaks
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

## ISO (Linux live + Chromium kiosk)

Requires **WSL** (Ubuntu) with sudo. First build downloads a Debian rootfs and Chromium (slow); later builds reuse `.cache/`.

```powershell
mingw32-make -C kernelbuild
```

Boot flow: GRUB → Debian live → autologin → Openbox → Chromium `--kiosk` → React UI at `http://127.0.0.1/` (nginx serves `/opt/terminal/ui`).

Output: `kernelbuild/out/terminal.iso`

## Bridge (Windows host)

| JS | C |
|----|---|
| `nativePwd()` | working directory |
| `nativeCd(path)` | change directory |
| `nativeRun(cmd)` | run command / builtin |
| `nativeQuit()` | terminate app |

On the ISO kiosk, those bridges are absent; the React shell still runs (browser fallbacks in `native.ts`).
