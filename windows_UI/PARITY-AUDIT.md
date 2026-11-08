# 成像一致性核对 · 2026-11-08

结论：当前 Qt Windows 版本不能声明与 GitHub 最新 macOS 版本成像一致。
Windows 自身的预览/导出一致，不能证明 Windows/macOS 一致。

本次只读取远端与 Git 对象，没有切换分支、覆盖工作区或合并更新。

| 核对对象 | 结果与证据 |
| --- | --- |
| 本地 Windows 根基 | `99c2d5c9883be485085895157279cc7c4eaa8db6`，工作区另有未提交的移植改动 |
| 此前采用的 Mac 对照 | `c52657409ad8d4c1bcd2e581879891ae32e8352f`（1.3.3） |
| 本次读取的 GitHub main | `2ad7f8ed0339e77cb2a440dab1e639da4c638c9f`；这是固定提交的核对，不能代表以后的 main |
| 原有材料配置 | 本地 33 个胶片/相纸 profile 与此远端提交的对应文件没有 Git 内容差异。基础 `engine/resources` 目录也没有差异；没有改测量曲线来调外观 |
| 最新材料目录 | 远端已有 34 个 profile，比本地多 `cinestill_800t.json`。该文件使用 `derived_from`，还需要对应的新 profile 加载器及物理模型，单独复制 JSON 不能完成移植 |
| 核心数学 | 本地 `engine/src/core` 对 1.3.3 对照没有 Git 内容差异；这是源码核对，不能替代运行结果对照 |
| 最新光晕模型 | 远端新增背反射光晕模型、`halation_headroom_ev` 及对应 Metal 实现；当前 Vulkan 版没有完整移植这些变化 |
| RAW 输入 | Mac 使用 Core Image；Windows 使用 LibRaw。兼容模式输出为 AHD、16 位中间结果再转线性 ProPhoto；高光处理、去马赛克、白平衡可能不同，不能保证同 RAW 解码一致 |
| 计算后端 | Mac 为 Metal，Windows 为 Vulkan。已有数学/内核测试只验证其各自指定的样例及容差，没有同一 RAW、同参数的 Mac/Windows 全流程像素对照 |
| 输出及显影后 | 原版以 ProPhoto 作为工作空间并经 ColorSync 显示；当前 Windows 引擎输出边界为 sRGB，非中性显影后暂转为 ProPhoto 计算再返回 sRGB，不能挽回已压缩色域；显示器 ICC/HDR 也未对齐 |

可重做的 Git 核对：

```powershell
git ls-remote origin HEAD refs/heads/main
git diff --name-only 2ad7f8ed0339e77cb2a440dab1e639da4c638c9f -- engine/resources engine/resources_product
git diff --name-only c52657409ad8d4c1bcd2e581879891ae32e8352f -- engine/src/core
git diff --stat c52657409ad8d4c1bcd2e581879891ae32e8352f 2ad7f8ed0339e77cb2a440dab1e639da4c638c9f -- engine/src modern_UI/Spektrafilm/Spektrafilm/Import/ImageDecoder.swift
```

后续达到成像验收所需的证据：固定源码版本和材料配置，先对相同线性输入、相同参数比较 Metal/Vulkan 结果，再对同一 RAW 比较解码与整条色彩链；将颗粒随机性、输出色域及显影后调整分别控制。当前没有 Mac 参考输出，不能把这些项目标为已通过。

上游：[固定提交](https://github.com/JamesQiu2005/SpektraLab/tree/2ad7f8ed0339e77cb2a440dab1e639da4c638c9f)、[色彩链说明](../ARCHITECTURE.md#75-the-colour-chain-end-to-end)。
