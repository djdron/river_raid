# River Raid (Atari 8-bit) → C++ 移植：项目状态

[English](HANDOFF.md) | [Русский](HANDOFF.ru.md) | [Українська](HANDOFF.uk.md) | [Deutsch](HANDOFF.de.md) | [Polski](HANDOFF.pl.md) | [Italiano](HANDOFF.it.md) | [Español](HANDOFF.es.md) | 中文 | [日本語](HANDOFF.ja.md)

这个文件用于在任何电脑上或在新的对话中接着工作。开始新会话时先说“阅读 HANDOFF.zh.md 和 docs/NOTES.zh.md”。

## 目标
把 River Raid（Activision，Atari 800，1983，作者 Carol Shaw）移植到 C++，运行于 Windows、Linux 和 macOS：
- 游戏逻辑与原版完全相同，不懂 6502 的普通程序员也能读懂；
- 声音完全相同：同样的声音驱动加一个 POKEY 核心；
- 跨平台层：SDL3（SDL2 作为选项，用于 Xbox 上的 UWP）；
- 不分发任何原版资源；移植版在运行时读取用户的 ROM（已决定：代码中没有 ROM 数据）。

## 状态
游戏已完成、可以玩：`port/game/river_raid.cpp`（演示模式、游戏 1–8、单人和双人、飞机、燃料、敌人、坦克、桥、分数、生命、最高分、暂停、声音）。已在 Windows、macOS（Apple M2）和 Linux 上构建并试玩。在机器人、双人、空闲和长时间运行中，逐帧与原版的 RAM 对照过；声音驱动写入 POKEY 的值相同。命令行和按键：`port/README.zh.md` 的“游戏”。游戏如何运作，写给所有人：`docs/HOW_IT_WORKS.zh.md`。

## 需要什么
- 整个项目文件夹。
- 你自己的 ROM `River_Raid.rom`：8192 字节，无文件头，加载到 $A000–$BFFF。仓库中没有它；请放到 `rom/River_Raid.rom`。

## 结构
| 路径 | 内容 |
|---|---|
| `port/` | C++ 移植：游戏、河流查看器、比对工具；见 `port/README.zh.md`（构建、选项、按键、内容、验证） |
| `docs/HOW_IT_WORKS.zh.md` | 游戏如何运作，写给所有人：河、敌人、桥、碰撞、为什么越来越难 |
| `docs/NOTES.zh.md` | 带地址的发现 |
| `disasm/river_raid_a8.asm` | 带文件头的完整反汇编（入口点、帧、变量） |
| `tools/dis6502.py` | 反汇编器：`python dis6502.py <rom> <out.asm> B500 B2A5 B485` |
| `tools/check_lfsr.py`、`tools/verify_trace.py` | 用参考记录检验文档中的规则 |
| `tools/level_map.py` | 整条河的图片，一段一段 |
| `tools/doc_figures.py` | `docs/HOW_IT_WORKS.zh.md` 中的图（`docs/img/`），来自 `port/tools/river_dump` |
| `oracle/` | 基于 libatari800 的参考运行：`rr_oracle.c`、`build.sh`、`README.md`、输入脚本、自动驾驶 `-bot`、无敌飞机 `-ghost` |
| `docs/oracle_demo.png` | 一次参考运行的帧 |

## 已确立的事实（全部经过验证）
1. **处理程序。**主循环 A000–AF5A，执行一遍 = 一帧（按 VCOUNT $50/$78 同步）。VBI $B2A5 = POKEY 声音驱动和计数器。DLI $B500 = 逐行的精灵复用和碰撞读取。IRQ $B485 = 空桩。
2. **河流生成器。**软件实现的 24 位 LFSR（`sub_B183`，$AA/$AB/$AC），每行走一步；一切（河岸、岛、装饰、物体）都来自它和关卡。它既不依赖玩家，也不依赖 RANDOM。河在大约 17 000 座桥之内不会重复（从游戏 1 起点算起 LFSR 周期为 16 766 977 步）。C++（`port/src/river_gen.cpp`）逐字节一致：119 758 行，以及从零开始的起点。
3. **起点。**游戏 1–8 从同一条河的第 1/5/20/50 座桥开始：表 $B494–$B4A8 保存那里的 LFSR、河岸、关卡和桥计数；从游戏 1 起点运行生成器，恰好到达这些值。移植版通过运行生成器来填同一张表，用于 `-seed N`（另一条河）和 `-bridge N`（从任意桥开始）。
4. **难度。**关卡（= 你正飞向的桥，到 80 为止，之后 79/80 交替）移动与随机数比较的阈值：块中间的敌人、加油站减少到第 46 关（PAL 上为 47）、热气球从 3 关起、喷气战机从 5 关起、岸上坦克从 7 关起、导弹直升机从 13 关起、弯道更大。表格见 `docs/HOW_IT_WORKS.zh.md`。
5. **RANDOM（$D20A）。**游戏逻辑只在 A205 读取它：每 16 帧随机唤醒一个槽位（0–10）（`$42,X` 的位 $40）。B362/B3D0：声音用的噪声。其值取决于读取时的 CPU 周期；移植版模拟了这个计数器（`src/pokey_random.h`），像真机开机一样从时钟启动；`-random N` 可重复一次运行。
6. **碰撞**来自 GTIA 的碰撞寄存器，DLI 在每个物体的带之后读取；在读取那一行上的接触是否算数取决于周期。移植版按周期计数运行 DLI（`src/display.cpp`，即 atari800 的非周期精确时序）。
7. **PAL/NTSC。**两者共用一个卡带：PAL 位（$D014 → $BF）选择显示列表的变体（$B77A）和加油站界限（$CB = $2E/$2F）。别的什么都不变，所以在 PAL 上整个游戏以每秒 50 帧运行，大约慢 17%。英国的磁带版（`.cas`，2 个文件：加载器在 $0A00，游戏在 $1000–$31FF）是同样的代码搬到了 RAM 中，有不同的防拷贝措施，滚动字幕中是“1982, 1983, 1984”。
8. **字体**从操作系统 ROM 复制（$E000/$E100/$E300）；移植版内置 AltirraOS 的字体，`-os` 加载 Atari 的字体。
9. **参考运行。**atari800 从时钟启动 POKEY；`rr_oracle -seed`（默认 0）使运行逐字节相同。atari800 的构建在 `-Werror` 下因 ULONG 重定义而失败；`build.sh` 用 `CFLAGS="-g -O2 -DNETSIO"` 构建。测试过的 atari800 提交：98cc2dc。

## 环境限制
- `oracle/build.sh` 可在 Linux、macOS、WSL 和 MSYS2 上构建（被拦截的模拟器函数在编译时重命名，不用 GNU 的 `--wrap`）；C++ 移植用普通的 CMake 构建（MSVC、Xcode、gcc）。
- ROM 不在仓库中（`.gitignore`）；把你的转储放到 `rom/River_Raid.rom`。
- 代码用制表符缩进，续行也一样（`.editorconfig`）。文档：`X.md` 为英文，`X.ru.md` 为俄文，另有 `X.uk.md`、`X.de.md`、`X.pl.md`、`X.it.md`、`X.es.md`、`X.zh.md`、`X.ja.md`；`tools/doc_languages.py` 在每个标题下写入语言行，图片的文字在 `tools/doc_figures_i18n.py` 中。修改一个文档时要改所有语言。

## 方法
规则先从反汇编中读出，再用参考记录检验（`tools/verify_trace.py`，然后是 C++ 比对工具）。记录本身只用来找该看哪里。不应改变行为的修改（重命名、重构）用 `port/tools/equivalence` 与旧提交对比：要求 0 差异，并通过 `port/README.zh.md`“验证”中的所有检查。

## 决定
- 移植用 C++ 编写，而不是包装一个模拟器。
- 验证逐帧比较移植版的 RAM 与参考的 `ram.bin`；所用的 RANDOM 值来自参考的 `random.log`。
- 代码中没有 ROM 数据：卡带的表通过具名访问函数读取（`port/src/rom.h`）。
- 默认颜色：PAL，Altirra 的模型；NTSC 和 atari800 的模型用按键切换。
- 窗口显示游戏画面（364×200，含完整的滚动字幕），而不是带黑边的整帧（`-border`、O）；先清晰地整数倍放大，再平滑缩放（`-filter`、L）。
- 游戏结束时的周期竞争（帧计数器在该帧 VBI 之前还是之后被重置，取决于主循环的周期，见 `port/README.zh.md` 的“验证”）有意不重现。
- 在 60/120 Hz 的屏幕上，游戏锁定到屏幕（每秒 60.00 帧而不是 59.92），声音相应重新采样。

## 未决问题
- 开机本身（A000–A17C，前 38 帧）画成空屏幕；`Game::powerOn` 不精确。
- PAL：显示列表和 50 Hz 时序没有重现（`-pal` 只改变加油站界限和颜色）。

## 下一步
1. 可选：PAL 时序（50 Hz、PAL 显示列表），给那些在欧洲电视上玩过这款游戏的人。
2. 可选：保存和读取状态（全部状态都在 `GameState` + `RiverState` 中，是简单数据）。
3. 可选：基于 SDL2 的 UWP/Xbox 版本（`-DRR_SDL2=ON`）。
