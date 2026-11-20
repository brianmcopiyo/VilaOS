#!/usr/bin/env bash
set -euo pipefail

HOST="/mnt/c/Users/HP/Desktop/OS"
VILA_BUILD="/root/vilaos-work/src/build"
OLD_BUILD="/root/phoneos-work/src/build"
KVER="6.6.22"
KDIR=""
for cand in "${VILA_BUILD}/linux-${KVER}" "${OLD_BUILD}/linux-${KVER}"; do
  if [[ -d "${cand}" ]]; then
    KDIR="${cand}"
    break
  fi
done
if [[ -z "${KDIR}" ]]; then
  echo "missing kernel tree linux-${KVER}" >&2
  exit 1
fi

cd "${KDIR}"
cfg=./scripts/config
make olddefconfig >/dev/null 2>&1 || true

$cfg --enable CONFIG_BLOCK || true
$cfg --enable CONFIG_BLK_DEV || true
$cfg --enable CONFIG_ATA || true
$cfg --enable CONFIG_ATA_PIIX || true
$cfg --enable CONFIG_ATA_GENERIC || true
$cfg --enable CONFIG_SATA_AHCI || true
$cfg --enable CONFIG_SCSI || true
$cfg --enable CONFIG_BLK_DEV_SR || true
$cfg --enable CONFIG_CHR_DEV_SG || true
$cfg --enable CONFIG_ISO9660_FS || true
$cfg --enable CONFIG_JOLIET || true
$cfg --enable CONFIG_ZISOFS || true
$cfg --enable CONFIG_NLS || true
$cfg --enable CONFIG_NLS_CODEPAGE_437 || true
$cfg --enable CONFIG_NLS_ISO8859_1 || true
$cfg --enable CONFIG_NLS_UTF8 || true
$cfg --enable CONFIG_DRM || true
$cfg --enable CONFIG_DRM_KMS_HELPER || true
$cfg --enable CONFIG_DRM_VMWGFX || true
$cfg --enable CONFIG_DRM_FBDEV_EMULATION || true
$cfg --enable CONFIG_FB || true
$cfg --enable CONFIG_FB_VESA || true
$cfg --enable CONFIG_FB_MODE_HELPERS || true
$cfg --enable CONFIG_HYPERVISOR_GUEST || true
$cfg --enable CONFIG_PCI || true
$cfg --enable CONFIG_ACPI || true
$cfg --enable CONFIG_PM || true
$cfg --enable CONFIG_UNIX || true
$cfg --enable CONFIG_NET || true
$cfg --enable CONFIG_INET || true
$cfg --enable CONFIG_IPV6 || true
$cfg --enable CONFIG_PACKET || true
$cfg --enable CONFIG_NETDEVICES || true
$cfg --enable CONFIG_NET_CORE || true
$cfg --enable CONFIG_INET_DIAG || true
$cfg --enable CONFIG_TCP_CONG_CUBIC || true
$cfg --enable CONFIG_DEFAULT_TCP_CONG || true
$cfg --enable CONFIG_IP_MULTICAST || true
$cfg --enable CONFIG_SYN_COOKIES || true
$cfg --enable CONFIG_FUTEX || true
$cfg --enable CONFIG_FUTEX_PI || true
$cfg --enable CONFIG_EPOLL || true
$cfg --enable CONFIG_EVENTFD || true
$cfg --enable CONFIG_SIGNALFD || true
$cfg --enable CONFIG_TIMERFD || true
$cfg --enable CONFIG_TMPFS || true
$cfg --enable CONFIG_TMPFS_POSIX_ACL || true
$cfg --enable CONFIG_SHMEM || true
$cfg --enable CONFIG_SYSVIPC || true
$cfg --enable CONFIG_POSIX_MQUEUE || true
$cfg --enable CONFIG_ADVISE_SYSCALLS || true
$cfg --enable CONFIG_MEMBARRIER || true
$cfg --enable CONFIG_CHECKPOINT_RESTORE || true
$cfg --enable CONFIG_PROC_FS || true
$cfg --enable CONFIG_MULTIUSER || true
$cfg --enable CONFIG_FILE_LOCKING || true
$cfg --enable CONFIG_DEVTMPFS || true
$cfg --enable CONFIG_DEVTMPFS_MOUNT || true

make olddefconfig
make -j"$(nproc)" bzImage
mkdir -p "${VILA_BUILD}" "${HOST}/build" "${OLD_BUILD}"
cp -f arch/x86/boot/bzImage "${VILA_BUILD}/bzImage"
cp -f arch/x86/boot/bzImage "${OLD_BUILD}/bzImage" 2>/dev/null || true
cp -f arch/x86/boot/bzImage "${HOST}/build/bzImage"
ls -lh "${HOST}/build/bzImage"
grep -E '^CONFIG_(ATA|SCSI|BLK_DEV_SR|ISO9660|JOLIET|DRM|DRM_VMWGFX|HYPERVISOR|NET|INET|NETDEVICES|UNIX)=' .config | head -50
echo "kernel ready for Chromium ISO mount + VMSVGA DRM + IPv4 loopback"
