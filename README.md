qimgv（个人修改版）
=====

> 本项目 fork 自 [easymodo/qimgv](https://github.com/easymodo/qimgv)，遵循原项目的 GPLv3 许可证。
> 原版功能全部保留，下述内容为本分支在此基础上所做的修改。

## 本分支修改了什么

| 改动 | 说明 |
| --- | --- |
| **底部悬浮导航条** | 窗口底部一条可自定义的图标栏，每个按钮都是某个快捷键的别名，点击即执行对应动作。 |
| **导航条设置页** | 首选项新增「导航条」页：自动隐藏开关、按钮数量（1–16）、逐按钮的快捷键录制与自定义图标|
| **完整简体中文翻译** | 补齐原版缺失的全部条目（首选项各页、右键菜单、脚本编辑器、侧栏、关于页）|

## 截图

主窗口与面板        |  文件夹视图   |  设置窗口
:-------------------------:|:-------------------------:|:-------------------------:|
[![img1](qimgv/distrib/screenshots/qimgv0.9_1_thumb.jpg)](qimgv/distrib/screenshots/qimgv0.9_1.jpg?raw=true)  |  [![img2](qimgv/distrib/screenshots/qimgv0.9_2_thumb.jpg)](qimgv/distrib/screenshots/qimgv0.9_2.jpg?raw=true) | [![img3](qimgv/distrib/screenshots/qimgv_3_thumb.jpg)](qimgv/distrib/screenshots/qimgv_3.jpg?raw=true)

## 主要特性

- 简洁的界面
- 速度快
- 易于使用
- 高度可配置，包括主题与快捷键
- 高质量缩放
- 基础图片编辑：裁剪、旋转、调整尺寸
- 快速把图片复制 / 移动到其他文件夹
- 通过 libmpv 播放视频（实验性）
- 文件夹视图模式
- 支持运行 shell 脚本

## 默认操作

| 操作 | 快捷键 |
| ------------- | ------------- |
| 下一张图片 | 右方向键 / 鼠标滚轮 |
| 上一张图片 | 左方向键 / 鼠标滚轮 |
| 跳到第一张 | Home |
| 跳到最后一张 | End |
| 放大 | Ctrl+滚轮 / Ctrl+上 |
| 缩小 | Ctrl+滚轮 / Ctrl+下 |
| 缩放（另一种方式） | 按住鼠标右键上下移动 |
| 适应模式：适应窗口 | 1 |
| 适应模式：适应宽度 | 2 |
| 适应模式：1:1（不缩放） | 3 |
| 切换适应模式 | 空格 |
| 切换全屏 | 双击 / F / F11 |
| 退出全屏 | Esc |
| 显示 EXIF 面板 | I |
| 裁剪图片 | X |
| 调整图片尺寸 | R |
| 向左旋转 | Ctrl+L |
| 向右旋转 | Ctrl+R |
| 打开所在目录 | Ctrl+D |
| 幻灯片模式 | ~ |
| 随机播放模式 | Ctrl+~ |
| 快速复制 | C |
| 快速移动 | M |
| 移到回收站 | Delete |
| 删除文件 | Shift+Delete |
| 保存 | Ctrl+S |
| 另存为 | Ctrl+Shift+S |
| 文件夹视图 | Enter / Backspace |
| 打开 | Ctrl+O |
| 打印 / 导出 PDF | Ctrl+P |
| 设置 | P |
| 退出程序 | Esc / Ctrl+Q / Alt+X / 中键单击 |

……以及更多。

提示：所有快捷键都可以在 __设置 > 控制__ 中自行配置。

# 用户界面

设计目标是界面整洁、简单易用：只有在你需要的时候才会看到对应的界面元素。

底部有一个可拉出的缩略图面板，另外还有文件夹视图。右键也可以唤出上下文菜单。

## 使用快速复制 / 快速移动面板

用 `C`（复制）或 `M`（移动）唤出面板。面板里有 9 个目标目录，点击文件夹图标可以修改。

面板显示时，按 `1`–`9` 即可把当前图片复制 / 移动到对应目录。

操作完成后再次按 `C` 或 `M` 隐藏面板。

## 运行脚本

你可以对当前图片运行自定义脚本。

打开 __设置 > 脚本__，点击「添加」。这里可以选择「shell 命令」或「shell 脚本文件」。

命令示例：

`convert %file% %file%_.pdf`

脚本文件示例（`$1` 为图片路径）：

```
#!/bin/bash
gimp "$1"
```

_注意：脚本文件必须具有可执行权限，且必须包含 shebang（`#!/bin/bash`）。_

创建好脚本后，进入 __设置 > 控制 > 添加__，选中它并像普通动作一样分配快捷键。

## HiDPI（仅 Linux / macOS）

如果 qimgv 在你的显示器上过大 / 过小，可以覆盖缩放系数，例如：

```
QT_SCALE_FACTOR="1.5" qimgv /path/to/image.png
```

把它写进 `qimgv.desktop` 可以永久生效。不支持小于 `1.0` 的值。

qimgv 也会遵循 KDE 系统设置里的全局缩放系数。

## 高质量缩放

如果在编译时启用了 `opencv` 支持，qimgv 可以使用更好的缩放滤镜（默认开启，但不同 Linux 发行版可能不同）。滤镜选项位于 __设置 > 缩放__，推荐 `Bicubic` 或 `bilinear+sharpen`。

# 额外图片格式

qimgv 可以通过第三方图片插件打开一些额外格式，Windows 安装包内均已包含。

| 格式 | 插件 |
| ------- | ------------- |
| JPEG-XL | [github.com/novomesk/qt-jpegxl-image-plugin](https://github.com/novomesk/qt-jpegxl-image-plugin) |
| AVIF | [github.com/novomesk/qt-avif-image-plugin](https://github.com/novomesk/qt-avif-image-plugin) |
| APNG | [github.com/Skycoder42/QtApng](https://github.com/Skycoder42/QtApng) |
| HEIF / HEIC | [github.com/jakar/qt-heif-image-plugin](https://github.com/jakar/qt-heif-image-plugin) |
| RAW | [https://gitlab.com/mardy/qtraw](https://gitlab.com/mardy/qtraw) |

# 安装

本分支目前没有发布编译好的安装包，请参考上文的「本分支的构建提示」自行编译，或使用上游的发行版：

## 从源码编译

参见上游 wiki：[Compiling qimgv from source](https://github.com/easymodo/qimgv/wiki/Compiling-qimgv-from-source)。Windows 下的补充说明见上文「本分支的构建提示」。

# 许可证与致谢

- 原项目：[easymodo/qimgv](https://github.com/easymodo/qimgv)，作者 easymodo 及所有贡献者
- 许可证：GNU GPLv3
