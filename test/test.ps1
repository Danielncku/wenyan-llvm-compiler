<#
.SYNOPSIS
    Tests for wenyan-llvm.
.DESCRIPTION
    Part 1: verbose compiler output  (wy -v)
    Part 2: runtime execution output (wyc + run)
.PARAMETER File
    Filter by filename substring
.PARAMETER Stop
    Stop on first failure
.PARAMETER NoCompile
    Skip rebuild
.PARAMETER English
    English mode (questions_en + questions_advanced_en, VERBOSE_EN=ON)
.PARAMETER Interactive
    Interactive diff (with pager)
.PARAMETER BuildDir
    Custom build directory
#>
param(
    [Alias("f")][string]$File        = "",
    [Alias("s")][switch]$Stop,
    [Alias("n")][switch]$NoCompile,
    [Alias("e")][switch]$English,
    [Alias("i")][switch]$Interactive,
    [Alias("b")][string]$BuildDir    = ""
)

$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$Root      = Split-Path -Parent $ScriptDir

if (-not $BuildDir) { $BuildDir = Join-Path $Root "build" }

# ── build ──────────────────────────────────────────────────────────────────────
if (-not $NoCompile) {
    Write-Host "Compiling..." -ForegroundColor Cyan
    $CmakeArgs = @("-B", $BuildDir, "-S", $Root, "-DCMAKE_BUILD_TYPE=Release")
    if ($English) { $CmakeArgs += "-DVERBOSE_EN=ON" }
    if (-not (Test-Path $BuildDir)) {
        & cmake @CmakeArgs
        if ($LASTEXITCODE -ne 0) { Write-Host "CMake configure failed." -ForegroundColor Red; exit 1 }
    }
    cmake --build $BuildDir
    if ($LASTEXITCODE -ne 0) { Write-Host "Build failed." -ForegroundColor Red; exit 1 }
}

$WY  = Join-Path $BuildDir "wy.exe"
$WYC = Join-Path $BuildDir "wyc.exe"
if (-not (Test-Path $WY))  { Write-Host "wy not found"  -ForegroundColor Red; exit 1 }
if (-not (Test-Path $WYC)) { Write-Host "wyc not found" -ForegroundColor Red; exit 1 }

$TmpDir = Join-Path ([System.IO.Path]::GetTempPath()) ([System.IO.Path]::GetRandomFileName())
New-Item -ItemType Directory -Path $TmpDir | Out-Null

try {

function Normalize([string]$text) {
    $text = $text -replace "﻿", ""       # BOM
    $text = $text -replace "\r", ""           # CR
    $text = $text -replace "(?s)^\n+", ""     # leading blank lines
    return $text.TrimEnd()
}

function Show-Diff([string]$expected, [string]$actual) {
    $expFile = Join-Path $TmpDir "expected.txt"
    $actFile = Join-Path $TmpDir "actual.txt"
    [System.IO.File]::WriteAllText($expFile, $expected + "`n", [System.Text.Encoding]::UTF8)
    [System.IO.File]::WriteAllText($actFile, $actual   + "`n", [System.Text.Encoding]::UTF8)
    $opts = @("-c", "core.autocrlf=false", "diff", "--ignore-cr-at-eol", "--no-index", "--color=always")
    if ($Interactive) {
        & git --no-pager @opts $expFile $actFile 2>$null | less -R "+Gg"
    } else {
        & git --no-pager @opts $expFile $actFile
    }
    $global:LASTEXITCODE = 0
}

# ── scoring per test ──────────────────────────────────────────────────────────
# 00: 3v+2e=5  W1 01: 12v+8e=20  W2 02-05: 3v+2e=5each  W3 06-11: 3v+2e=5each
# W4 12: 4v+2e=6  殿試×3: 5v+3e=8each   Grand total: 64v+41e=105
$TestVPts = @{
    "00_快速入門" = 3;  "00_quick_start" = 3
    "01_開物_定名" = 12; "01_declare_naming" = 12
    "02_流轉_易值" = 3; "02_assign_update" = 3
    "03_留墨_書法" = 3; "03_print_string" = 3
    "04_九章_算術" = 3; "04_arithmetic" = 3
    "05_山村_算術" = 3; "05_village_arithmetic" = 3
    "06_尋隱_決策" = 3; "06_decision" = 3
    "07_酒宴_循環" = 3; "07_loop" = 3
    "08_口訣_乘算" = 3; "08_multiplication_table" = 3
    "09_百雞_算術" = 3; "09_hundred_chickens" = 3
    "10_客上_倒敘" = 3; "10_reverse_string" = 3
    "11_布陣_行列" = 3; "11_array" = 3
    "12_萬化_方術" = 4; "12_function" = 4
    "割圓術" = 5; "circle_division" = 5
    "曼德博集" = 5; "mandelbrot" = 5
    "牛頓求根法" = 5; "newton_root" = 5
}
$TestEPts = @{
    "00_快速入門" = 2;  "00_quick_start" = 2
    "01_開物_定名" = 8; "01_declare_naming" = 8
    "02_流轉_易值" = 2; "02_assign_update" = 2
    "03_留墨_書法" = 2; "03_print_string" = 2
    "04_九章_算術" = 2; "04_arithmetic" = 2
    "05_山村_算術" = 2; "05_village_arithmetic" = 2
    "06_尋隱_決策" = 2; "06_decision" = 2
    "07_酒宴_循環" = 2; "07_loop" = 2
    "08_口訣_乘算" = 2; "08_multiplication_table" = 2
    "09_百雞_算術" = 2; "09_hundred_chickens" = 2
    "10_客上_倒敘" = 2; "10_reverse_string" = 2
    "11_布陣_行列" = 2; "11_array" = 2
    "12_萬化_方術" = 2; "12_function" = 2
    "割圓術" = 3; "circle_division" = 3
    "曼德博集" = 3; "mandelbrot" = 3
    "牛頓求根法" = 3; "newton_root" = 3
}

if ($English) {
    $TestDirs = @(
        Join-Path $ScriptDir "questions_en"
        Join-Path $ScriptDir "questions_advanced_en"
    )
} else {
    $TestDirs = @(
        Join-Path $ScriptDir "策問"
        Join-Path $ScriptDir "殿試"
    )
}

$AllPassed  = $true
$Part1Score = 0; $Part1Max = 0
$Part2Score = 0; $Part2Max = 0

# ── Part 1: Verbose ────────────────────────────────────────────────────────────
Write-Host ""
Write-Host "══ Part 1: Verbose Output ══" -ForegroundColor Cyan

:part1 foreach ($dir in $TestDirs) {
    if (-not (Test-Path $dir)) { continue }

    foreach ($wyFile in (Get-ChildItem -Path $dir -Filter "*.wy" | Sort-Object Name)) {
        $filename = $wyFile.Name
        if ($File -and $filename -notlike "*$File*") { continue }

        $base        = $wyFile.FullName -replace '\.wy$', ''
        $stem        = Split-Path -Leaf $base
        $verboseFile = $base + ".verbose"
        $pts         = if ($TestVPts.ContainsKey($stem)) { $TestVPts[$stem] } else { 0 }
        $Part1Max   += $pts

        if (-not (Test-Path $verboseFile)) {
            Write-Host "  " -NoNewline
            Write-Host "SKIP" -ForegroundColor Yellow -NoNewline
            Write-Host "  $filename  (no reference)"
            continue
        }

        $irFile = Join-Path $TmpDir "_ir.ll"
        $rawOut = & $WY -v $wyFile.FullName $irFile 2>&1; $global:LASTEXITCODE = 0
        $rawStr = ($rawOut | ForEach-Object { "$_" }) -join "`n"
        $actual   = Normalize $rawStr
        $expected = Normalize (Get-Content $verboseFile -Raw -Encoding UTF8)

        if ($actual -eq $expected) {
            $Part1Score += $pts
            Write-Host "  " -NoNewline
            Write-Host "PASS" -ForegroundColor Green -NoNewline
            Write-Host "  $filename  (+$pts)"
        } else {
            $AllPassed = $false
            Write-Host "  " -NoNewline
            Write-Host "FAIL" -ForegroundColor Red -NoNewline
            Write-Host "  $filename"
            Write-Host "  ── diff ──────────────────────────────────"
            Show-Diff $expected $actual
            Write-Host "  ──────────────────────────────────────────"
            if ($Stop) { break part1 }
        }
    }
}

Write-Host "  Subtotal: $Part1Score/$Part1Max" -ForegroundColor Cyan

# ── Part 2: Runtime Output ─────────────────────────────────────────────────────
Write-Host ""
Write-Host "══ Part 2: Runtime Output ══" -ForegroundColor Cyan

:part2 foreach ($dir in $TestDirs) {
    if (-not (Test-Path $dir)) { continue }

    foreach ($wyFile in (Get-ChildItem -Path $dir -Filter "*.wy" | Sort-Object Name)) {
        $filename = $wyFile.Name
        if ($File -and $filename -notlike "*$File*") { continue }

        $base         = $wyFile.FullName -replace '\.wy$', ''
        $stem         = Split-Path -Leaf $base
        $expectedFile = $base + ".expected"
        $pts          = if ($TestEPts.ContainsKey($stem)) { $TestEPts[$stem] } else { 0 }
        $Part2Max    += $pts

        if (-not (Test-Path $expectedFile)) {
            Write-Host "  " -NoNewline
            Write-Host "SKIP" -ForegroundColor Yellow -NoNewline
            Write-Host "  $filename  (no reference)"
            continue
        }

        $exeName = (Split-Path -Leaf $base) + ".exe"
        $exe     = Join-Path $TmpDir $exeName

        & $WYC $wyFile.FullName $exe 2>$null
        if ($LASTEXITCODE -ne 0) {
            $AllPassed = $false
            Write-Host "  " -NoNewline
            Write-Host "FAIL" -ForegroundColor Red -NoNewline
            Write-Host "  $filename  (compile error)"
            if ($Stop) { break part2 }
            continue
        }

        $rawOut  = & $exe 2>&1; $global:LASTEXITCODE = 0
        $rawStr  = ($rawOut | ForEach-Object { "$_" }) -join "`n"
        $actual   = Normalize $rawStr
        $expected = Normalize (Get-Content $expectedFile -Raw -Encoding UTF8)

        if ($actual -eq $expected) {
            $Part2Score += $pts
            Write-Host "  " -NoNewline
            Write-Host "PASS" -ForegroundColor Green -NoNewline
            Write-Host "  $filename  (+$pts)"
        } else {
            $AllPassed = $false
            Write-Host "  " -NoNewline
            Write-Host "FAIL" -ForegroundColor Red -NoNewline
            Write-Host "  $filename"
            Write-Host "  ── diff ──────────────────────────────────"
            Show-Diff $expected $actual
            Write-Host "  ──────────────────────────────────────────"
            if ($Stop) { break part2 }
        }
    }
}

Write-Host "  Subtotal: $Part2Score/$Part2Max" -ForegroundColor Cyan

# ── Summary ────────────────────────────────────────────────────────────────────
$Total = $Part1Score + $Part2Score
$Max   = $Part1Max   + $Part2Max

Write-Host ""
Write-Host "══ Score ══" -ForegroundColor Cyan
Write-Host "  Part 1 (verbose):  $Part1Score/$Part1Max"
Write-Host "  Part 2 (runtime):  $Part2Score/$Part2Max"
Write-Host "  Total:             $Total/$Max" -ForegroundColor Cyan

if ($AllPassed) {
    Write-Host "`nAll tests passed!" -ForegroundColor Green
    exit 0
} else {
    Write-Host "`nSome tests failed." -ForegroundColor Red
    exit 1
}

} finally {
    Remove-Item -Recurse -Force $TmpDir -ErrorAction SilentlyContinue
}
