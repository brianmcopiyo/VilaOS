# Build Multiboot2 ELF kernel. Host MinGW cannot emit ELF; try WSL, else stub.

$ErrorActionPreference = "Continue"
$kb = Split-Path $PSScriptRoot -Parent
$src = Join-Path $kb "src"
$out = Join-Path $kb "out"
$kernel = Join-Path $kb "iso_root\boot\kernel.bin"
$ld = Join-Path $kb "linker.ld"
$sh = Join-Path $PSScriptRoot "build-kernel-wsl.sh"

New-Item -ItemType Directory -Force -Path $out | Out-Null
New-Item -ItemType Directory -Force -Path (Split-Path $kernel) | Out-Null

function Convert-ToWslPath([string]$winPath) {
    $forward = $winPath -replace '\\', '/'
    $p = & wsl -e wslpath -a $forward 2>$null
    if ($LASTEXITCODE -ne 0 -or [string]::IsNullOrWhiteSpace($p)) { return $null }
    return ($p | Select-Object -Last 1).Trim()
}

function Try-WslElf {
    if (-not (Get-Command wsl -ErrorAction SilentlyContinue)) { return $false }
    $wSrc = Convert-ToWslPath $src
    $wOut = Convert-ToWslPath $out
    $wLd = Convert-ToWslPath $ld
    $wKernel = Convert-ToWslPath $kernel
    $wSh = Convert-ToWslPath $sh
    if (-not $wSrc -or -not $wOut -or -not $wLd -or -not $wKernel -or -not $wSh) { return $false }

    Write-Host "Building Multiboot2 kernel via WSL..."
    & wsl -e bash "$wSh" $wSrc $wOut $wLd $wKernel
    return ($LASTEXITCODE -eq 0 -and (Test-Path $kernel) -and ((Get-Item $kernel).Length -gt 64))
}

function Write-StubKernel {
    Write-Host "WARNING: No ELF toolchain — writing stub kernel.bin (UI still staged)."
    $bytes = [System.Text.Encoding]::ASCII.GetBytes(
        "Terminal OS stub kernel. Build with WSL i686-elf-gcc or gcc -m32 for Multiboot2.`n")
    [System.IO.File]::WriteAllBytes($kernel, $bytes)
}

if (Try-WslElf) {
    Write-Host "Wrote $kernel"
    exit 0
}

Write-StubKernel
exit 0
