param(
    [switch]$Test,            # zusaetzlich den Test bauen und laufen lassen
    [string]$AbiOriginal = "",   # OTHELLO_ABI.DLL (unveraendert) fuer den Test
    [string]$AbiGepatcht = ""    # OTHELLO_ABI.DLL aus einer gepatchten VDF (freiwillig)
)
# Baut OthelloModUnlock.dll (32 Bit, statische Laufzeit - keine msvcr-Abhaengigkeit).
# Braucht die Visual Studio Build Tools (getestet: 2019, v142). Ergebnis in Bau\.

$ErrorActionPreference = 'Stop'
$wurzel = Split-Path -Parent $PSScriptRoot
$bau = Join-Path $wurzel 'Bau'
New-Item -ItemType Directory -Force $bau | Out-Null

# vcvars32 suchen (2022, 2019; Community/Professional/Enterprise/BuildTools)
$vcvars = $null
foreach ($p in @(
    "${env:ProgramFiles}\Microsoft Visual Studio\2022",
    "${env:ProgramFiles(x86)}\Microsoft Visual Studio\2022",
    "${env:ProgramFiles(x86)}\Microsoft Visual Studio\2019")) {
    if (-not (Test-Path $p)) { continue }
    foreach ($e in @('BuildTools', 'Community', 'Professional', 'Enterprise')) {
        $k = Join-Path $p "$e\VC\Auxiliary\Build\vcvars32.bat"
        if (Test-Path $k) { $vcvars = $k; break }
    }
    if ($vcvars) { break }
}
if (-not $vcvars) { throw "vcvars32.bat nicht gefunden - Visual Studio Build Tools (C++, x86) installieren" }
"Werkzeuge: $vcvars"

$quelle = Join-Path $wurzel 'Quelltext\OthelloModUnlock.cpp'
$res    = Join-Path $wurzel 'Quelltext\OthelloModUnlock.rc'

# /MT: statische Laufzeit, die DLL haengt an keiner msvcr-Datei des Spielordners
$cl = "cl /nologo /c /O2 /MT /W4 /EHsc /DWIN32 /D_CRT_SECURE_NO_WARNINGS " +
      "/Fo`"$bau\OthelloModUnlock.obj`" `"$quelle`""
$rc = "rc /nologo /fo `"$bau\OthelloModUnlock.res`" `"$res`""
$link = "link /nologo /DLL /SUBSYSTEM:WINDOWS /MACHINE:X86 /OPT:REF /OPT:ICF " +
        "/OUT:`"$bau\OthelloModUnlock.dll`" `"$bau\OthelloModUnlock.obj`" `"$bau\OthelloModUnlock.res`" kernel32.lib"

Remove-Item "$bau\OthelloModUnlock.dll" -ErrorAction SilentlyContinue
$log = Join-Path $bau 'bau.log'
cmd /c "(call `"$vcvars`" && $cl && $rc && $link) > `"$log`" 2>&1"
$ausgabe = if (Test-Path $log) { Get-Content $log } else { @() }
$ausgabe | Where-Object { $_ -match 'error|warning|fatal' } | ForEach-Object { $_ }
if (-not (Test-Path "$bau\OthelloModUnlock.dll")) { $ausgabe; throw "Bau fehlgeschlagen (siehe $log)" }
$dll = Get-Item "$bau\OthelloModUnlock.dll"
"DLL: $($dll.FullName)  $($dll.Length) Byte  SHA-256 $((Get-FileHash $dll.FullName).Hash.Substring(0,16))"

if (-not $Test) { return }

if (-not $AbiOriginal) { throw "-Test braucht -AbiOriginal <OTHELLO_ABI.DLL>" }
$img = Join-Path $bau 'othello_abi.img'
python (Join-Path $wurzel 'Werkzeuge\abbild.py') $AbiOriginal $img
$imgArgs = "`"$img`""
if ($AbiGepatcht) {
    $img2 = Join-Path $bau 'othello_abi_gepatcht.img'
    python (Join-Path $wurzel 'Werkzeuge\abbild.py') $AbiGepatcht $img2
    $imgArgs += " `"$img2`""
}
$tquelle = Join-Path $wurzel 'Test\unlock_test.cpp'
$tcl = "cl /nologo /O2 /MT /W4 /EHsc /DWIN32 /D_CRT_SECURE_NO_WARNINGS " +
       "/Fo`"$bau\unlock_test.obj`" /Fe`"$bau\unlock_test.exe`" `"$tquelle`" /link /MACHINE:X86 /FIXED:NO"
Remove-Item "$bau\unlock_test.exe" -ErrorAction SilentlyContinue
$tlog = Join-Path $bau 'test-bau.log'
cmd /c "(call `"$vcvars`" && $tcl) > `"$tlog`" 2>&1"
$tausgabe = if (Test-Path $tlog) { Get-Content $tlog } else { @() }
$tausgabe | Where-Object { $_ -match ': error|: fatal' } | ForEach-Object { $_ }
if (-not (Test-Path "$bau\unlock_test.exe")) { $tausgabe; throw "Test-Bau fehlgeschlagen (siehe $tlog)" }
cmd /c "`"$bau\unlock_test.exe`" $imgArgs"
if ($LASTEXITCODE -ne 0) { throw "Test fehlgeschlagen (Code $LASTEXITCODE)" }

