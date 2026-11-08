[CmdletBinding()]
param(
    [string]$BuildDirectory = (Join-Path $PSScriptRoot '../build/qt-native'),
    [string]$OutputDirectory = (Join-Path $PSScriptRoot '../build/release-qt-native'),
    [string]$DependencyDirectory = (Join-Path $PSScriptRoot '../build/deps')
)
$ErrorActionPreference='Stop'
$repository=Split-Path $PSScriptRoot -Parent
$BuildDirectory=[IO.Path]::GetFullPath($BuildDirectory)
$OutputDirectory=[IO.Path]::GetFullPath($OutputDirectory)
$DependencyDirectory=[IO.Path]::GetFullPath($DependencyDirectory)
$built=Join-Path $BuildDirectory 'app'
$name='SpektraLab-Qt-Windows-x64'
$portable=Join-Path $OutputDirectory $name
$zip=Join-Path $OutputDirectory "$name.zip"
if((Test-Path -LiteralPath $portable) -or (Test-Path -LiteralPath $zip)){throw 'Output already exists. Select a new output directory; existing packages are never replaced.'}
$record=Get-Content (Join-Path $BuildDirectory 'qt-build.json') -Raw | ConvertFrom-Json
. (Join-Path $PSScriptRoot 'build-inputs.ps1')
$inputs=Get-QtBuildInputs $repository
if(-not $record.inputs -or ($inputs | ConvertTo-Json -Compress) -cne ($record.inputs | ConvertTo-Json -Compress)){throw 'Source differs from the recorded build. Run windows_UI/build.ps1 before packaging.'}
$executable=Join-Path $built 'SpektraLabQt.exe'
if((Get-FileHash $executable).Hash.ToLowerInvariant() -ne $record.executable_sha256){throw 'Executable differs from the recorded build. Run windows_UI/build.ps1 first.'}
. (Join-Path $repository 'engine/product-resources.ps1')
Assert-CatalogResources $repository (Join-Path $built 'resources')
New-Item -ItemType Directory -Path $portable -Force | Out-Null
Copy-Item -LiteralPath $executable -Destination $portable
$savedPath=$env:PATH
try {
    $qt=Join-Path $DependencyDirectory 'qt-6.11.3/mingw_64'
    $compiler=Join-Path $DependencyDirectory 'qt-6.11.3/Tools/mingw1310_64/bin'
    $env:PATH="$compiler;$qt/bin;$savedPath"
    # Resolve imports into a NEW directory. No build cache, Qt SDK, unused
    # executables or stale deployment from an older frontend can leak in.
    & "$qt/bin/windeployqt.exe" --release --compiler-runtime --no-translations --no-opengl-sw --qmldir "$PSScriptRoot/qml" --dir $portable $executable
    if($LASTEXITCODE -ne 0){throw 'Qt deployment failed'}
} finally { $env:PATH=$savedPath }
foreach($directory in @('resources','licenses')){Copy-Item -LiteralPath (Join-Path $built $directory) -Destination $portable -Recurse}
# Basic is selected explicitly in main.cpp and the QML imports. The generic
# Controls module lists every other style as optional, so the deployment scan
# otherwise brings all styles along. Prune only this newly generated bundle.
function Remove-GeneratedPath([string]$Relative) {
    $target=[IO.Path]::GetFullPath((Join-Path $portable $Relative))
    if(-not $target.StartsWith($portable+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase)){throw 'Generated path escaped package'}
    if(Test-Path -LiteralPath $target){Remove-Item -LiteralPath $target -Recurse -Force}
}
foreach($style in @('Fusion','Imagine','Material','Universal','Windows','FluentWinUI3')) {
    Remove-GeneratedPath "qml/QtQuick/Controls/$style"
    Remove-GeneratedPath "Qt6QuickControls2$style.dll"
    Remove-GeneratedPath "Qt6QuickControls2$($style)StyleImpl.dll"
}
Remove-GeneratedPath 'qmltooling'
# Keep the official machine-readable SBOMs and every license/notice text.
# Alternate SBOM formats and SDK source-file inventories stay in the SDK.
Get-ChildItem -LiteralPath (Join-Path $portable 'licenses/Qt-SBOM') -File | Where-Object {$_.Name -notlike '*.spdx.json'} | ForEach-Object {
    Remove-GeneratedPath ([IO.Path]::GetRelativePath($portable,$_.FullName))
}
if(Get-ChildItem -LiteralPath $portable -Recurse -File | Where-Object {$_.Name -match 'WebView|Qt6WebEngine|Qt6Test|spk_qt_.*_test|msedge|node\.exe|\.pdb$|\.a$'}){throw 'Unexpected browser/development dependency in native package'}
@'
@echo off
setlocal
cd /d "%~dp0"
start "" "%~dp0SpektraLabQt.exe" %*
'@ | Set-Content -LiteralPath (Join-Path $portable '启动 SpektraLab.cmd') -Encoding ascii
@'
SpektraLab Windows · Qt Quick 原生预览版

双击 SpektraLabQt.exe 或「启动 SpektraLab.cmd」。保留同目录的 DLL、qml、resources 和 licenses 文件夹。
不依赖 WebView2、浏览器、Node、Python、本地网页服务器或管理员权限。
通过「文件 → 导入照片 / 导入文件夹」打开 RAW、JPEG、PNG、TIFF，也可把多张照片拖入画布。
普通图片通过原项目的 ICC/TRC 解码器转入线性 ProPhoto 后参与胶片模拟；JPG 无法恢复 RAW 的高光和位深。
相机 RAW 白平衡和高光扩展仅用于 RAW；JPEG 等照片可用显影后白平衡。
Ctrl+O 导入，Ctrl+E 导出，Ctrl+Z 撤销，Ctrl+Shift+Z / Ctrl+Y 重做；双击画布切换适合/100%。
工具栏提供撤销、重做与「历史」按钮。左侧历史记录可点击返回过去状态，也可点击灰色后续步骤重做。
工具栏「观片台」打开原生窗口，可加入当前显影快照或导入导出图片；画幅按毫米比例比较，带片边快照沿用编辑器的画幅。
观片台：拖照片移动，拖空白平移，滚轮缩放，拖右下角改变尺寸，右键旋转；D 拖动、C 点击拿起/放下、L 放大镜、0 适合窗口。
观片台支持保存/复制 PNG，自动恢复本机布局，最多 100 张；清空只删除缓存副本。尚无官网玻璃光学材质和浏览器分享面板。
每张照片在本次运行中保留最近 64 步；连续拖动一个滑块合并为一步，分支新调整会替代后续重做步骤。
照片参数会自动保存；撤销历史不跨软件重启。输入框按回车提交后可撤销照片，输入文字时 Ctrl+Z 撤销文字。
在画面上滚动鼠标滚轮即可缩放，以光标位置为中心；按住左键或中键拖动画面，无需先切换缩放模式。
支持高精度滚轮，缩放范围 1%～1600%；Ctrl+0 恢复适合窗口，Ctrl+1 切换 100%。
侧栏分界线可拖动，双击恢复宽度；「查看」菜单可折叠面板并恢复布局。

界面按原版源码恢复胶片与相纸、片边、日期后背、放大机、显影前/显影后、曲线、色轮、原图及分屏对比。
支持正负片、黑白胶片/相纸、曝光/滤镜、颗粒/光晕/辉光、裁剪/旋转/拉直、逐图保存、撤销和批量导出。
片边由原引擎生成，并写入导出；开启片边后，裁剪作用于片门内的照片。日期后背也写入实际像素。
显影后调色复用已显影画面，使用原版曝光/曲线/色轮算法；重置恢复未调色结果。
照片条按需加载；标注 RAW 的图片为相机内嵌缩略图，打开后替换为当前显影结果。
反转片直接扫描，负片经相纸；黑白胶片自动匹配黑白相纸。图像引擎及测量数据来自项目源码。
输出为 sRGB，支持 TIFF16、PNG8/16 和 JPEG8；已有文件保留。
尚未完整迁移广色域/HDR/显示器 ICC、镜头校正、半格双图配对、DI 文件导出与导出软打样。
Windows 日期字形与 macOS CoreText 仍有差异。当前显影后入口为 sRGB，尚非原版完整 ProPhoto 色彩链。
当前预览仍采用原尺寸渲染；初次 RAW 解码与颗粒渲染耗时不能代表界面刷新速度。
旧 Tauri 版的编辑记录保留在原目录，Qt 版使用自己的编辑记录，暂不自动导入旧版记录。

源码：项目 windows_UI/ 与 engine/；构建说明 windows_UI/README.md。
'@ | Set-Content -LiteralPath (Join-Path $portable '使用说明.txt') -Encoding utf8
$files=Get-ChildItem -LiteralPath $portable -Recurse -File
$total=($files | Measure-Object Length -Sum).Sum
$manifest=[ordered]@{
    frontend='Qt Quick / C++';qt=$record.qt;compiler=$record.compiler
    webview2_required=$false;local_web_server_required=$false
    build=$record;unpacked_bytes=$total
    files=@($files | ForEach-Object {[ordered]@{path=[IO.Path]::GetRelativePath($portable,$_.FullName);bytes=$_.Length;sha256=(Get-FileHash -LiteralPath $_.FullName).Hash.ToLowerInvariant()}})
}
$manifest | ConvertTo-Json -Depth 9 | Set-Content -LiteralPath (Join-Path $portable 'build-manifest.json') -Encoding utf8
Compress-Archive -LiteralPath $portable -DestinationPath $zip -CompressionLevel Optimal
[ordered]@{executable=(Join-Path $portable 'SpektraLabQt.exe');zip=$zip;unpacked_mib=[math]::Round($total/1MB,2);zip_mib=[math]::Round((Get-Item $zip).Length/1MB,2)} | ConvertTo-Json
