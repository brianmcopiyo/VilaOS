$ErrorActionPreference = "Stop"
$Root = "C:\Users\HP\Desktop\OS"
$Log = Join-Path $Root "build\post_reboot.log"
$IsoPath = Join-Path $Root "build\vilaos.iso"
$VBox = "C:\Program Files\Oracle\VirtualBox\VBoxManage.exe"
$VmName = "VilaOS"
$LegacyNames = @("PhoneOS", "PhoneOS-32MB")
$Distro = "PhoneOS-Build"
$MemoryMB = 128

function Log($m) {
    $line = "$(Get-Date -Format o) $m"
    Add-Content -Path $Log -Value $line
    Write-Host $line
}

function Remove-VmIfExists {
    param([string]$Name)
    $listed = & $VBox list vms | Out-String
    if ($listed -notmatch "`"$Name`"") { return }
    $running = & $VBox list runningvms | Out-String
    if ($running -match "`"$Name`"") {
        & $VBox controlvm $Name poweroff | Out-Null
        Start-Sleep -Seconds 3
    }
    & $VBox unregistervm $Name --delete | Out-Null
    Log "Removed $Name"
}

New-Item -ItemType Directory -Force -Path (Join-Path $Root "build") | Out-Null
Log "START post_reboot_vilaos"

try {
    $status = & wsl.exe --status 2>&1 | Out-String
    Log "wsl status: $status"
    $list = & wsl.exe -l -v 2>&1 | Out-String
    Log "wsl list: $list"

    if ($list -notmatch "PhoneOS-Build") {
        $tarball = Join-Path $Root "build\ubuntu-wsl.rootfs.tar.gz"
        if (-not (Test-Path $tarball)) { throw "Missing $tarball" }
        New-Item -ItemType Directory -Force -Path "C:\WSL\Ubuntu24" | Out-Null
        & wsl.exe --import $Distro "C:\WSL\Ubuntu24" $tarball --version 2 2>&1 | Tee-Object -FilePath (Join-Path $Root "build\wsl_import2.txt")
    }

    Log "Installing apt packages"
    & wsl.exe -d $Distro -u root -- bash -lc "export DEBIAN_FRONTEND=noninteractive; apt-get update -y && apt-get install -y build-essential bc bison flex libelf-dev libssl-dev libncurses-dev dwarves cpio gzip xz-utils curl wget make musl-tools grub-pc-bin grub-common xorriso mtools genisoimage syslinux isolinux rsync" 2>&1 | Tee-Object -FilePath (Join-Path $Root "build\apt_install.log")

    Log "Building ISO"
    & wsl.exe -d $Distro -u root -- bash -lc "cd /mnt/c/Users/HP/Desktop/OS && bash scripts/wsl_build_iso.sh" 2>&1 | Tee-Object -FilePath (Join-Path $Root "build\iso_build.log")
    if (-not (Test-Path $IsoPath)) { throw "ISO missing after build: $IsoPath" }
    Log "ISO OK size=$((Get-Item $IsoPath).Length)"

    if (-not (Test-Path $VBox)) { throw "VBoxManage missing: $VBox" }

    foreach ($legacy in $LegacyNames) {
        Remove-VmIfExists -Name $legacy
    }
    Remove-VmIfExists -Name $VmName

    & $VBox createvm --name $VmName --ostype Linux_64 --register | Out-Null
    & $VBox modifyvm $VmName --memory $MemoryMB --vram 8 --cpus 1 --acpi on --ioapic on --boot1 dvd --boot2 none --boot3 none --boot4 none --graphicscontroller vboxvga --accelerate3d off --mouse ps2 --keyboard ps2 --audio-driver none --nic1 none --usb off | Out-Null
    & $VBox storagectl $VmName --name "IDE" --add ide --controller PIIX4 | Out-Null
    & $VBox storageattach $VmName --storagectl "IDE" --port 0 --device 0 --type dvddrive --medium $IsoPath | Out-Null
    & $VBox startvm $VmName --type gui | Out-Null
    Start-Sleep -Seconds 4
    & $VBox list runningvms | Out-File (Join-Path $Root "build\runningvms.txt")
    & $VBox showvminfo $VmName --machinereadable | Out-File (Join-Path $Root "build\vminfo.txt")
    Log "DONE VM started memory=${MemoryMB}"
}
catch {
    Log "ERROR: $_"
    $_ | Out-String | Add-Content $Log
    exit 1
}

Unregister-ScheduledTask -TaskName "PhoneOSPostRebootBuild" -Confirm:$false -ErrorAction SilentlyContinue
Unregister-ScheduledTask -TaskName "VilaOSPostRebootBuild" -Confirm:$false -ErrorAction SilentlyContinue
Log "Task cleaned"
exit 0
