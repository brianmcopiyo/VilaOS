#!/bin/bash
set -euo pipefail
cd /mnt/c/Users/HP/Desktop/OS
pwd
export DEBIAN_FRONTEND=noninteractive
SUDO="sudo"
if ! sudo -n true 2>/dev/null; then
  SUDO="sudo -S"
  export SUDO_ASKPASS=
fi
run_sudo() {
  if sudo -n true 2>/dev/null; then
    sudo "$@"
  else
    printf '%s\n' 'mudita' | sudo -S "$@"
  fi
}
run_sudo apt-get update
run_sudo apt-get install -y build-essential musl-tools curl bison flex libelf-dev libssl-dev bc grub-pc-bin grub-common xorriso mtools cpio gzip rsync
bash scripts/build_vbox_iso.sh
ls -lh build/vilaos.iso
echo BUILD_DONE
