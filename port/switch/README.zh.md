# Nintendo Switch 上的 River Raid（homebrew）

[English](README.md) | [Русский](README.ru.md) | [Українська](README.uk.md) | [Deutsch](README.de.md) | [Polski](README.pl.md) | [Italiano](README.it.md) | [Español](README.es.md) | 中文 | [日本語](README.ja.md)

游戏（不含查看器）作为 homebrew 应用：一个 `.nro`，用于能通过 Homebrew Launcher 运行 homebrew 的 Switch。用 devkitPro 及其 SDL2 构建（SDL 本身不支持 Switch）。能够构建；尚未在主机上试过。

## 需要什么
- 带 Switch 软件包 `switch-dev` 和 `switch-sdl2` 的 devkitPro（用 `dkp-pacman` 安装），或其 Docker 镜像 `devkitpro/devkita64`。
- 你自己转储的卡带，放在 `rom/River_Raid.rom`（8192 字节；其他路径：`-DRR_SWITCH_ROM=...`）。它会被放**进** `.nro`（其 RomFS），所以这个 `.nro` 只属于你：不要分享。

## 构建
在仓库根目录下：
```
cmake -S port -B build-switch -DCMAKE_TOOLCHAIN_FILE=$DEVKITPRO/cmake/Switch.cmake -DCMAKE_BUILD_TYPE=Release
cmake --build build-switch
```
结果是 `build-switch/river_raid.nro`。没有 ROM 时也能构建，那时它会在 SD 卡上找 ROM（见下）。CI（`.github/workflows/switch.yml`）在每次改动 `port/` 的推送时构建它，并（不含 ROM）保存为运行的产物。

## 在主机上
把 `river_raid.nro` 复制到 SD 卡，例如 `/switch/river_raid/`，然后从 Homebrew Launcher 启动 River Raid (port)。不含 ROM 构建的 `.nro` 从它旁边的 `rom` 文件夹读取 ROM：
```
/switch/river_raid/river_raid.nro
/switch/river_raid/rom/River_Raid.rom
```
没有 ROM 时游戏会说明并等待按 +。

操作同 PC 上的手柄（见 [../README.zh.md](../README.zh.md)）：左摇杆或十字键转向，A、B、Y 或 ZR 开火，X 暂停，R 保存状态，L 回到该状态，ZL 切换缩放；− 是 SELECT，+ 是 START，两个同时按切换 NTSC 和 PAL。用 HOME 键退出游戏（关闭软件）。Switch 的屏幕和电视输出都只有 60 Hz，所以 PAL 机器按它自己的每秒 49.86 帧运行，不与屏幕同步。
