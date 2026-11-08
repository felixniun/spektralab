[CmdletBinding()]
param([string]$VisualStudioPath, [string]$NodeDirectory)
$ErrorActionPreference = 'Stop'
$repo = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$desktop = Join-Path $repo 'desktop'
. (Join-Path $PSScriptRoot 'desktop-build-inputs.ps1')
if (-not $VisualStudioPath) {
    $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
    if (Test-Path -LiteralPath $vswhere) {
        $VisualStudioPath = & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
    }
}
$vsDevCmd = if ($VisualStudioPath) { Join-Path $VisualStudioPath 'Common7\Tools\VsDevCmd.bat' } else { '' }
if (-not $vsDevCmd -or -not (Test-Path -LiteralPath $vsDevCmd)) {
    throw 'MSVC C++ Build Tools are missing. Install the C++ desktop workload (MSVC x64 and Windows SDK), then rerun; or pass -VisualStudioPath.'
}
if (-not $NodeDirectory) { $NodeDirectory = Split-Path (Get-Command node.exe -ErrorAction Stop).Source }
$node = Join-Path $NodeDirectory 'node.exe'
if (-not (Test-Path -LiteralPath (Join-Path $NodeDirectory 'npm.cmd'))) { throw 'Pass -NodeDirectory pointing to a Node.js installation that includes npm.cmd.' }
$cli = Join-Path $desktop 'node_modules\@tauri-apps\cli\tauri.js'
if (-not (Test-Path -LiteralPath $cli)) { throw 'Run npm ci in desktop/ first.' }
$savedEnvironment = @{}
Get-ChildItem Env: | ForEach-Object { $savedEnvironment[$_.Name] = $_.Value }
try {
    # Import the compiler environment into this process only.
    $vsEnvironment = & $env:ComSpec /d /s /c "`"`"$vsDevCmd`" -no_logo -arch=x64 -host_arch=x64 && set`""
    if ($LASTEXITCODE -ne 0) { throw 'Visual Studio environment initialization failed.' }
    foreach ($line in $vsEnvironment) {
        if ($line -match '^([^=]+)=(.*)$') { [Environment]::SetEnvironmentVariable($Matches[1], $Matches[2], 'Process') }
    }
    $cachedCargo = Join-Path $repo 'build\deps\cargo'
    $cachedRustup = Join-Path $repo 'build\deps\rustup'
    if (Test-Path -LiteralPath (Join-Path $cachedCargo 'bin\cargo.exe')) { $env:CARGO_HOME = $cachedCargo; $env:RUSTUP_HOME = $cachedRustup; $env:PATH = (Join-Path $cachedCargo 'bin') + ';' + $env:PATH }
    $env:PATH = $NodeDirectory + ';' + $env:PATH
    Push-Location $desktop
    try {
        # The CLI builds the web assets and explicitly enables the production protocol.
        & $node $cli build --no-bundle --target x86_64-pc-windows-msvc --features custom-protocol
        if ($LASTEXITCODE -ne 0) { throw 'Tauri production build failed; no release manifest was written.' }
        $release = Join-Path $desktop 'src-tauri\target\x86_64-pc-windows-msvc\release'
        $info = [ordered]@{
            production = $true
            custom_protocol = $true
            rustc_version = ((& rustc --version) -join ' ')
            cargo_version = ((& cargo --version) -join ' ')
            built_utc = [DateTime]::UtcNow.ToString('o')
            desktop_app_sha256 = (Get-FileHash -LiteralPath (Join-Path $release 'spektralab.exe') -Algorithm SHA256).Hash.ToLowerInvariant()
            inputs = Get-DesktopBuildInputs $repo
        }
        $info | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath (Join-Path $release 'desktop-build-info.json') -Encoding utf8
        Write-Output "Production desktop built: $release"
    } finally { Pop-Location }
} finally {
    foreach ($entry in @(Get-ChildItem Env:)) { if (-not $savedEnvironment.ContainsKey($entry.Name)) { [Environment]::SetEnvironmentVariable($entry.Name, $null, 'Process') } }
    foreach ($name in $savedEnvironment.Keys) { [Environment]::SetEnvironmentVariable($name, $savedEnvironment[$name], 'Process') }
}
