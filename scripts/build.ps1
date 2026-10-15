param([switch]$GuiTests)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
Push-Location $projectRoot
try {
    # Firmware needs the clang driver and LLVM objcopy, not clang-cl.
    if (-not (Get-Command clang -ErrorAction SilentlyContinue)) {
        $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
        if (Test-Path -LiteralPath $vswhere) {
            $clangPath = & $vswhere -latest -products '*' -find 'VC\Tools\Llvm\x64\bin\clang.exe' | Select-Object -First 1
            if ($clangPath) { $env:PATH = (Split-Path -Parent $clangPath) + ';' + $env:PATH }
        }
    }
    if (-not (Get-Command clang -ErrorAction SilentlyContinue)) {
        throw 'Install LLVM (clang, lld, llvm-objcopy), or the Visual Studio C++ Clang tools.'
    }
    $gui = if ($GuiTests) { 'ON' } else { 'OFF' }
    & cmake --preset windows '-DCSIM_GUI=ON' "-DCSIM_GUI_TESTS=$gui"
    if ($LASTEXITCODE -ne 0) { throw 'CMake configure failed' }
    & cmake --build --preset windows --parallel
    if ($LASTEXITCODE -ne 0) { throw 'Build failed' }
    & ctest --preset windows
    if ($LASTEXITCODE -ne 0) { throw 'Tests failed' }
} finally { Pop-Location }
