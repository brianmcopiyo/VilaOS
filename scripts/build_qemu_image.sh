#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BUILD="${ROOT}/build"
ROOTFS="${BUILD}/rootfs"
JOBS="$(nproc 2>/dev/null || echo 2)"

BUSYBOX_VER="${BUSYBOX_VER:-1.36.1}"
KERNEL_VER="${KERNEL_VER:-6.6.22}"
BUSYBOX_URL="https://busybox.net/downloads/busybox-${BUSYBOX_VER}.tar.bz2"
KERNEL_URL="https://cdn.kernel.org/pub/linux/kernel/v6.x/linux-${KERNEL_VER}.tar.xz"

mkdir -p "${BUILD}" "${ROOTFS}"

echo "==> Building vilaos + vilaos-pkg (dynamic musl if available)"
cd "${ROOT}"
if command -v musl-gcc >/dev/null 2>&1; then
  make clean
  make musl
  UI_BIN="${ROOT}/vilaos-musl"
  PKG_BIN="${ROOT}/vilaos-pkg-musl"
else
  make clean
  make CFLAGS="-std=c11 -O2 -Wall -Wextra" LDFLAGS="" LIBS="-lm -ldl -lpthread" APP=vilaos-static PKG_APP=vilaos-pkg-static all
  UI_BIN="${ROOT}/vilaos-static"
  PKG_BIN="${ROOT}/vilaos-pkg-static"
fi
test -x "${UI_BIN}"
test -x "${PKG_BIN}"
strip --strip-unneeded "${UI_BIN}" 2>/dev/null || true
strip --strip-unneeded "${PKG_BIN}" 2>/dev/null || true
bash "${ROOT}/scripts/pack_apps.sh" || true
bash "${ROOT}/scripts/pack_gfx_driver.sh" || true

install_musl_runtime() {
  local dest="$1"
  mkdir -p "${dest}/lib"
  local musl_ld=""
  for cand in /lib/ld-musl-x86_64.so.1 /usr/lib/x86_64-linux-musl/libc.so /lib/x86_64-linux-musl/libc.so; do
    if [[ -f "${cand}" && -x "${cand}" && "$(stat -c%s "${cand}" 2>/dev/null || echo 0)" -gt 4096 ]]; then
      musl_ld="${cand}"
      break
    fi
  done
  if [[ -z "${musl_ld}" ]]; then
    echo "missing musl dynamic linker" >&2
    exit 1
  fi
  cp -f "${musl_ld}" "${dest}/lib/ld-musl-x86_64.so.1"
  chmod 755 "${dest}/lib/ld-musl-x86_64.so.1"
  ln -sfn ld-musl-x86_64.so.1 "${dest}/lib/libc.so"
}

install_gfx_drivers() {
  local dest="$1"
  mkdir -p "${dest}/drivers/gfx/builtin"
  echo "builtin" > "${dest}/drivers/gfx/active"
  cat > "${dest}/drivers/gfx/builtin/manifest.xml" << EOF
<?xml version="1.0" encoding="UTF-8"?>
<driver>
  <id>builtin</id>
  <version>1.0.0</version>
  <abi>1</abi>
  <entry>libvilaos_gfx.so</entry>
  <name>vilaos-gfx-builtin</name>
</driver>
EOF
  if [[ -f "${ROOT}/build/libvilaos_gfx.so" ]]; then
    cp -f "${ROOT}/build/libvilaos_gfx.so" "${dest}/drivers/gfx/builtin/libvilaos_gfx.so"
    cp -f "${ROOT}/build/libvilaos_gfx.so" "${dest}/lib/libvilaos_gfx.so" 2>/dev/null || true
  fi
  if [[ -f "${ROOT}/build/packages/vilaos-gfx.vdrv" ]]; then
    mkdir -p "${dest}/packages"
    cp -f "${ROOT}/build/packages/vilaos-gfx.vdrv" "${dest}/packages/"
  fi
}

fetch() {
  local url="$1" out="$2"
  if [[ -f "${out}" ]]; then
    return 0
  fi
  echo "==> Downloading ${url}"
  curl -L --fail -o "${out}" "${url}"
}

echo "==> BusyBox ${BUSYBOX_VER}"
fetch "${BUSYBOX_URL}" "${BUILD}/busybox-${BUSYBOX_VER}.tar.bz2"
if [[ ! -d "${BUILD}/busybox-${BUSYBOX_VER}" ]]; then
  tar -xjf "${BUILD}/busybox-${BUSYBOX_VER}.tar.bz2" -C "${BUILD}"
fi
cd "${BUILD}/busybox-${BUSYBOX_VER}"
make distclean >/dev/null 2>&1 || true
make defconfig
sed -i 's/# CONFIG_STATIC is not set/CONFIG_STATIC=y/' .config
sed -i 's/^CONFIG_EXTRA_CFLAGS=".*"/CONFIG_EXTRA_CFLAGS="-Os"/' .config || true
yes "" 2>/dev/null | make oldconfig >/dev/null 2>&1 || true
for opt in \
  CONFIG_TC CONFIG_FEATURE_TC_INGRESS CONFIG_FEATURE_MOUNT_NFS \
  CONFIG_IFCONFIG CONFIG_ROUTE CONFIG_IP CONFIG_IPADDR CONFIG_IPLINK \
  CONFIG_IPROUTE CONFIG_IPTUNNEL CONFIG_IPRULE CONFIG_IPNEIGH \
  CONFIG_NETSTAT CONFIG_WGET CONFIG_BRCTL CONFIG_UDHCPC CONFIG_UDHCPD \
  CONFIG_PING CONFIG_PING6 CONFIG_TRACEROUTE CONFIG_TRACEROUTE6 \
  CONFIG_NSLOOKUP CONFIG_HOSTNAME CONFIG_ARP CONFIG_ARPING
do
  sed -i "/^${opt}=y/d" .config
  sed -i "/^# ${opt} is not set/d" .config
  echo "# ${opt} is not set" >> .config
done
grep -q '^CONFIG_STATIC=y' .config || echo 'CONFIG_STATIC=y' >> .config
make -j"${JOBS}" busybox
strip --strip-unneeded busybox 2>/dev/null || true
install -m 755 busybox "${BUILD}/busybox"

configure_kernel_tiny() {
  local cfg=./scripts/config

  $cfg --enable CONFIG_EXPERT || true
  $cfg --enable CONFIG_64BIT
  $cfg --enable CONFIG_X86_64
  $cfg --enable CONFIG_SMP
  $cfg --set-val CONFIG_NR_CPUS 2
  $cfg --enable CONFIG_PCI
  $cfg --enable CONFIG_PCI_MSI || true
  $cfg --enable CONFIG_PRINTK
  $cfg --enable CONFIG_BUG
  $cfg --enable CONFIG_ELF_CORE || true
  $cfg --enable CONFIG_BINFMT_ELF
  $cfg --enable CONFIG_BINFMT_SCRIPT || true
  $cfg --enable CONFIG_BLK_DEV_INITRD
  $cfg --enable CONFIG_RD_GZIP
  $cfg --enable CONFIG_DEVTMPFS
  $cfg --enable CONFIG_DEVTMPFS_MOUNT
  $cfg --enable CONFIG_TMPFS
  $cfg --enable CONFIG_PROC_FS
  $cfg --enable CONFIG_SYSFS
  $cfg --enable CONFIG_UNIX98_PTYS
  $cfg --enable CONFIG_EPOLL
  $cfg --enable CONFIG_EVENTFD || true
  $cfg --enable CONFIG_SIGNALFD || true
  $cfg --enable CONFIG_INOTIFY_USER
  $cfg --enable CONFIG_VT
  $cfg --enable CONFIG_VT_CONSOLE
  $cfg --enable CONFIG_VGA_CONSOLE || true
  $cfg --enable CONFIG_DUMMY_CONSOLE || true
  $cfg --enable CONFIG_FB
  $cfg --enable CONFIG_FB_VESA
  $cfg --enable CONFIG_FRAMEBUFFER_CONSOLE
  $cfg --enable CONFIG_FRAMEBUFFER_CONSOLE_DETECT_PRIMARY || true
  $cfg --enable CONFIG_FONT_8x8 || true
  $cfg --enable CONFIG_FONT_8x16 || true
  $cfg --enable CONFIG_INPUT
  $cfg --enable CONFIG_INPUT_EVDEV
  $cfg --enable CONFIG_INPUT_KEYBOARD
  $cfg --enable CONFIG_INPUT_MOUSE
  $cfg --enable CONFIG_SERIO
  $cfg --enable CONFIG_SERIO_I8042
  $cfg --enable CONFIG_SERIO_LIBPS2
  $cfg --enable CONFIG_KEYBOARD_ATKBD
  $cfg --enable CONFIG_MOUSE_PS2
  $cfg --enable CONFIG_USB
  $cfg --enable CONFIG_USB_SUPPORT
  $cfg --enable CONFIG_USB_XHCI_HCD || true
  $cfg --enable CONFIG_USB_EHCI_HCD || true
  $cfg --enable CONFIG_USB_OHCI_HCD || true
  $cfg --enable CONFIG_USB_UHCI_HCD || true
  $cfg --enable CONFIG_USB_HID
  $cfg --enable CONFIG_HID
  $cfg --enable CONFIG_HID_GENERIC
  $cfg --enable CONFIG_HID_SUPPORT || true
  $cfg --enable CONFIG_TTY
  $cfg --enable CONFIG_SERIAL_8250 || true
  $cfg --enable CONFIG_SERIAL_8250_CONSOLE || true
  $cfg --enable CONFIG_EARLY_PRINTK || true
  $cfg --enable CONFIG_IKCONFIG || true
  $cfg --enable CONFIG_IKCONFIG_PROC || true
  $cfg --enable CONFIG_CC_OPTIMIZE_FOR_SIZE
  $cfg --disable CONFIG_CC_OPTIMIZE_FOR_PERFORMANCE || true

  $cfg --disable CONFIG_DEBUG_INFO || true
  $cfg --disable CONFIG_DEBUG_INFO_DWARF_TOOLCHAIN_DEFAULT || true
  $cfg --disable CONFIG_DEBUG_INFO_DWARF4 || true
  $cfg --disable CONFIG_DEBUG_INFO_DWARF5 || true
  $cfg --disable CONFIG_DEBUG_INFO_BTF || true
  $cfg --disable CONFIG_DEBUG_KERNEL || true
  $cfg --disable CONFIG_DEBUG_FS || true
  $cfg --disable CONFIG_KGDB || true
  $cfg --disable CONFIG_FTRACE || true
  $cfg --disable CONFIG_KALLSYMS || true
  $cfg --disable CONFIG_KALLSYMS_ALL || true
  $cfg --disable CONFIG_MODULES || true
  $cfg --disable CONFIG_DRM || true
  $cfg --disable CONFIG_DRM_I915 || true
  $cfg --disable CONFIG_DRM_FBDEV_EMULATION || true
  $cfg --disable CONFIG_SOUND || true
  $cfg --disable CONFIG_SND || true
  $cfg --disable CONFIG_NET || true
  $cfg --disable CONFIG_NETDEVICES || true
  $cfg --disable CONFIG_WIRELESS || true
  $cfg --disable CONFIG_WLAN || true
  $cfg --disable CONFIG_BT || true
  $cfg --disable CONFIG_IPV6 || true
  $cfg --disable CONFIG_INET || true
  $cfg --disable CONFIG_PACKET || true
  $cfg --disable CONFIG_UNIX || true
  $cfg --disable CONFIG_SECURITY || true
  $cfg --disable CONFIG_SECURITY_SELINUX || true
  $cfg --disable CONFIG_IMA || true
  $cfg --disable CONFIG_EVM || true
  $cfg --disable CONFIG_AUDIT || true
  $cfg --disable CONFIG_CGROUPS || true
  $cfg --disable CONFIG_NAMESPACES || true
  $cfg --disable CONFIG_CHECKPOINT_RESTORE || true
  $cfg --disable CONFIG_BPF_SYSCALL || true
  $cfg --disable CONFIG_PERF_EVENTS || true
  $cfg --disable CONFIG_HW_RANDOM || true
  $cfg --disable CONFIG_NVME_CORE || true
  $cfg --disable CONFIG_BLK_DEV_NVME || true
  $cfg --disable CONFIG_ATA || true
  $cfg --disable CONFIG_SATA_AHCI || true
  $cfg --disable CONFIG_SCSI || true
  $cfg --disable CONFIG_MD || true
  $cfg --disable CONFIG_MDIO || true
  $cfg --disable CONFIG_MMC || true
  $cfg --disable CONFIG_VIRTUALIZATION || true
  $cfg --disable CONFIG_HYPERVISOR_GUEST || true
  $cfg --disable CONFIG_KVM_GUEST || true
  $cfg --disable CONFIG_IA32_EMULATION || true
  $cfg --disable CONFIG_X86_X32_ABI || true
  $cfg --disable CONFIG_LOGO || true
  $cfg --disable CONFIG_FB_EFI || true
  $cfg --disable CONFIG_FB_SIMPLE || true
  $cfg --disable CONFIG_EFI || true
  $cfg --disable CONFIG_EFI_STUB || true
  $cfg --disable CONFIG_WLAN || true
  $cfg --disable CONFIG_RFKILL || true
  $cfg --disable CONFIG_INPUT_JOYSTICK || true
  $cfg --disable CONFIG_INPUT_TABLET || true
  $cfg --disable CONFIG_INPUT_TOUCHSCREEN || true
  $cfg --disable CONFIG_INPUT_MISC || true
  $cfg --disable CONFIG_MEDIA_SUPPORT || true
  $cfg --disable CONFIG_VIDEO_DEV || true
  $cfg --disable CONFIG_SOUNDWIRE || true
  $cfg --enable CONFIG_PM || true
  $cfg --enable CONFIG_PM_SLEEP || true
  $cfg --enable CONFIG_ACPI || true
  $cfg --enable CONFIG_ACPI_BUTTON || true
  $cfg --enable CONFIG_ACPI_FAN || true
  $cfg --enable CONFIG_ACPI_PROCESSOR || true
  $cfg --enable CONFIG_ACPI_THERMAL || true
  $cfg --enable CONFIG_X86_ACPI_CPUFREQ || true
  $cfg --disable CONFIG_ACPI_AC || true
  $cfg --disable CONFIG_ACPI_BATTERY || true
  $cfg --disable CONFIG_ACPI_DOCK || true
}

echo "==> Linux kernel ${KERNEL_VER} (tiny)"
fetch "${KERNEL_URL}" "${BUILD}/linux-${KERNEL_VER}.tar.xz"
if [[ ! -d "${BUILD}/linux-${KERNEL_VER}" ]]; then
  tar -xf "${BUILD}/linux-${KERNEL_VER}.tar.xz" -C "${BUILD}"
fi
cd "${BUILD}/linux-${KERNEL_VER}"
make distclean >/dev/null 2>&1 || true
make tinyconfig
configure_kernel_tiny
make olddefconfig
make -j"${JOBS}" bzImage
cp -f arch/x86/boot/bzImage "${BUILD}/bzImage"
ls -lh "${BUILD}/bzImage"

echo "==> Assembling rootfs"
rm -rf "${ROOTFS}"
mkdir -p "${ROOTFS}"/{bin,sbin,dev,proc,sys,tmp,lib,assets/screens,apps,data/apps,var/lib/vilaos,packages,drivers/gfx}
cp -f "${BUILD}/busybox" "${ROOTFS}/bin/busybox"
for a in sh ls cat mount umount mkdir mknod chmod chown sleep ps free dmesg tar; do
  ln -sf busybox "${ROOTFS}/bin/${a}"
done
cp -f "${UI_BIN}" "${ROOTFS}/bin/vilaos"
cp -f "${PKG_BIN}" "${ROOTFS}/bin/vilaos-pkg"
chmod 755 "${ROOTFS}/bin/vilaos" "${ROOTFS}/bin/vilaos-pkg"
install_musl_runtime "${ROOTFS}"
install_gfx_drivers "${ROOTFS}"
cp -a "${ROOT}/assets/screens/." "${ROOTFS}/assets/screens/"
cp -a "${ROOT}/apps/." "${ROOTFS}/apps/"
if [[ -d "${ROOT}/build/packages" ]]; then
  cp -a "${ROOT}/build/packages/." "${ROOTFS}/packages/" || true
fi

cat > "${ROOTFS}/init" << 'EOF'
#!/bin/sh
mount -t proc proc /proc
mount -t sysfs sysfs /sys
mount -t devtmpfs devtmpfs /dev 2>/dev/null || {
  mknod /dev/fb0 c 29 0 2>/dev/null
  mknod /dev/tty0 c 4 0 2>/dev/null
  mknod /dev/ttyS0 c 4 64 2>/dev/null
  mkdir -p /dev/input
}
mkdir -p /tmp /dev/input /apps /data/apps /var/lib/vilaos /packages /drivers/gfx /lib
[ -e /dev/fb0 ] || mknod /dev/fb0 c 29 0 2>/dev/null
[ -e /dev/tty0 ] || mknod /dev/tty0 c 4 0 2>/dev/null
export LD_LIBRARY_PATH=/lib:/drivers/gfx/builtin
cd /
export HOME=/
while true; do
  /bin/vilaos
  sleep 1
done
EOF
chmod 755 "${ROOTFS}/init"
ln -sf /init "${ROOTFS}/sbin/init"

echo "==> Creating initramfs"
cd "${ROOTFS}"
find . | cpio -o -H newc 2>/dev/null | gzip -9 > "${BUILD}/rootfs.cpio.gz"
ls -lh "${BUILD}/rootfs.cpio.gz"

cat > "${BUILD}/run_qemu.sh" << EOF
#!/usr/bin/env bash
exec qemu-system-x86_64 \\
  -m 256M \\
  -kernel "${BUILD}/bzImage" \\
  -initrd "${BUILD}/rootfs.cpio.gz" \\
  -append "console=ttyS0 quiet" \\
  -nographic \\
  "\$@"
EOF
chmod 755 "${BUILD}/run_qemu.sh"

cat > "${BUILD}/run_qemu_fb.sh" << EOF
#!/usr/bin/env bash
exec qemu-system-x86_64 \\
  -m 256M \\
  -kernel "${BUILD}/bzImage" \\
  -initrd "${BUILD}/rootfs.cpio.gz" \\
  -append "console=tty0 vga=0x311" \\
  -vga std \\
  "\$@"
EOF
chmod 755 "${BUILD}/run_qemu_fb.sh"

echo
echo "Build complete."
echo "  Kernel : ${BUILD}/bzImage ($(stat -c%s "${BUILD}/bzImage") bytes)"
echo "  Rootfs : ${BUILD}/rootfs.cpio.gz ($(stat -c%s "${BUILD}/rootfs.cpio.gz") bytes)"
echo "  Serial : ${BUILD}/run_qemu.sh"
echo "  VGA FB : ${BUILD}/run_qemu_fb.sh"
