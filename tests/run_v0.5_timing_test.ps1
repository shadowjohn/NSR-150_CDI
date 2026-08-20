[CmdletBinding()]
param()

$projectRoot = Split-Path -Parent $PSScriptRoot
$source = Join-Path $PSScriptRoot 'v0.5_timing_test.cpp'
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'

if (-not (Test-Path -LiteralPath $vswhere)) {
  throw '找不到 Visual Studio Build Tools；請安裝 C++ Build Tools 後再執行此測試。'
}

$installationPath = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
$vsDevCmd = Join-Path $installationPath 'Common7\Tools\VsDevCmd.bat'

if (-not $installationPath -or -not (Test-Path -LiteralPath $vsDevCmd)) {
  throw '找不到可用的 Visual Studio C++ 編譯環境。'
}

$testDirectory = Join-Path ([IO.Path]::GetTempPath()) 'nsr-cdi-v0.5-test'
New-Item -ItemType Directory -Force -Path $testDirectory | Out-Null
$testExecutable = Join-Path $testDirectory 'v0.5_timing_test.exe'
$testObject = Join-Path $testDirectory 'v0.5_timing_test.obj'

& cmd.exe /d /c ('"' + $vsDevCmd + '" -arch=x64 >nul && cl.exe /nologo /utf-8 /std:c++14 /EHsc /W4 /WX /Fo:"' + $testObject + '" /Fe:"' + $testExecutable + '" "' + $source + '"')
if ($LASTEXITCODE -ne 0) {
  exit $LASTEXITCODE
}

& $testExecutable
exit $LASTEXITCODE
