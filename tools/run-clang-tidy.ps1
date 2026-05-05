param(
    [string[]]$Path = @("src", "include"),
    [string]$BuildDir = "cmake-build-debug",
    [string]$ClangTidy = ""
)

$ErrorActionPreference = "Stop"

if (-not $ClangTidy) {
    $cmd = Get-Command clang-tidy -ErrorAction SilentlyContinue
    if ($cmd) {
        $ClangTidy = $cmd.Source
    }
}

if (-not $ClangTidy) {
    $vsPath = "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Tools\Llvm\x64\bin\clang-tidy.exe"
    if (Test-Path $vsPath) {
        $ClangTidy = $vsPath
    }
}

if (-not $ClangTidy -or -not (Test-Path $ClangTidy)) {
    throw "clang-tidy was not found. Install LLVM or pass -ClangTidy <path-to-clang-tidy.exe>."
}

$compileCommands = Join-Path $BuildDir "compile_commands.json"
if (-not (Test-Path $compileCommands)) {
    throw "Missing $compileCommands. Reconfigure CMake first so CMAKE_EXPORT_COMPILE_COMMANDS is generated."
}

$files = foreach ($entry in $Path) {
    if (Test-Path $entry -PathType Leaf) {
        Get-Item $entry
    } elseif (Test-Path $entry -PathType Container) {
        Get-ChildItem $entry -Recurse -File -Include *.cpp,*.cxx,*.cc
    } else {
        throw "Path not found: $entry"
    }
}

foreach ($file in $files) {
    Write-Host "clang-tidy $($file.FullName)"
    & $ClangTidy $file.FullName -p $BuildDir
    if ($LASTEXITCODE -ne 0) {
        exit $LASTEXITCODE
    }
}
