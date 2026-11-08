[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$PackageDirectory,
    [Parameter(Mandatory)][string]$OutputFile,
    [string]$Version='0.1.5'
)
$ErrorActionPreference='Stop'
$PackageDirectory=[IO.Path]::GetFullPath($PackageDirectory)
$OutputFile=[IO.Path]::GetFullPath($OutputFile)
if(Test-Path -LiteralPath $OutputFile){throw 'Installer already exists; choose a new output file.'}
$manifest=Get-Content -LiteralPath (Join-Path $PackageDirectory 'build-manifest.json') -Raw|ConvertFrom-Json
foreach($entry in $manifest.files){
    $path=[IO.Path]::GetFullPath((Join-Path $PackageDirectory $entry.path))
    if(-not $path.StartsWith($PackageDirectory+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase)){throw 'Unsafe manifest path'}
    if((Get-FileHash -LiteralPath $path).Hash.ToLowerInvariant() -ne $entry.sha256){throw "Package differs from manifest: $($entry.path)"}
}
$compiler=Get-Command makensis.exe -ErrorAction Stop
New-Item -ItemType Directory -Force -Path (Split-Path $OutputFile)|Out-Null
& $compiler.Source "/DPACKAGE_DIR=$PackageDirectory" "/DOUTPUT_FILE=$OutputFile" "/DAPP_VERSION=$Version" "$PSScriptRoot/installer.nsi"
if($LASTEXITCODE -ne 0){throw 'NSIS compilation failed'}
[ordered]@{file=$OutputFile;bytes=(Get-Item -LiteralPath $OutputFile).Length;sha256=(Get-FileHash -LiteralPath $OutputFile -Algorithm SHA256).Hash.ToLowerInvariant()}|ConvertTo-Json
