# SpektraLab Windows

SpektraLab 的 Windows 移植版，使用 **Qt Quick + C++ / Vulkan + LibRaw**，逐步迁移 macOS 界面和功能。运行时不需要 WebView2、浏览器、Node、Python 或本地网页服务器。本仓库是独立移植，未由原项目作者发布或背书。

## 下载与启动

从 [Releases](https://github.com/felixniun/spektralab/releases) 下载：

- `*-setup.exe`：完整安装包，默认安装到当前用户目录，无需管理员权限。
- `SpektraLab-Qt-Windows-x64.zip`：便携版，完整解压后运行 `SpektraLabQt.exe` 或 `启动 SpektraLab.cmd`。

不要只复制便携目录里的 EXE；同目录的 DLL、`qml`、`resources` 和 `licenses` 都需要保留。两种包包含相同程序和材料配置。当前是未签名的技术预览版。

要求：Windows x64、支持 Vulkan 的显卡及驱动。没有 CPU 渲染回退。RAW 支持范围取决于捆绑的 LibRaw；当前不支持 Nikon HE / HE*。

## 已有功能

- 导入 RAW、文件夹或拖入多张照片，逐图保存设置。
- 正片直接扫描，负片经过相纸；黑白胶片自动匹配黑白相纸，包含原有 33 份胶片／相纸配置。
- 胶片曝光、测光、相机白平衡、配光、滤镜、颗粒、光晕、耀光和场景置位。
- 片边、齿孔、片门、片夹和日期后背。边框进入实际渲染和文件导出；关闭片边恢复原裁剪。
- 显影后白平衡、曝光、曲线和色彩平衡，调整时复用胶片结果。
- 原图／分屏对比，滚轮围绕光标缩放，左键／中键拖动画面。
- 裁剪、旋转、翻转、拉直，TIFF16、PNG8/16、JPEG8 和批量导出。
- 撤销、重做与可点击历史列表，每张照片本次运行保留最近 64 步。
- 原生观片台：显影快照、图片导入、胶片尺寸比较、拖动／旋转／缩放、放大镜和 PNG 分享图。

## 常用操作

| 操作 | 快捷键或入口 |
| --- | --- |
| 导入 RAW | Ctrl+O |
| 导出 | Ctrl+E |
| 撤销 | Ctrl+Z |
| 重做 | Ctrl+Shift+Z 或 Ctrl+Y |
| 适合窗口／100% | Ctrl+0／Ctrl+1，或双击画面 |
| 缩放／平移 | 滚轮／按住左键或中键拖动 |
| 查看历史 | 工具栏「历史」，或「查看 → 历史记录」 |

输入数值后按回车提交，再按 Ctrl+Z 撤销照片调整；正在输入文字时，Ctrl+Z 撤销文字。参数保存在当前用户的应用数据目录，原 RAW 不会被改写。撤销历史不跨软件重启。已有导出文件不会被静默覆盖。

## 与 macOS 原版的差异

核心基于原版 1.3.3，**不能声称与 GitHub 最新版成像完全一致**。原有材料配置一致，但 RAW 解码、GPU 后端和色彩链仍有差异；最新版 CineStill 800T／背反射光晕尚未完整迁移。

输出边界为 sRGB，尚未对齐完整 ProPhoto / ColorSync / HDR 显示。镜头校正、半格双图配对、透视几何、DI 文件输出和导出软打样仍待完成。完整分辨率处理仍可能较慢。详见 [成像核对记录](windows_UI/PARITY-AUDIT.md) 和 [Windows 构建说明](windows_UI/README.md)。

## 从源码构建

准备 Git、CMake 3.22+ 和 7-Zip。Qt 脚本下载固定版本的官方 Qt 6.11.3 / MinGW 13.1，不修改系统 PATH；Qt 构建流程可使用 MinGW，无需安装 MSVC。

```powershell
git clone https://github.com/felixniun/spektralab.git
cd spektralab
$cmakeDirectory = 'C:/Tools/CMake/bin' # 改成自己的 CMake 路径
./windows_UI/setup-qt.ps1
./engine/setup-windows.ps1 -Toolchain MinGW -CMakeDirectory $cmakeDirectory
./windows_UI/build.ps1 -CMakeDirectory $cmakeDirectory -RunTests
./windows_UI/package.ps1 -OutputDirectory ./build/release-native
```

生成安装 EXE 还需要 NSIS 3：

```powershell
./windows_UI/package-installer.ps1 `
  -PackageDirectory ./build/release-native/SpektraLab-Qt-Windows-x64 `
  -OutputFile ./build/release-native/SpektraLab-Windows-setup.exe
```

输出目录必须是新目录。打包核对源码指纹、EXE 和材料配置，拒绝把陈旧程序重新标成新包。`build/`、SDK、测试 RAW、编辑记录和日志不提交到仓库；二进制放在 Releases。

自动检查不能替代实机验收。Qt/RAW 界面测试需要自己的 RAW 副本和独立输出目录；没有 Mac 对照输出时，不把成像一致性标为通过。历史 macOS / Tauri 工作流保留为手动运行，避免本 Windows 分支推送时自动构建旧前端。

## 观片台

参考官网的[观片台](https://spektralab.app/zh/lighttable/)，现在通过工具栏「观片台」或「查看 → 观片台」打开独立 Qt 窗口，无需浏览器。

1. 在编辑器完成显影后，点击「加入当前显影结果」，保留当前画面的独立快照，后续调色不会改变它。带片边的画面沿用编辑器的实际画幅。
2. 或先选「导入画幅」，再添加／拖入 PNG、JPEG 或 BMP 导出图片；当前精简包未包含 TIFF／WebP 读取插件。支持 135、半格、645、6×6／7／8／9、XPan、6×12／17，共 10 种画幅。普通照片按片门内接，常见片条／片框按比例估算；没有画幅元数据的图片需要手动选对画幅。
3. 拖动照片摆放，拖空白处平移，滚轮围绕光标缩放；右下角拖动改变照片尺寸，右键可旋转 90°、恢复实际尺寸或移出。「铺开」按物理尺寸重新排版。
4. D 切换拖动，C 点击拿起／放下，L 切换 3× 放大镜，0 适合窗口，Delete 移出所选照片。保存／复制的是整张观片台 PNG，不覆盖已有文件。

位置、旋转和缓存副本保存在当前用户应用数据目录的 `Lighttable/`，重启后恢复；最多 100 张，缓存长边上限 4096 像素。清空只删除观片台副本，保留源图片。不同画幅的毫米比例可比较，屏幕显示不代表实物 1:1。

这是独立实现的二维原生版本，尚未复刻官网 Three.js 的玻璃折射、景深与光学材质，也没有浏览器分享面板；可用保存／复制 PNG 分享。观片台使用编辑器现有 SDR 输出，沿用上述色彩限制。

## 来源与许可证

- 上游应用：[JamesQiu2005/SpektraLab](https://github.com/JamesQiu2005/SpektraLab)。保留原提交历史；原说明见 [English](README.upstream.md) / [简体中文](README.upstream.zh-CN.md)。
- 胶片／相纸物理模型、基础材料数据和打印 LUT：[Andrea Volpato / spektrafilm](https://github.com/andreavolpato/spektrafilm)。新增黑白材料来自上游 SpektraLab。
- 应用与引擎：[GPL-3.0-or-later](license/LICENSE)。材料配置和派生 LUT 保留 [CC BY-SA 4.0](license/SPEKTRAFILM_LICENSE.txt) 声明。
- Qt、MinGW、LibRaw 等依赖保留各自许可，随 `licenses/` 一同提供，见 [第三方说明](windows_UI/THIRD_PARTY.md)。

## English

This independent Windows port uses Qt Quick, C++, Vulkan and LibRaw. It runs without WebView2 or a local web server. Download the installer or complete portable ZIP from Releases. Existing film/paper data is retained, but full image parity with the latest macOS version is not established. Upstream attribution, GPL source licensing and CC BY-SA material notices are preserved.
