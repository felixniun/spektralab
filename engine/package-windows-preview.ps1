[CmdletBinding()]
param(
    [string]$HostDirectory = (Join-Path $PSScriptRoot '..\build\host-win-x64'),
    [string]$WebViewRuntime = 'C:\Program Files (x86)\Microsoft\EdgeWebView\Application\156.0.4314.50',
    [string]$OutputDirectory = (Join-Path $PSScriptRoot '..\build\release-windows-preview'),
    [string]$DesktopBuildDirectory = (Join-Path $PSScriptRoot '..\desktop\src-tauri\target\x86_64-pc-windows-msvc\release'),
    [ValidateSet('Bundled', 'System')][string]$RuntimeMode = 'Bundled'
)
$ErrorActionPreference = 'Stop'
$repo = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$HostDirectory = [IO.Path]::GetFullPath($HostDirectory)
$WebViewRuntime = [IO.Path]::GetFullPath($WebViewRuntime)
$OutputDirectory = [IO.Path]::GetFullPath($OutputDirectory)
. (Join-Path $PSScriptRoot 'desktop-build-inputs.ps1')
$app = Join-Path $DesktopBuildDirectory 'spektralab.exe'
$desktopInfoPath = Join-Path $DesktopBuildDirectory 'desktop-build-info.json'
if (-not (Test-Path -LiteralPath $desktopInfoPath)) { throw 'Production desktop manifest missing. Run engine/build-desktop-windows.ps1 first; refusing to package an unverified EXE.' }
$desktopInfo = Get-Content -LiteralPath $desktopInfoPath -Raw | ConvertFrom-Json
if ($desktopInfo.production -ne $true -or $desktopInfo.custom_protocol -ne $true -or $desktopInfo.desktop_app_sha256 -ne (Get-FileHash -LiteralPath $app -Algorithm SHA256).Hash.ToLowerInvariant()) { throw 'Production desktop manifest does not match this EXE. Rebuild the desktop app.' }
$currentInputs = Get-DesktopBuildInputs $repo
$builtInputs = @($desktopInfo.inputs.PSObject.Properties)
if ($builtInputs.Count -ne $currentInputs.Count) { throw 'Desktop sources changed since the production build. Rebuild before packaging.' }
foreach ($inputFile in $builtInputs) { if ($currentInputs[$inputFile.Name] -ne $inputFile.Value) { throw "Desktop source changed since build: $($inputFile.Name)" } }
$hostExe = Join-Path $HostDirectory 'spektralab-host.exe'
. (Join-Path $PSScriptRoot 'product-resources.ps1')
Assert-CatalogResources $repo (Join-Path $HostDirectory 'engine')
if (-not (Test-Path -LiteralPath $app)) { throw "Build the Tauri release app first: $app" }
if (-not (Test-Path -LiteralPath $hostExe)) { throw "Build and test the Vulkan host first: $hostExe" }
$infoPath = Join-Path $HostDirectory 'build-info.json'
if (-not (Test-Path -LiteralPath $infoPath) -or -not (Get-Content $infoPath -Raw | ConvertFrom-Json).tests_run) {
    throw 'Refusing to package: build-info.json does not confirm native tests ran.'
}
if ($RuntimeMode -eq 'Bundled') {
if (-not (Test-Path -LiteralPath (Join-Path $WebViewRuntime 'msedgewebview2.exe'))) {
    throw "WebView2 Fixed Version runtime is missing: $WebViewRuntime"
}
$signature = Get-AuthenticodeSignature -LiteralPath (Join-Path $WebViewRuntime 'msedgewebview2.exe')
if ($signature.Status -ne 'Valid' -or $signature.SignerCertificate.Subject -notmatch 'Microsoft Corporation') {
    throw 'The bundled WebView2 runtime entry point is not signed by Microsoft.'
}
$runtimeVersion = (Get-Item -LiteralPath (Join-Path $WebViewRuntime 'msedgewebview2.exe')).VersionInfo.ProductVersion
$versionDir = (Get-Content -LiteralPath (Join-Path $PSScriptRoot 'windows-fixed-runtime.json') -Raw | ConvertFrom-Json).version
if ($runtimeVersion -notlike "$versionDir*") { throw "WebView runtime version mismatch: $runtimeVersion vs $versionDir" }
} else { $versionDir = 'System Evergreen (not bundled)' }
if (Test-Path -LiteralPath $OutputDirectory) { throw "Output already exists; move it aside before rebuilding: $OutputDirectory" }
$sevenZip = (Get-Command 7z.exe -ErrorAction Stop).Source
$portable = Join-Path $OutputDirectory 'SpektraLab-Windows-x64'
$source = Join-Path $OutputDirectory 'SpektraLab-Windows-source'
New-Item -ItemType Directory -Force -Path $portable, $source, (Join-Path $portable 'host') | Out-Null
Copy-Item -LiteralPath $app -Destination $portable
Copy-Item -LiteralPath $desktopInfoPath -Destination $portable
Copy-Item -LiteralPath $hostExe -Destination $portable
Copy-Item -LiteralPath (Join-Path $HostDirectory 'engine') -Destination (Join-Path $portable 'host') -Recurse
Copy-Item -LiteralPath (Join-Path $HostDirectory 'licenses') -Destination $portable -Recurse
Copy-Item -LiteralPath $infoPath -Destination $portable
$buildInfoPath = Join-Path $portable 'build-info.json'
$buildInfo = Get-Content -LiteralPath $buildInfoPath -Raw | ConvertFrom-Json
$buildInfo | Add-Member -NotePropertyName desktop_app_sha256 -NotePropertyValue (Get-FileHash -LiteralPath $app -Algorithm SHA256).Hash.ToLowerInvariant()
$buildInfo | Add-Member -NotePropertyName rustc_version -NotePropertyValue $desktopInfo.rustc_version
$buildInfo | Add-Member -NotePropertyName cargo_version -NotePropertyValue $desktopInfo.cargo_version
$gitStatus = @(& git -c "safe.directory=$repo" -C $repo status --short)
$buildInfo | Add-Member -NotePropertyName source_tree_dirty -NotePropertyValue ([bool]$gitStatus.Count)
$buildInfo | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath $buildInfoPath -Encoding utf8
if ($RuntimeMode -eq 'Bundled') { Copy-Item -LiteralPath $WebViewRuntime -Destination (Join-Path $portable 'WebView2Fixed') -Recurse }

$launcher = @'
@echo off
setlocal
set "APP_DIR=%~dp0"
set "RUNTIME=%APP_DIR%WebView2Fixed"
if not exist "%RUNTIME%\msedgewebview2.exe" (
  echo SpektraLab WebView2 runtime is missing. Re-extract the complete ZIP.
  pause
  exit /b 1
)
icacls "%RUNTIME%" /grant "*S-1-15-2-2:(OI)(CI)(RX)" /T /C >nul 2>&1
if errorlevel 1 goto acl_error
icacls "%RUNTIME%" /grant "*S-1-15-2-1:(OI)(CI)(RX)" /T /C >nul 2>&1
if errorlevel 1 goto acl_error
set "WEBVIEW2_BROWSER_EXECUTABLE_FOLDER=%RUNTIME%"
start "" "%APP_DIR%spektralab.exe" %*
exit /b 0
:acl_error
echo SpektraLab could not prepare WebView2 permissions. The browser sandbox remains enabled.
echo Extract the ZIP to a local folder you own, then run this launcher again.
pause
exit /b 1
'@
if ($RuntimeMode -eq 'System') {
    $launcher = @'
@echo off
setlocal
set "WEBVIEW2_BROWSER_EXECUTABLE_FOLDER="
rem Tauri discovers the installed Evergreen WebView2 Runtime automatically.
rem If it is missing, install it from https://developer.microsoft.com/microsoft-edge/webview2/
start "" "%~dp0spektralab.exe" %*
'@
}
Set-Content -LiteralPath (Join-Path $portable '启动 SpektraLab.cmd') -Value $launcher -Encoding ascii

$runtimeInstructions = if ($RuntimeMode -eq 'Bundled') {
    '内含固定版 WebView2；启动脚本设置所需的只读执行权限，保持浏览器沙箱。此版本不自动更新，后续预览包需更新运行时。'
} else {
    '轻量包使用系统安装的 Microsoft WebView2 Evergreen Runtime，不含浏览器运行时。若提示缺失，请从 https://developer.microsoft.com/microsoft-edge/webview2/ 安装 WebView2 Runtime；已有 Microsoft Edge 浏览器不能代替此运行时。'
}

$readme = @"
# SpektraLab Windows x64 技术预览

这是尚未完成全部照片工作流的技术预览，**不是 SpektraLab 1.3.3 的完整 Windows 移植**。
当前已验证基础 RAW 开图、胶片渲染、RAW 中性点白平衡拾取、部分导出、场景置位、片边和日期后背。
Windows 片边字形使用 5×7 点阵替代 macOS CoreText，且条纹片边仍不支持。
双图半格配对及其 `.spektrapair` 保存重开、Lensfun 镜头校正、LCMS 输入 ICC、HEIC/EXR、
完整 Cineon 文件导出和 P3 画布显示仍未完成。
成像也未经 Mac 参考机逐像素比对。

## 启动

完整解压 ZIP 到本机 NTFS 或其他本地磁盘目录，然后双击 **启动 SpektraLab.cmd**。
$runtimeInstructions
不要只复制 EXE；不要从网络共享/UNC 路径启动。程序需要具备 Vulkan 的显卡驱动。

运行时模式：$RuntimeMode；版本：$versionDir。
Bundled 模式校验微软签名并配置 Windows 10 AppContainer ACL；System 模式由系统管理共享运行时。

编辑记录保存在用户应用数据目录；源 RAW 照片不会被覆盖。

## 内容

- `spektralab.exe`：SpektraLab 桌面程序
- `spektralab-host.exe`、`host/engine/`：Vulkan 引擎、着色器、胶片与 ICC 资源
- `WebView2Fixed/`：仅 Bundled 离线包包含的 Microsoft WebView2 固定版本运行时
- `licenses/`：程序及随附资料许可证
- `build-info.json`：host / 桌面 EXE 构建环境、版本和校验信息；开发源码有未提交修改时会注明

该预览包没有代码签名。发布 SHA256 位于 ZIP 旁的 `SHA256SUMS.txt`。
完整状态、测试结果和剩余工作见源码包内 `WINDOWS-IMPLEMENTATION.md`。
"@
Set-Content -LiteralPath (Join-Path $portable '使用说明.md') -Value $readme -Encoding utf8

if ($RuntimeMode -eq 'Bundled') {
$runtimeFiles = Get-ChildItem -LiteralPath $WebViewRuntime -File -Recurse
$runtimeManifest = foreach ($file in $runtimeFiles) {
    [pscustomobject]@{ path = $file.FullName.Substring($WebViewRuntime.Length).TrimStart('\'); sha256 = (Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256).Hash.ToLowerInvariant() }
}
$runtimeManifest | ConvertTo-Json -Depth 3 | Set-Content -LiteralPath (Join-Path $portable 'WebView2Fixed-files.sha256.json') -Encoding utf8
}

$licenseFiles = @(
    @{ source = 'license\LICENSE'; name = 'SpektraLab-GPL-3.0.txt' },
    @{ source = 'license\SPEKTRAFILM_LICENSE.txt'; name = 'spektrafilm-LICENSE.txt' },
    @{ source = 'engine\third_party\stb\LICENSE'; name = 'stb-LICENSE.txt' },
    @{ source = 'engine\third_party\volk\LICENSE.md'; name = 'volk-LICENSE.txt' },
    @{ source = 'modern_UI\Spektrafilm\Spektrafilm\Resources\Licenses\Profiles-and-LUTs-CC-BY-SA-4.0.txt'; name = 'Profiles-and-LUTs-CC-BY-SA-4.0.txt' },
    @{ source = 'modern_UI\Spektrafilm\Spektrafilm\Resources\Licenses\Profiles-and-LUTs-CHANGELOG.txt'; name = 'Profiles-and-LUTs-CHANGELOG.txt' }
)
$sourceLicenses = Join-Path $portable 'licenses'
foreach ($item in $licenseFiles) {
    if (-not (Test-Path -LiteralPath (Join-Path $repo $item.source))) { throw "Missing license: $($item.source)" }
    Copy-Item -LiteralPath (Join-Path $repo $item.source) -Destination (Join-Path $sourceLicenses $item.name)
}
if ($RuntimeMode -eq 'Bundled') { Get-ChildItem -LiteralPath $WebViewRuntime -File -Filter 'show_third_party_software_licenses.bat' | ForEach-Object {
    Copy-Item -LiteralPath $_.FullName -Destination (Join-Path $sourceLicenses 'WebView2-third-party-licenses.cmd')
}
}

$excludeDirs = @('.git', 'build', 'node_modules', 'target', 'dist', 'release', 'artifacts')
$robocopyArgs = @($repo, $source, '/E', '/COPY:DAT', '/DCOPY:DAT', '/R:1', '/W:1', '/NFL', '/NDL', '/NJH', '/NJS')
foreach ($dir in $excludeDirs) { $robocopyArgs += @('/XD', $dir) }
& robocopy.exe @robocopyArgs | Out-Null
if ($LASTEXITCODE -gt 7) { throw "Source staging failed with robocopy exit code $LASTEXITCODE" }

$portableZip = Join-Path $OutputDirectory 'SpektraLab-Windows-x64-TechnicalPreview.zip'
$sourceZip = Join-Path $OutputDirectory 'SpektraLab-Windows-source.zip'
& $sevenZip a -tzip -mx=5 -mmt=on $portableZip (Join-Path $portable '*') *> (Join-Path $OutputDirectory 'portable-zip.log')
if ($LASTEXITCODE -ne 0) { throw 'Portable ZIP creation failed.' }
& $sevenZip a -tzip -mx=5 -mmt=on $sourceZip (Join-Path $source '*') *> (Join-Path $OutputDirectory 'source-zip.log')
if ($LASTEXITCODE -ne 0) { throw 'Source ZIP creation failed.' }
& $sevenZip t $portableZip *> (Join-Path $OutputDirectory 'portable-zip-test.log')
if ($LASTEXITCODE -ne 0) { throw 'Portable ZIP integrity test failed.' }
& $sevenZip t $sourceZip *> (Join-Path $OutputDirectory 'source-zip-test.log')
if ($LASTEXITCODE -ne 0) { throw 'Source ZIP integrity test failed.' }

$checksums = foreach ($file in @($portableZip, $sourceZip)) {
    $hash = (Get-FileHash -LiteralPath $file -Algorithm SHA256).Hash.ToLowerInvariant()
    "$hash  $([IO.Path]::GetFileName($file))"
}
Set-Content -LiteralPath (Join-Path $OutputDirectory 'SHA256SUMS.txt') -Value $checksums -Encoding ascii
Write-Output "Created preview release: $OutputDirectory"
