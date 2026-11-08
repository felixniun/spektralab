[CmdletBinding()]
param(
    [string]$DependencyDirectory = (Join-Path $PSScriptRoot '..\build\deps'),
    [int]$Jobs = 6,
    [ValidateSet('MSVC','MinGW')][string]$Toolchain='MSVC',
    [string]$CMakeDirectory
)
$ErrorActionPreference = 'Stop'
$DependencyDirectory = [IO.Path]::GetFullPath($DependencyDirectory)
New-Item -ItemType Directory -Force -Path $DependencyDirectory | Out-Null
& (Join-Path $PSScriptRoot 'setup-libraw.ps1') -DependencyDirectory $DependencyDirectory
$pins = Get-Content -LiteralPath (Join-Path $PSScriptRoot 'windows-dependencies.json') -Raw | ConvertFrom-Json
Add-Type -AssemblyName System.IO.Compression.FileSystem
foreach ($name in @('Vulkan-Headers', 'glslang')) {
    $pin = $pins.$name
    $archiveName = if ($name -eq 'glslang') { 'glslang-source.zip' } else { 'vulkan-headers.zip' }
    $archive = Join-Path $DependencyDirectory $archiveName
    if (-not (Test-Path -LiteralPath $archive)) { Invoke-WebRequest -Uri $pin.url -OutFile $archive }
    if ((Get-FileHash -LiteralPath $archive -Algorithm SHA256).Hash.ToLowerInvariant() -ne $pin.sha256) {
        throw "$name archive checksum mismatch. No files were extracted."
    }
    $folder = if ($name -eq 'glslang') { "glslang-$($pin.commit)" } else { $name }
    $root = Join-Path $DependencyDirectory $folder
    $zip = [IO.Compression.ZipFile]::OpenRead($archive)
    try {
        foreach ($entry in $zip.Entries) {
            if (-not $entry.Name) { continue }
            $relative = $entry.FullName.Substring($entry.FullName.IndexOf('/') + 1)
            $target = [IO.Path]::GetFullPath((Join-Path $root $relative))
            if (-not $target.StartsWith($root + [IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase)) {
                throw "Unsafe $name archive path"
            }
            $stream = $entry.Open()
            $hasher = [Security.Cryptography.SHA256]::Create()
            try { $digest = [Convert]::ToHexString($hasher.ComputeHash($stream)) }
            finally { $hasher.Dispose(); $stream.Dispose() }
            if (Test-Path -LiteralPath $target) {
                if ((Get-FileHash -LiteralPath $target -Algorithm SHA256).Hash -ne $digest) {
                    throw "Modified dependency source: $target. Existing file was not overwritten."
                }
            } else {
                New-Item -ItemType Directory -Force -Path (Split-Path $target) | Out-Null
                [IO.Compression.ZipFileExtensions]::ExtractToFile($entry, $target)
            }
        }
    } finally { $zip.Dispose() }
    Write-Output "$name source verified."
}
if($Toolchain -eq 'MinGW'){
    $compiler=Join-Path $DependencyDirectory 'qt-6.11.3/Tools/mingw1310_64/bin'
    if(-not (Test-Path -LiteralPath (Join-Path $compiler 'g++.exe'))){throw 'Run windows_UI/setup-qt.ps1 first.'}
    $cmake=if($CMakeDirectory){Join-Path $CMakeDirectory 'cmake.exe'}else{(Get-Command cmake.exe -ErrorAction Stop).Source}
    $savedPath=$env:PATH
    try{
        $env:PATH="$compiler;$savedPath"
        $build=Join-Path $DependencyDirectory 'glslang-build-mingw'
        & $cmake -S (Join-Path $DependencyDirectory "glslang-$($pins.glslang.commit)") -B $build -G 'MinGW Makefiles' `
            "-DCMAKE_MAKE_PROGRAM=$compiler/mingw32-make.exe" "-DCMAKE_C_COMPILER=$compiler/gcc.exe" "-DCMAKE_CXX_COMPILER=$compiler/g++.exe" `
            -DCMAKE_BUILD_TYPE=Release -DENABLE_OPT=OFF -DBUILD_EXTERNAL=OFF -DGLSLANG_TESTS=OFF -DENABLE_HLSL=OFF
        if($LASTEXITCODE -ne 0){throw 'glslang MinGW configure failed'}
        & $cmake --build $build --parallel $Jobs
        if($LASTEXITCODE -ne 0){throw 'glslang MinGW build failed'}
    }finally{$env:PATH=$savedPath}
    Write-Output 'Pinned MinGW native dependencies are ready.'
    return
}
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
$vs = & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $vs) { throw 'Visual Studio 2022 C++ x64 Build Tools are required.' }
$cmake = Join-Path $vs 'Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe'
$build = Join-Path $DependencyDirectory 'glslang-build'
New-Item -ItemType Directory -Force -Path $build | Out-Null
& $cmake -S (Join-Path $DependencyDirectory "glslang-$($pins.glslang.commit)") -B $build -G 'Visual Studio 17 2022' -A x64 `
    -DCMAKE_CONFIGURATION_TYPES=Release -DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded `
    -DENABLE_OPT=OFF -DBUILD_EXTERNAL=OFF -DGLSLANG_TESTS=OFF -DENABLE_HLSL=OFF *> (Join-Path $build 'configure.log')
if ($LASTEXITCODE -ne 0) { throw "glslang configure failed: $build\configure.log" }
& $cmake --build $build --config Release --parallel $Jobs *> (Join-Path $build 'build.log')
if ($LASTEXITCODE -ne 0) { throw "glslang build failed: $build\build.log" }
Write-Output 'Pinned Windows native dependencies are ready.'
