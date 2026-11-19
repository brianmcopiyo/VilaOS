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

$cfg --enable CONFIG_PM || true
$cfg --enable CONFIG_PM_SLEEP || true
$cfg --enable CONFIG_ACPI || true
$cfg --enable CONFIG_ACPI_BUTTON || true
$cfg --enable CONFIG_ACPI_FAN || true
$cfg --enable CONFIG_ACPI_PROCESSOR || true
$cfg --enable CONFIG_ACPI_THERMAL || true
$cfg --enable CONFIG_X86_ACPI_CPUFREQ || true

make olddefconfig
make -j"$(nproc)" bzImage
mkdir -p "${VILA_BUILD}" "${HOST}/build" "${OLD_BUILD}"
cp -f arch/x86/boot/bzImage "${VILA_BUILD}/bzImage"
cp -f arch/x86/boot/bzImage "${OLD_BUILD}/bzImage"
cp -f arch/x86/boot/bzImage "${HOST}/build/bzImage"
ls -lh "${HOST}/build/bzImage"
grep -E '^CONFIG_ACPI|^CONFIG_PM' .config | head -20
echo "kernel ready with ACPI/PM"
