param(
    [switch]$BuildOnly,
    [switch]$Test,
    [ValidateSet('auto', 'g++', 'msvc')]
    [string]$Compiler = 'auto'
)

$ErrorActionPreference = 'Stop'
$projectRoot = $PSScriptRoot
$exitCode = 0
Push-Location $projectRoot

try {
    $gnuCompiler = Get-Command 'g++.exe' -ErrorAction SilentlyContinue
    $msvcCompiler = Get-Command 'cl.exe' -ErrorAction SilentlyContinue
    if ($Compiler -eq 'auto') {
        if ($gnuCompiler) { $Compiler = 'g++' }
        elseif ($msvcCompiler) { $Compiler = 'msvc' }
        else { throw 'No C++ compiler found. Use Developer PowerShell for Visual Studio, or install MinGW-w64 and add its bin folder to PATH.' }
    }

    $buildFolder = Join-Path $projectRoot 'build'
    $objectFolder = Join-Path $buildFolder 'obj'
    New-Item -ItemType Directory -Force -Path $objectFolder | Out-Null

    $sources = @('config.cpp', 'instruction.cpp', 'process.cpp', 'scheduler.cpp', 'report.cpp')
    if ($Test) {
        $sources += 'tests\scheduler_tests.cpp'
        $output = Join-Path $buildFolder 'scheduler_tests.exe'
    }
    else {
        $sources += @('main.cpp', 'console.cpp')
        $output = Join-Path $buildFolder 'ProcessScheduler.exe'
    }
    $sources = @($sources | ForEach-Object { Join-Path $projectRoot $_ })

    Write-Host "Building with $Compiler..."
    Push-Location $objectFolder
    try {
        if ($Compiler -eq 'g++') {
            if (-not $gnuCompiler) { throw 'g++.exe was not found on PATH.' }
            & $gnuCompiler.Source -std=c++20 -O2 -Wall -Wextra -Wpedantic -pthread @sources -o $output
        }
        else {
            if (-not $msvcCompiler) { throw 'cl.exe was not found. Open Developer PowerShell for Visual Studio and try again.' }
            & $msvcCompiler.Source /nologo /std:c++20 /EHsc /W4 /O2 /MT @sources "/Fe:$output"
        }
        if ($LASTEXITCODE -ne 0) { throw 'Build failed. The existing executable will not be launched.' }
    }
    finally { Pop-Location }

    Write-Host "Build succeeded: $output"
    if (-not $BuildOnly) {
        & $output
        $exitCode = $LASTEXITCODE
    }
}
catch {
    Write-Host $_.Exception.Message -ForegroundColor Red
    $exitCode = 1
}
finally { Pop-Location }

exit $exitCode
