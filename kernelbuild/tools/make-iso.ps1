# Pack kernelbuild/iso_root into out/terminal.iso
# Prefers: WSL grub-mkrescue → oscdimg (Windows ADK) → zip-backed fallback.

$ErrorActionPreference = "Continue"
$kb = Split-Path $PSScriptRoot -Parent
$isoRoot = Join-Path $kb "iso_root"
$outDir = Join-Path $kb "out"
$isoPath = Join-Path $outDir "terminal.iso"

New-Item -ItemType Directory -Force -Path $outDir | Out-Null

if (-not (Test-Path (Join-Path $isoRoot "boot\grub\grub.cfg"))) {
    throw "Missing iso_root/boot/grub/grub.cfg — run make stage first."
}

function Convert-ToWslPath([string]$winPath) {
    $forward = $winPath -replace '\\', '/'
    $out = & wsl -e wslpath -a $forward 2>$null
    if ($LASTEXITCODE -ne 0 -or [string]::IsNullOrWhiteSpace($out)) {
        return $null
    }
    return ($out | Select-Object -Last 1).Trim()
}

function Try-GrubMkRescue {
    if (-not (Get-Command wsl -ErrorAction SilentlyContinue)) { return $false }
    $unixRoot = Convert-ToWslPath $isoRoot
    $unixIso = Convert-ToWslPath $isoPath
    if (-not $unixRoot -or -not $unixIso) { return $false }
    Write-Host "Building bootable ISO with WSL grub-mkrescue..."
    & wsl -e bash -lc "command -v grub-mkrescue >/dev/null && grub-mkrescue -o '$unixIso' '$unixRoot'"
    return ($LASTEXITCODE -eq 0 -and (Test-Path $isoPath) -and ((Get-Item $isoPath).Length -gt 0))
}

function Try-Oscdimg {
    $candidates = @(
        "${env:ProgramFiles(x86)}\Windows Kits\10\Assessment and Deployment Kit\Deployment Tools\amd64\Oscdimg\oscdimg.exe",
        "${env:ProgramFiles}\Windows Kits\10\Assessment and Deployment Kit\Deployment Tools\amd64\Oscdimg\oscdimg.exe"
    )
    $exe = $candidates | Where-Object { Test-Path $_ } | Select-Object -First 1
    if (-not $exe) { return $false }
    Write-Host "Building ISO with oscdimg..."
    & $exe -m -o -u2 -udfver102 $isoRoot $isoPath
    return ($LASTEXITCODE -eq 0 -and (Test-Path $isoPath))
}

function Write-ZipFallback {
    $zipPath = Join-Path $outDir "terminal-iso-root.zip"
    if (Test-Path $zipPath) { Remove-Item -Force $zipPath }
    if (Test-Path $isoPath) { Remove-Item -Force $isoPath }
    Write-Host "No grub-mkrescue/oscdimg found — packing iso_root as zip: $zipPath"
    Write-Host "Install WSL + grub-mkrescue, or Windows ADK (oscdimg), then re-run."
    Compress-Archive -Path (Join-Path $isoRoot "*") -DestinationPath $zipPath -Force
    Copy-Item -Force $zipPath $isoPath
    return $true
}

if (Try-GrubMkRescue) {
    Write-Host "Wrote $isoPath"
    exit 0
}
if (Try-Oscdimg) {
    Write-Host "Wrote $isoPath"
    exit 0
}
Write-ZipFallback | Out-Null
Write-Host "Wrote $isoPath (zip-backed staging fallback)"
exit 0
