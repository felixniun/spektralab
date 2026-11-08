[CmdletBinding()]
param(
    [string]$BuildDirectory = (Join-Path $PSScriptRoot '..\build\msvc-release'),
    [string]$DependencyDirectory = (Join-Path $PSScriptRoot '..\build\deps'),
    [string]$GlslangExecutable,
    [switch]$SkipTests
)
$ErrorActionPreference = 'Stop'
$repo = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$BuildDirectory = [IO.Path]::GetFullPath($BuildDirectory)
$DependencyDirectory = [IO.Path]::GetFullPath($DependencyDirectory)
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
if (-not (Test-Path -LiteralPath $vswhere)) { throw 'Visual Studio C++ Build Tools are required.' }
$vs = & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $vs) { throw 'Visual Studio C++ x64 tools were not found.' }
$devcmd = Join-Path $vs 'Common7\Tools\VsDevCmd.bat'
# Import only into this process; no registry or machine PATH changes.
& $env:ComSpec /d /c "call `"$devcmd`" -arch=x64 -no_logo && set" | ForEach-Object {
    if ($_ -match '^([^=]+)=(.*)$') { [Environment]::SetEnvironmentVariable($matches[1], $matches[2], 'Process') }
}
if ($LASTEXITCODE -ne 0) { throw 'Visual Studio environment initialization failed.' }
$env:VSLANG = '1033'
$cmake = Join-Path $vs 'Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe'
$ctest = Join-Path (Split-Path $cmake) 'ctest.exe'
if (-not $GlslangExecutable) {
    $candidate = Join-Path $DependencyDirectory 'glslang-build\StandAlone\Release\glslang.exe'
    if (Test-Path -LiteralPath $candidate) { $GlslangExecutable = $candidate }
    else { $GlslangExecutable = (Get-Command glslangValidator.exe -ErrorAction SilentlyContinue).Source }
}
if (-not $GlslangExecutable -or -not (Test-Path -LiteralPath $GlslangExecutable)) {
    throw 'Pass -GlslangExecutable or build the pinned glslang tool first.'
}
New-Item -ItemType Directory -Force -Path $BuildDirectory | Out-Null
# MSBuild tracks headers without Ninja's localized /showIncludes prefix.
$configure = @('-S', $repo, '-B', $BuildDirectory, '-G', 'Visual Studio 17 2022', '-A', 'x64',
    '-DCMAKE_CONFIGURATION_TYPES=Release', '-DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded',
    '-DCMAKE_SUPPRESS_REGENERATION=ON',
    '-DSPEKTRALAB_BUILD_NATIVE_RAW=ON', '-DSPEKTRALAB_BUILD_HOST=ON',
    '-DSPEKTRALAB_BUILD_DESKTOP=OFF', '-DSPEKTRALAB_BUILD_QT_UI=OFF', '-DSPEKTRALAB_REQUIRE_VULKAN=ON',
    "-DSPEKTRALAB_DEPS_DIR=$DependencyDirectory",
    "-DSPEKTRALAB_LIBRAW_SOURCE=$(Join-Path $DependencyDirectory 'LibRaw-0.22.2')",
    "-DSPEKTRALAB_VULKAN_HEADERS=$(Join-Path $DependencyDirectory 'Vulkan-Headers\include')",
    "-DSPEKTRALAB_GLSLANG=$GlslangExecutable")
& $cmake @configure *> (Join-Path $BuildDirectory 'configure.log')
if ($LASTEXITCODE -ne 0) { throw "CMake configure failed: $BuildDirectory\configure.log" }
# Serial MSBuild avoids silent project-reference failures in CMake's generated solution.
& $cmake --build $BuildDirectory --config Release --parallel 1 *> (Join-Path $BuildDirectory 'build.log')
if ($LASTEXITCODE -ne 0) { throw "CMake build failed: $BuildDirectory\build.log" }
if (-not $SkipTests) {
    & $ctest --test-dir $BuildDirectory -C Release --output-on-failure *> (Join-Path $BuildDirectory 'ctest.log')
    if ($LASTEXITCODE -ne 0) { throw "CTest failed: $BuildDirectory\ctest.log" }
}
$stage = Join-Path $repo 'build\host-win-x64'
New-Item -ItemType Directory -Force -Path $stage, (Join-Path $stage 'engine'), (Join-Path $stage 'licenses') | Out-Null
Copy-Item -LiteralPath (Join-Path $BuildDirectory 'engine\host\Release\spektralab-host.exe') -Destination $stage
Copy-Item -Path (Join-Path $BuildDirectory 'engine\resources\*') -Destination (Join-Path $stage 'engine') -Recurse -Force
. (Join-Path $PSScriptRoot 'product-resources.ps1')
Sync-ProductResources $repo (Join-Path $stage 'engine')
$notices = @{
    'license\LICENSE' = 'SpektraLab-GPL-3.0.txt'
    'license\SPEKTRAFILM_LICENSE.txt' = 'spektrafilm-LICENSE.txt'
    'engine\third_party\stb\LICENSE' = 'stb-LICENSE.txt'
    'engine\third_party\volk\LICENSE.md' = 'volk-LICENSE.txt'
    'modern_UI\Spektrafilm\Spektrafilm\Resources\Licenses\Profiles-and-LUTs-CC-BY-SA-4.0.txt' = 'Profiles-and-LUTs-CC-BY-SA-4.0.txt'
    'modern_UI\Spektrafilm\Spektrafilm\Resources\Licenses\Profiles-and-LUTs-CHANGELOG.txt' = 'Profiles-and-LUTs-CHANGELOG.txt'
}
foreach ($entry in $notices.GetEnumerator()) {
    Copy-Item -LiteralPath (Join-Path $repo $entry.Key) -Destination (Join-Path $stage "licenses\$($entry.Value)")
}
Copy-Item -LiteralPath (Join-Path $DependencyDirectory 'LibRaw-0.22.2\LICENSE.LGPL') -Destination (Join-Path $stage 'licenses\LibRaw-LGPL-2.1.txt')
Copy-Item -LiteralPath (Join-Path $DependencyDirectory 'LibRaw-0.22.2\COPYRIGHT') -Destination (Join-Path $stage 'licenses\LibRaw-COPYRIGHT.txt')
Copy-Item -LiteralPath (Join-Path $DependencyDirectory 'Vulkan-Headers\LICENSE.md') -Destination (Join-Path $stage 'licenses\Vulkan-Headers-LICENSE.txt')
if (-not (Test-Path (Join-Path $stage 'engine\io\sRGB.icc')) -or
    -not (Get-ChildItem (Join-Path $stage 'engine\vulkan') -Filter '*.spv')) { throw 'Incomplete host resources.' }
@{
    source_commit = (& git -C $repo rev-parse HEAD)
    compiler = (Get-Command cl.exe).Source
    glslang_sha256 = (Get-FileHash -LiteralPath $GlslangExecutable -Algorithm SHA256).Hash
    host_sha256 = (Get-FileHash -LiteralPath (Join-Path $stage 'spektralab-host.exe') -Algorithm SHA256).Hash
    tests_run = (-not $SkipTests)
} | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $stage 'build-info.json') -Encoding utf8
Write-Output "Windows host staged: $stage"
