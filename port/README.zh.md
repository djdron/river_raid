# River Raid 的 C++ 移植

[English](README.md) | [Русский](README.ru.md) | [Українська](README.uk.md) | [Deutsch](README.de.md) | [Polski](README.pl.md) | [Italiano](README.it.md) | [Español](README.es.md) | 中文 | [日本語](README.ja.md)

一个出于学习目的的非官方爱好者项目，与 Activision 和 Microsoft 无关；不包含游戏的任何部分（见 `../README.zh.md` 中的声明）。

完整的游戏：`river_raid` 像原版一样运行 River Raid——演示模式、选择游戏、单人和双人、飞机、燃料、敌人、桥、分数、生命、最高分、暂停和声音。河流生成器和游戏逻辑逐帧产生与原版相同的 RAM 状态，绘制出相同的屏幕（见“验证”）。

## 构建
```
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```
核心和工具只需要 C++17 编译器。游戏 `river_raid` 和查看器 `river_view` 需要 SDL3：如果 CMake 找到已安装的 SDL3（3.4 或更新），就使用它，否则在配置时从 GitHub 下载 SDL 3.4.20（需要 git 和网络）并静态链接。在 Linux 上从源码构建 SDL 需要 X11/Wayland 开发包（见 SDL 的 `docs/README-linux.md`）。缺少这类包时 SDL 3.4 会报错停止，并给出不带它构建的选项（例如 `-DSDL_X11_XCURSOR=OFF`）。完全不用 SDL：`cmake -S . -B build -DRR_VIEWER=OFF`。

用 SDL2 代替 SDL3（用于 SDL3 不再支持的平台，例如 Xbox 上的 UWP）：`cmake -S . -B build -DRR_SDL2=ON`。这时 CMake 使用已安装的 SDL2（2.0.18 或更新）或下载 SDL 2.32.10。程序只通过 `sdl/host.h` 与 SDL 打交道，它为每个版本各实现一次；两者给出相同的画面。

在 Windows 上用 Visual Studio：`cmake --build build --config Release`，程序位于 `build\Release\`。已在 Windows（MSVC）、macOS（Xcode，Apple M2）和 Linux（gcc）上构建并试玩。

## 游戏
```
river_raid [rom] [选项]
```
不带参数时，它会自己找到 ROM（见 `rom`），并像卡带一样启动：先演示模式，然后按 START。

| 选项 | 作用 |
|---|---|
| `rom` | 卡带映像（8192 字节）。默认在当前文件夹及其上至三层文件夹中寻找 `rom/River_Raid.rom`，所以从仓库或 `port/build/Release` 不带参数即可启动 |
| `-os 文件` | 来自 Atari 自己操作系统的字母和数字：一个 OS ROM 映像（10 KB 或 16 KB）或一个 1 KB 的字体。不指定时使用内置的 AltirraOS 字体 |
| `-seed N` | 另一条河，有另外的敌人和加油站。N 是河流生成器开始时的状态，从 1 到 16777215。游戏 1 从这条河的起点开始，游戏 3–8 从它的第 5、20、50 座桥开始；每局保持通常的关卡，所以难度照常增长。卡带自己的河是 `-seed 16241891` |
| `-bridge N` | 每局（1–8）都从第 N 座桥开始，1 到 9999：桥计数器显示 N，关卡为该桥的关卡（80 之后关卡在 79/80 之间交替，与原版一样）。也可以和 `-seed` 一起用。起点通过让河流生成器一直运行到那里来求得（即使 9999 也不到一秒） |
| `-random N` | POKEY 的 RANDOM 计数器的初始值，它决定哪个敌人何时醒来以及爆炸的噪声。默认取自时钟，就像真机的值取决于开机的时刻，所以每次运行都不同；相同的 N 加上相同的输入，运行会完全重复 |
| `-pal` | 作为 PAL 机器运行：和真正的 PAL Atari 一样，加油站一直变少到第 47 关而不是第 46 关；PAL 颜色。显示时序仍为 NTSC（见“尚未完成”） |
| `-colours ntsc\|pal` | NTSC 或 PAL 机器的颜色；默认 PAL。游戏中按 C 切换 |
| `-palette altirra\|atari800` | 颜色模型：`altirra`（默认）或 `atari800`（中间色调更暗）。游戏中按 V 切换 |
| `-border` | 像模拟器那样显示带黑色边框的整帧（384×240，两侧各去掉 10 列）。默认窗口只显示游戏画面和完整的滚动字幕，364×200，因此全屏时画面大约大 20%，窗口则相应地矮一些。游戏中按 O 切换 |
| `-filter sharp\|nearest\|linear` | 画面如何缩放到窗口或屏幕：`sharp`（默认）先以整数倍、清晰的像素放大，再平滑缩放到最终尺寸，因此保持清晰且每个像素一样大；`nearest` 是简单的方块，但在非整数倍缩放时（横向总是如此，因为 Atari 的像素不是正方形）有些列会宽出一个屏幕像素；`linear` 平滑但模糊。游戏中按 L 切换 |
| `-stats` | 打印渲染器和屏幕刷新率，然后每秒打印帧率、每帧耗时和声音队列 |
| `-nosound` | 无声音 |

窗口标题显示当前使用的颜色和缩放方式。在 60 Hz（或 120 Hz……）的屏幕上，游戏正好以屏幕的刷新率运行（60 帧/秒，而不是 Atari 的 59.92），以便每帧在屏幕上停留同样长的时间；在其他屏幕上则以 Atari 的速率运行。

示例：`river_raid -bridge 30` 从第 30 座桥开始；`river_raid -seed 12345` 是另一条河；`river_raid -seed 12345 -bridge 80` 是这条河最难的部分。

开机后，演示模式在河上飞行。START 开始游戏；SELECT（或 OPTION）选择游戏 1–8：偶数编号是双人游戏（玩家轮流），游戏 3–4、5–6、7–8 从第 5、20、50 座桥开始。Atari 键盘上的任意键可以暂停。游戏结束后画面会停住，直到演示模式重新开始（约 35 秒）。游戏和查看器用常规方式退出：关闭窗口、Alt+F4、Cmd+Q。

| 按键 | Atari |
|---|---|
| 方向键或 W A S D | 摇杆：左/右转向，上加速，下减速；任意方向都会让飞机从桥边出发 |
| 左/右 Ctrl、Z、X | 开火 |
| F4 或 Enter | START |
| F3 | SELECT |
| F2 | OPTION |
| 空格 | 键盘上的一个键：暂停 |
| F、F11 或 Alt+Enter | 全屏 |
| C | NTSC / PAL 颜色 |
| V | 颜色模型 Altirra / atari800 |
| O | 只显示游戏画面 / 带边框的整帧 |
| L | 缩放：sharp / nearest / linear |
| F7 / F8 | 保存游戏状态 / 回到该状态（仅在内存中，退出即失效） |
| Esc | 退出全屏 |
| 鼠标点击 | 隐藏鼠标指针，直到鼠标移动（进入全屏也一样） |

键盘同时控制两个摇杆，所以一个人也能玩双人游戏。手柄：第一个是摇杆 1（在没有第二个时也是摇杆 2），第二个是摇杆 2；十字键或左摇杆，A/B/X/Y 或右扳机开火，Start = START，Back = SELECT，LB / RB = F7 / F8（保存状态 / 回到该状态）。

## 河流查看器
```
river_view [rom] [选项]
```
它显示原版的演示模式，即没人按 START 时播放的河上飞行：开机后屏幕被游戏 1 的河填满，然后每帧滚动 1 行；船和直升机醒来并移动，坦克开向桥并开火，直升机发射导弹（移植的游戏逻辑，`src/game.cpp`）；下面是状态栏和滚动字幕，有制作人员、“Press START key”和彩虹色的 Activision 标志。选项与游戏相同：`rom`、`-os`、`-seed`、`-bridge`（查看器先只用河道从河的起点飞到该桥，9999 需要几秒，然后按连续飞行时的样子显示它；可以一直往回看到起点）、`-random`、`-pal`、`-colours`、`-palette`、`-border`、`-filter`；以及它自己的 `-sound`：带声音的演示模式（见 M）。

字母和数字 1–9 来自操作系统的字体，卡带里没有。移植版内置了 AltirraOS 的字体：随 atari800 一起发布的免费替代操作系统（许可：保留版权声明即可复制和修改，见 `src/os_font.cpp`）；参考记录也是用它做的。`-os 文件` 从 OS ROM 映像（10 KB 或 16 KB）或 1 KB 字体文件中读取 Atari 自己操作系统的字体。

| 按键 | 动作 |
|---|---|
| 空格 | 暂停 |
| 上 / 下 | 更快 / 更慢，经过停止直到倒着飞（两个方向都从 x1/4 到 x16） |
| 鼠标滚轮、PgUp / PgDn | 手动沿河滚动，每次一屏 |
| Backspace | 回到最新的一行，以正常速度 |
| B / Shift+B | 下一座 / 上一座桥 |
| Alt+B / Alt+Shift+B | 前进 / 后退 10 座桥 |
| Ctrl+B / Ctrl+Shift+B | 前进 / 后退 100 座桥 |
| Home | 回到第 1 座桥，即河的起点 |
| F1 / F2 / F3 | 声音测试：通过游戏的声音驱动播放爆炸 / 射击 / 得分声（演示模式本身是无声的，和原版一样） |
| F | 全屏 |
| C | NTSC / PAL 颜色 |
| V | 颜色模型 Altirra / atari800 |
| O | 只显示游戏画面 / 带边框的整帧 |
| L | 缩放：sharp / nearest / linear |
| M | 声音开 / 关：坦克炮弹、直升机导弹和飞机引擎，与游戏中相同（原版没有：原版演示模式是无声的）；向前时任何速度都有声音，引擎随速度变化，就像飞机加速或减速；倒退时声音倒放，像倒带一样 |
| Esc | 退出全屏 |
| 鼠标点击 | 隐藏鼠标指针，直到鼠标移动（进入全屏也一样） |

与原版的屏幕相比（演示模式，第 1000–2300 帧，使用原版的 RANDOM 值）：河的区域和状态栏逐像素一致，滚动字幕也一致，只有最外侧的列除外（24 以左、359 以右），那里模拟器对边缘的精细滚动处理不同。

哪个物体何时醒来，由 POKEY 的 RANDOM 寄存器决定，它是一个硬件计数器，读取的时刻取决于整台机器的时序。移植版模拟了这个计数器（`src/pokey_random.h`），并从时钟启动它，就像真机在开机时启动它一样，因此每次运行都不同，和真机一样；`-random N` 可以重复一次运行。

## 内容
| 文件 | 内容 |
|---|---|
| `src/rom.h` | 卡带映像，以及移植版从中读取的每张表的具名访问函数（形状、颜色、分数、生成器表、状态栏文字……）；物体类型 |
| `src/river_gen.h/.cpp` | 生成器：`RiverGen::startGame(n)`，然后 `nextLine(line)`——每次调用一行屏幕 |
| `src/river_draw.h/.cpp` | 像屏幕上那样绘制一行：河岸色调、桥边的公路、物体的精灵（`drawScreenLine`） |
| `src/palette.h/.cpp` | NTSC 和 PAL 机器的 Atari 颜色（`atariRgb`）：Altirra 和 atari800 的模型 |
| `src/game.h/.cpp` | 游戏：原版的主循环和 VBI，逐帧——演示模式、控制台按键、飞机、燃料、碰撞、爆炸、分数、生命、玩家、坠毁与重开、游戏结束、最高分、声音驱动 |
| `src/display.cpp` | 由 DLI 驱动的河流显示：一个精灵复用于每个物体，按物体的碰撞，绘制河的区域 |
| `src/pokey_random.h` | POKEY 的 RANDOM 计数器 |
| `src/pokey_sound.h/.cpp` | POKEY 声音：四个声道、分频器和噪声计数器，逐周期生成 |
| `src/status_bar.h/.cpp` | 状态栏和滚动字幕：字符集、状态栏、滚动步进、绘制 |
| `src/os_font.h/.cpp` | 操作系统字体：内置 AltirraOS，或从 OS ROM 加载 |
| `src/river_state_io.cpp`、`src/game_ram.cpp` | 生成器和游戏的字段与原版 RAM 地址的对应：加载、写入和比较（用于验证） |
| `game/river_raid.cpp` | 游戏（SDL3）：画面、声音、键盘和手柄 |
| `viewer/river_view.cpp` | 河流查看器（SDL3），见上文 |
| `sdl/host.h`、`host_sdl3.cpp`、`host_sdl2.cpp` | 游戏和查看器的窗口、画面、声音、键盘和手柄，基于 SDL3 或 SDL2；隐藏鼠标指针 |
| `tools/river_map.cpp` | 生成河并保存为 PNG，样子和游戏中一样：`river_map <rom> <游戏 1..8> <行数> out.png [-pal] [-flat]`（`-flat`：只有位图，固定颜色） |
| `tools/river_dump.cpp` | 把生成的河以文本写出（每个点的颜色、放置的物体），供 `tools/doc_figures.py` 使用，后者绘制 `docs/HOW_IT_WORKS.zh.md` 中的图：`python tools/doc_figures.py rom/River_Raid.rom`（需要 matplotlib） |
| `tools/sound_check.cpp` | 把声音驱动与原版写入 POKEY 的值比较（`rr_oracle -vbi`） |
| `tools/sound_render.cpp` | 用移植版的 POKEY 把 POKEY 写入记录渲染成 WAV，以便与模拟器的声音比较（`rr_oracle -wav`） |
| `tools/game_check.cpp` | 用原版的输入和 RANDOM 值，把游戏与原版的 RAM 记录逐帧比较（并把河的区域与其屏幕比较） |
| `tools/status_check.cpp` | 把状态栏和滚动字幕与原版的完整屏幕比较（`rr_oracle -screens A:B -ram A:B`） |
| `tools/render_check.cpp` | 把绘制结果与原版屏幕比较（`oracle/rr_oracle -ghost -strip 20`） |
| `tools/terrain_check.cpp` | 与参考记录比较（`oracle/rr_oracle -ram A:B`） |
| `tools/equivalence/` | 重构检查：移植版的旧提交与工作副本逐帧对比（见“代码”） |

生成器从用户的 ROM（`rom/River_Raid.rom`）读取表和图形；代码中没有游戏数据。

## 代码
移植版写得不需要手边有原版的反汇编也能读懂：
- 状态字段、函数和常量按含义命名（`planeState`、`shellTimer`、`enteringSlot`、`positionObject`、`kFacingLeft`）；每个字段的注释给出原版的 RAM 地址，每个函数的注释给出它来自的代码地址，所以仍然可以对照反汇编（`disasm/`）和记录；
- 卡带的表通过 `src/rom.h` 中的具名访问函数读取（`rom.points(type)`、`rom.objectHeight(type)`、`rom.shellRise(t)`……），而不是按地址；
- 原版中 8 位回绕的值在这里也保持 8 位回绕（`lo()`），因为位置会绕过屏幕边缘；原版把 6502 的进位标志从一次加法带到下一次时，`addWithCarry`/`subtractWithBorrow` 会明确写出来；
- 显示列表中断（`display.cpp`）是唯一仍然贴近指令的部分：它在扫描线内的时序决定哪些碰撞会被看到，所以它按周期计数执行。

不应改变行为的修改用 `tools/equivalence` 检查：它在工作副本旁构建一个旧提交（06ffc73 及以后），让两者在每局的 150 000 行河上（NTSC 和 PAL）、在从开机开始和从原版记录中 194 个状态开始的随机游戏上运行，每一步之后比较全部状态、屏幕和声音寄存器。生成器、游戏和显示的重命名和重构都这样检查过（0 差异），也通过了下面所有的检查。

```
port/tools/equivalence/build.sh 06ffc73
port/build/eq/eq
```

## 生成器如何工作
每次调用 `nextLine()` 生成屏幕最上方的新一行：
1. `countLines`：数行（块是 32 行，段是 2 块，到桥共 16 段），LFSR 走一步，在段的开头设定新的目标宽度（`chooseSectionWidth`：岛的出现与合拢）；
2. `changeIsland`：岛和水宽平滑地趋向目标；
3. `moveBank`：河岸的随机游走，在界限处和新物体前掉头（`bankWouldHitObject`）；
4. `startDecoration`：河岸上的房子或山；
5. `drawLine`：一行的 48 个字节（填充、来自 `$B800` 表的岸边片段、装饰的一行）；
6. `scrollObjects` / `placeObject`：物体槽位移动，每 16 行放置一个新物体（`chooseEnemy`、`positionObject`、`placeTank`）。

生成既不依赖玩家，也不依赖 `RANDOM`。这在参考运行上检查过：不同的 POKEY 种子，以及机器人运行与无输入飞行相比，每一行的状态都相同。

## 验证
- 从参考状态开始：整条河 119 758 行（120 000 帧），状态和位图行 0 差异。
- 从零开始（`startGame(1)`，不用模拟器）：在填满屏幕的 165 行之后，状态与原版一致，之后 29 757 行没有差异。
- 摧毁桥并有闪光效果的机器人游戏：只有重生帧有差异。它们来自尚未移植的重开代码。
- 检查能抓住故意引入的错误：四处故意破坏（游走条件、水宽、绘制、填充色）每一处都产生了差异。

```
oracle/rr_oracle rom/River_Raid.rom oracle/scripts/ghost_film.txt 30001 out -ghost -ram 0:30000
port/build/terrain_check rom/River_Raid.rom out/ram.bin 0 240 29760 5 -fresh 1 145
```

绘制：在每个有新行进入屏幕第 20 行的帧，把原版的这一行与 C++ 生成器（从零启动）该行的 `drawScreenLine()` 比较。
- 物体放在生成器放置的位置：29 000 帧中 96.8% 的行相同（120 000 帧中 95.3%）。其余的在游戏移动过物体的地方不同。
- 物体位置取自运行中的游戏（`-ram`，槽位 x、标志、种类）：98.1% 相同；剩下的每一行只在公路上的坦克（`$38`）活动时不同，坦克由游戏逻辑绘制。29 600 行中 0 行无法解释。

```
oracle/rr_oracle rom/River_Raid.rom oracle/scripts/ghost_film.txt 30001 out -ghost -strip 20 -ram 0:30000
port/build/render_check rom/River_Raid.rom out/strip.bin out/frames.csv -ram out/ram.bin
```
状态栏和滚动字幕：由操作系统字体和卡带构建的字符集与原版的 RAM 相同，滚动状态（$3FB3/$26/$27）每帧一致，绘制的第 180–239 行在全部 1299 帧中相同，只有滚动字幕最外侧的列除外（见“河流查看器”）。

```
oracle/rr_oracle rom/River_Raid.rom oracle/scripts/attract_idle.txt 2300 out -screens 1000:2299 -ram 1000:2299
port/build/status_check rom/River_Raid.rom out/screens.bin out/ram.bin
```
演示模式中的游戏帧（`game_check`）：从开机后的状态（`Game::startAttract`）启动并给予原版的 RANDOM 值，移植版运行 19 900 帧（到第 20 座桥），在每个比较的字节上都与原版的 RAM 没有差异：生成器状态、物体槽位（位置、方向、种类、碰撞位）、坦克、它的炮弹、直升机导弹、碰撞结果、player-missile 内存、屏幕上的河流行。由此绘制的河的区域在全部 1299 个比较帧中与原版屏幕相同。

```
oracle/rr_oracle rom/River_Raid.rom oracle/scripts/attract_idle.txt 20000 out -strip 20
port/build/game_check rom/River_Raid.rom out/ram.bin out/random.log 38 19900 -attract
```

有玩家的游戏（对带输入的运行做 `game_check`）。从演示模式的状态开始，移植版获得原版的输入和 RANDOM 值，然后独立运行，从不重新加载；每一帧都把它的 RAM 与原版比较（生成器、槽位、飞机、燃料、子弹、碰撞、分数、生命、两名玩家、最高分及其校验字节、状态栏、滚动字幕、player-missile 内存、屏幕行）：
- 单人机器人游戏（`start_only.txt -bot -seed 3`，8 950 帧：演示模式、START、4 架飞机、桥、坠毁与重开、游戏结束、再次演示模式）：每个比较帧都相同；
- 带 SELECT 和暂停键的双人游戏（`two_players.txt`，直到第 3 007 帧游戏结束）：直到游戏结束都相同（一帧，见下文）；
- 移植版绘制的河的区域（飞机、子弹、敌人、爆炸、坦克、炮弹）在一局机器人游戏的全部 1 565 个比较帧中与原版屏幕相同。

原版结果取决于 CPU 周期的地方：
- DLI（`display.cpp`）按周期计数运行，与参考模拟器对一条扫描线的时间分配相同：开头是 player/missile 和显示列表的读取，CPU 运行到第 28 周期，然后绘制整行（包括碰撞），再读取屏幕数据，WSYNC 在第 106 周期。这样就知道读取碰撞寄存器时是否已经看到当前扫描线：只在读取那一行碰到物体的子弹，在桥上算数（那条路径在第 28 周期之后读取），在别处不算，与原版一样。
- RAM 快照在帧末获取；通常这时原版的主循环正在等待下一帧（AE99–AE9E）。刚开始游戏时，河以每帧 2 行重新填充，此时它有时还在绘制第二行（每次填充 1 帧）。`game_check` 单独报告这样的帧，不做比较；下一帧又完全相同。
- 未模拟：游戏结束。主循环的游戏逻辑从第 160 条扫描线开始，与 DLI 和 VBI（第 248 条扫描线）共享 CPU。在游戏结束的那一帧，最高分检查及其校验字节（sub $B261、$B298）要花几千个周期，因此 `sub_B21A` 把帧计数器设为 $FF 的时机可能在 VBI 之前、之后，或者（双人游戏）在下一帧，取决于该帧中它之前所有代码的周期数。移植版在 VBI 之前做；这时帧计数器可能相差一，使之后的演示模式移动一帧。要重现它，需要该帧整个主循环的周期数（包括操作系统的中断处理程序）。

```
oracle/rr_oracle rom/River_Raid.rom oracle/scripts/start_only.txt 9000 out -bot -seed 3
port/build/game_check rom/River_Raid.rom out/ram.bin out/random.log 38 8950 -attract
```

声音。声音驱动（VBI B2A5–B3D8，`Game::soundDriver`）在原版于一局机器人游戏每个 VBI 开始时的 RAM 上运行（2989 帧，其中 2199 帧有声音：引擎、燃料不足警告、爆炸、射击、得分、坦克炮弹）：每个 AUDF/AUDC 值都与原版写入的相同。POKEY 模型（`src/pokey_sound.cpp`）与 atari800 对相同寄存器写入产生的声音做了比较：随时间的响度相同（包络相关系数 0.95），相同音高的纯音相同，噪声的频谱形状相同（平滑频谱的余弦 0.94）；噪声逐个采样的图样不同，因为它取决于噪声计数器当时所处的位置。

```
oracle/rr_oracle rom/River_Raid.rom oracle/scripts/start_only.txt 3000 out -bot -wav -vbi
port/build/sound_check rom/River_Raid.rom out/vbi.bin out/pokey.log out/random.log
port/build/sound_render out/pokey.log port.wav
```

## 尚未完成
- 开机本身（A000–A17C：演示模式之前的前 38 帧）画成空屏幕；`Game::powerOn` 还不精确。
- PAL 机器（`-pal`）：生成器知道它，显示列表和时序还不知道。
- 颜色遵循模拟器对 GTIA 输出的模型（默认 Altirra，按 V 为 atari800；两者都在默认设置下与模拟器自己的调色板代码逐字节核对过，atari800 从其视频电平 16–235 拉伸到 0–255），而不是在真电视上测得的调色板；看起来有多亮，既取决于机器，也同样取决于电视的设置。
