# Xbox 上的 River Raid（UWP）

[English](README.md) | [Русский](README.ru.md) | [Українська](README.uk.md) | [Deutsch](README.de.md) | [Polski](README.pl.md) | [Italiano](README.it.md) | [Español](README.es.md) | 中文 | [日本語](README.ja.md)

游戏（不含查看器）作为 UWP 应用：可在 Windows 10/11 以及处于 Dev Mode 的 Xbox One 或 Series 上运行。它基于 SDL2 构建，因为 SDL3 已不再支持 UWP。**尚未试过**：编写时手边没有 Windows；如果某一步与描述不符，请告知发生了什么。

## 需要什么
- Windows 10/11 和 Visual Studio 2022，带“使用 C++ 的桌面开发”和“通用 Windows 平台开发”工作负载（含 C++ UWP 工具，v143）。
- CMake 3.16 或更新版本（Visual Studio 自带的即可）和 git：SDL2 会随游戏一起下载并构建。
- 对于 Xbox：开发者帐户，以及处于 Dev Mode 的主机（Xbox 上的“Dev Mode Activation”应用）。
- 你自己的卡带转储，命名为 `rom/River_Raid.rom`（8192 字节）：它会被放**进安装包**，所以安装包只供你自己使用；请勿分发。

## 构建
```
cmake -S port -B build-uwp -G "Visual Studio 17 2022" -A x64 -DCMAKE_SYSTEM_NAME=WindowsStore -DCMAKE_SYSTEM_VERSION=10.0
```
打开 `build-uwp/river_raid_port.sln`，选择 Release 和 x64，并把 `river_raid` 设为启动项目。先在本地计算机（Windows）上运行：窗口里应出现游戏。

安装包用 CMake 自带的测试证书签名（`CN=CMake Test Cert`）。使用你自己的证书：`-DRR_UWP_CERTIFICATE=my.pfx -DRR_UWP_PUBLISHER="CN=..."`（证书的使用者）。换一个 ROM 路径：`-DRR_UWP_ROM=...`。

## 部署到 Xbox
两种方式都可以：
- **从 Visual Studio：**调试目标选“远程计算机”，填 Xbox 的 IP 地址（Dev Home 中显示），身份验证选“Universal (Unencrypted Protocol)”；用 Dev Home 显示的 PIN 配对。
- **通过 Device Portal 安装包：**在 Visual Studio 中 项目 → 发布 → 创建应用程序包 → 旁加载，x64；然后在 PC 的浏览器中打开 `https://<xbox ip>:11443`（Device Portal，在 Dev Home 中开启），Add，选择 `.msix` 以及其文件夹中的依赖项（VCLibs 包）。

然后在 Dev Home 中选中该应用，View details，把 App type 设为 **Game**：否则应用得到的主机资源更少。

## Xbox 上的操作
| 手柄 | |
|---|---|
| 方向键或左摇杆 | 摇杆 |
| A、B、Y 或右扳机 | 开火 |
| X | 暂停键（Xbox 没有键盘） |
| Menu | START |
| View | SELECT |
| LB / RB | 保存状态 / 回到该状态 |

颜色、调色板、边框和缩放均为默认（PAL 颜色、Altirra、只显示游戏画面、清晰）。B 同时也是系统的返回键；游戏会接管它，所以它不会关闭应用。Xbox 键退出游戏。

## 可能还需要做的
- 电视的安全区域：Xbox 上的 UWP 应用可能会与屏幕边缘留出边距绘制。画面本来就被黑色包围；如果边距碍事，应用可以请求整个屏幕（`ApplicationView::SetDesiredBoundsMode`）。
- 文件的组织方式（`uwp/uwp.cmake`、`uwp/Package.appxmanifest.in`）：CMake 对 UWP 项目的支持和 SDL2 的 WinRT 入口点（`SDL_winrt_main_NonXAML.cpp`，按 C++/CX 编译）都按文档使用，但在这里没有构建过。
