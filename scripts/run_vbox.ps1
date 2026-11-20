param(
    [int]$MemoryMB = 1024
)

$ErrorActionPreference = "Stop"

if ($MemoryMB -lt 256) {
    throw "MemoryMB must be at least 256 (minimum capability target)."
}
if ($MemoryMB -gt 32768) {
    throw "MemoryMB exceeds 32 GB supported scaling ceiling."
}

$VmName = "VilaOS"
$LegacyNames = @("PhoneOS", "PhoneOS-32MB")
$Root = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)
$IsoPath = Join-Path $Root "build\vilaos.iso"

function Find-VBoxManage {
    $candidates = @(
        "C:\Program Files\Oracle\VirtualBox\VBoxManage.exe",
        "C:\Program Files (x86)\Oracle\VirtualBox\VBoxManage.exe"
    )
    foreach ($p in $candidates) {
        if (Test-Path $p) { return $p }
    }
    $cmd = Get-Command VBoxManage.exe -ErrorAction SilentlyContinue
    if ($cmd) { return $cmd.Source }
    throw "VBoxManage.exe not found. Install Oracle VirtualBox."
}

function Remove-VmIfExists {
    param([string]$Name, [string]$VBox)
    $listed = & $VBox list vms | Out-String
    if ($listed -notmatch "`"$Name`"") { return }
    $running = & $VBox list runningvms | Out-String
    if ($running -match "`"$Name`"") {
        & $VBox controlvm $Name poweroff | Out-Null
        Start-Sleep -Seconds 2
    }
    & $VBox unregistervm $Name --delete | Out-Null
}

if (-not (Test-Path $IsoPath)) {
    throw "Missing ISO: $IsoPath`nBuild first in WSL: bash scripts/build_vbox_iso.sh"
}

$VBox = Find-VBoxManage
Write-Host "Using $VBox"
Write-Host "ISO $IsoPath"
Write-Host "VM $VmName memory=${MemoryMB}MB dynamic display"

foreach ($legacy in $LegacyNames) {
    Remove-VmIfExists -Name $legacy -VBox $VBox
}
Remove-VmIfExists -Name $VmName -VBox $VBox

& $VBox createvm --name $VmName --ostype Linux_64 --register | Out-Null
& $VBox modifyvm $VmName `
    --memory $MemoryMB `
    --vram 128 `
    --cpus 2 `
    --acpi on `
    --ioapic on `
    --boot1 dvd `
    --boot2 none `
    --boot3 none `
    --boot4 none `
    --graphicscontroller vmsvga `
    --accelerate3d on `
    --mouse usbtablet `
    --keyboard ps2 `
    --audio-driver none `
    --nic1 none `
    --usb off | Out-Null

& $VBox setextradata $VmName "GUI/MaxGuestResolution" "any"

& $VBox storagectl $VmName --name "IDE" --add ide --controller PIIX4 | Out-Null
& $VBox storageattach $VmName `
    --storagectl "IDE" `
    --port 0 `
    --device 0 `
    --type dvddrive `
    --medium $IsoPath | Out-Null

Write-Host "Starting $VmName (${MemoryMB} MB RAM, dynamic display, VMSVGA)..."
& $VBox startvm $VmName --type gui
Write-Host "Started."
