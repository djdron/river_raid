# rr_oracle — River Raid 参考运行

[English](README.md) | [Русский](README.ru.md) | [Українська](README.uk.md) | [Deutsch](README.de.md) | [Polski](README.pl.md) | [Italiano](README.it.md) | [Español](README.es.md) | 中文 | [日本語](README.ja.md)

在 libatari800 上无窗口地运行原版卡带（Atari 800，默认 NTSC，内置 AltirraOS，关闭 BASIC）。用于把 C++ 移植与原版逐帧比较。

## 构建
```
./build.sh
```
脚本会克隆 atari800（测试过的提交 98cc2dc），构建 `libatari800.a` 并链接 `rr_oracle`。需要 git、C 编译器（gcc 或 clang）、make、autoconf 和 automake；可在 Linux、macOS（Xcode 命令行工具，`brew install autoconf automake`）、WSL 和 MSYS2 上运行。参考运行拦截模拟器的三个函数（写入 POKEY、读取 RANDOM、垂直消隐 NMI）：脚本把 `pokey.c` 和 `cpu.c` 再编译一次，并把这些函数改名，所以不需要任何链接器技巧。

## 运行
```
./rr_oracle <rom> <script> <frames> <outdir> [-pal] [-seed N] [-bot|-bot-nofire] [-ghost] [-noram] [-ram A:B] [-strip Y] [-screens A:B] [-shot N,N,...]
./rr_oracle ../rom/River_Raid.rom scripts/start_and_fly.txt 1000 out -shot 240,500
./rr_oracle ../rom/River_Raid.rom scripts/start_only.txt 8000 out -bot      # bot
./rr_oracle ../rom/River_Raid.rom scripts/bot_run_8000.txt 8000 out         # replay of a bot run
```

## 机器人（`-bot`）
先由脚本操作（例如 `start_only.txt`：开机、START、等待），然后自动驾驶接手。它朝前方水面的中央驾驶，躲避附近的物体，一直开火，并在爆炸或在桥边等待时按“上”。机器人很简单，只用于生成长记录，不是为了高分：它不找燃料，而且会死于从侧面来的直升机。

`-bot-nofire`：同样的自动驾驶，但不开火也不躲避。

任何一次运行中实际施加的输入都会以脚本格式写入 `inputs.txt`。不带 `-bot` 把它重新输入，会得到逐字节相同的记录（在 8000 帧上检查过）。

## 拍摄关卡的选项
- `-ghost` — 无敌飞机（在模拟内存中打 5 字节的代码补丁，仅用于拍摄）。
- `-strip Y` — 每一帧的屏幕第 Y 行写入 `strip.bin`，调色板写入 `palette.bin`。
- `-screens A:B` — 第 A…B 帧的完整屏幕写入 `screens.bin`。
- `-noram` — 不写 `ram.bin`（用于长时间运行）。
- `-ram A:B` — 只为第 A…B 帧写 `ram.bin`（第 A 帧位于偏移 0）。
- `-wav` — 模拟器的声音输出写入 `sound.wav`。
- `-vbi` — `vbi.bin`：每次垂直消隐中断开始时的 RAM（每条记录为 4 字节帧号 + 16 384 字节），即声音驱动看到的状态。
- `frames.csv` 的列为 `$60, $2F, $5D, $AA, $AB, $AC, $76, $52, $53, $54, $2D, $4E`。

## 脚本
| 文件 | 内容 |
|---|---|
| `start_and_fly.txt` | START、加速、转弯、开火（1000 帧） |
| `attract_fire_only.txt` | 在标题画面开火（游戏不会开始） |
| `start_only.txt` | 开机、START、等待；用于 `-bot` |
| `probe_lr.txt`、`probe_hold.txt` | 用于寻找变量的短按 / 长按 |
| `bot_run_8000.txt` | 录下的机器人运行：2 座桥、5 次坠毁、游戏结束、重新开始 |
| `refuel_first_depot.txt` | 不开火飞过第一个加油站（加油） |
| `attract_idle.txt` | 开机且无输入：演示模式（游戏 1 的河、滚动的制作人员名单） |
| `two_players.txt` | SELECT 到游戏 2，START；两名玩家一直向前飞并开火，直到失去所有飞机；按两次暂停键 |
| `ghost_film.txt` | 开始并无输入地飞行；配合 `-ghost -noram -strip 20` 拍摄关卡地图 |

## 输入脚本
每行：`<帧数> <摇杆> <开火> [start] [select] [option] [摇杆2] [开火2] [按键]`；`#` 开始注释。最后一行会一直重复到运行结束。`按键` 按住空格键（任意键都会暂停 River Raid）。
摇杆的位：1 上，2 下，4 左，8 右，0 居中。
游戏用 START 键开始；在标题画面开火不会开始游戏。

## `<outdir>` 中有什么
| 文件 | 内容 |
|---|---|
| `ram.bin` | 每帧之后的 $0000–$3FFF（16 KB）；第 N 帧在偏移 N*16384 |
| `pokey.log` | 每次写入 POKEY：`帧 扫描线 周期 寄存器 值` |
| `random.log` | 每次读取 RANDOM：`帧 扫描线 周期 值`。约第 249 条扫描线是声音（VBI），其他扫描线是游戏逻辑（A205） |
| `frames.csv` | 施加的输入（摇杆、开火、START，然后在 CPU 列之后是 SELECT、OPTION、摇杆 2、开火 2、按键）以及帧末的 CPU 寄存器 |
| `inputs.txt` | 脚本格式的施加输入（用于重放） |
| `fNNNNN.ppm` | 384×240 的截图 |

## 确定性
atari800 本身从系统时钟启动 POKEY 生成器，所以运行无法重复。`rr_oracle` 明确设置计数器（`-seed`，默认 0）。使用相同的 ROM、脚本和种子，所有文件都逐字节相同（已检查）。

许可：libatari800 是 GPL-2.0+，所以作为与之链接的程序，`rr_oracle` 也是。它是开发工具：C++ 移植不与它链接，只读取它的输出文件。
