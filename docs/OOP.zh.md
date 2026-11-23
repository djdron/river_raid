# 面向对象的重写（分支 `oop`）

[English](OOP.md) | [Русский](OOP.ru.md) | [Українська](OOP.uk.md) | [Deutsch](OOP.de.md) | [Polski](OOP.pl.md) | [Italiano](OOP.it.md) | [Español](OOP.es.md) | 中文 | [日本語](OOP.ja.md)

分支 `oop`，自 `dli_removed`。目标：把移植版的游戏写成有名字的对象（河道、飞机、它的子弹、坦克和它的炮弹、直升机的导弹、槽位中的船、直升机、喷气机、气球、燃料库、桥和爆炸、玩家和面板、声音），而不是按地址排布的原版 RAM；同时游戏的运行与 `dli_removed` 完全一致，逐帧、逐位相同。ZX Spectrum 移植以 `dli_removed` 为准并与之对照；`oop` 是同一个游戏的重写，不是新游戏。

## 规则

- **同一个游戏。** 每一帧的状态、声音寄存器和画面都与 `dli_removed` 相同。不用 float：位置、速度、燃料和计时器仍是会回绕的字节，带着各自的小数字节；分数、桥数和最高分仍是 BCD。各种特殊行为也保留：一帧中各件事的顺序、槽位及其顺序、一个物体变成另一个、RANDOM 读取的次数和时机、重置的那些帧、两位玩家的交换。
- **每一步之后都检查**，用 `tools/trace/compare.sh`（见下）：所有录制都相同，否则这一步不算完成。
- **一次只做一件事**，每件一个提交：新的类型或类先原样包住状态，然后代码搬进去，最后去掉旧状态。
- **原版的 RAM 仍可访问**：`saveToRam` / `loadFromRam` / `diffWithRam`（`ram_map.h`、`game_ram.cpp`）保持原版的布局，供 `game_check` 和 ZX 移植的检查（`nxcheck`、`tscheck`）使用，无论类内部怎样保存。

## 检查

`port/tools/game_trace` 每帧写一行游戏做了什么，按名称：飞机、子弹、坦克、炮弹、导弹、每个槽位、分数、生命、另一位玩家、最高分、河道生成器、声音寄存器、碰撞结果和画面的哈希（河和面板）。它用 `display_bench` 的自动驾驶来玩；不加 `-ghost` 时飞机会坠毁、会耗尽燃料，所以录制中有生命、重生和游戏结束。

`port/tools/trace/runs.txt` 列出 17 段录制，共 700 000 帧：演示模式、NTSC 和 PAL 上的游戏 1–8、双人、`-wild`（随机转向，也飞到河岸上空）、从第 60 和第 100 座桥开始的长 `-ghost` 录制。`golden.txt` 保存每段录制各行的哈希，即 `dli_removed` 的运行结果，在 `REFERENCE` 中的提交上生成。

```
port/tools/trace/compare.sh <rom> [build dir] [runs...]
```

运行它们（约 2 分钟）并比较。出现差异时，它在临时 worktree 中构建 `REFERENCE` 提交，在那里也运行这段录制，并打印第一个不同的帧的两行。`tools/fixed_check` 对每个输入，把数值类型（见下）与它们取代的代码对照检验。

## 已完成

1. `game_trace`、录制、参考。
2. 去掉了 `PokeyRandom` 约 1 MB 的表：计数器跳过 n 个周期是 GF(2) 上的线性变换，所以由 17 个映射组合而成（2^k 步，1.2 KB），游戏用到的长度保存为每次读 4 位的映射，与 ZX Spectrum 移植的做法相同（每次读取查 5 次表）。已在 600 万次读取上与旧类对照。
3. 显示只用几何：DLI 的模型（`displayBeam`、`displaySimple`、`RR_DISPLAY` 各模式、`RR_GEOMCHECK`、`Beam` 类、实验用的开关）已从本分支去掉；它们保留在 `dli_removal` 上。`display.cpp`：276 行，原来 1331 行。
4. `src/fixed.h`：`Fixed8`（一个字节和它的小数字节，带进位的加 / 减）、`Bcd<N>`（BCD 计数器：向上带进位的加法，原版的比较方式）、`addBcd`。
5. 子弹（`bullet.h`）和飞机（`plane.h`：转向、速度、燃料）成为独立类型。
6. 没有 player-missile 内存：精灵的各行由它们的状态得出（`Game::spriteRow`：每个形状取自 ROM 并位于自己的行，导弹一行，子弹 8 行），从每帧开头显示，直到重置把它们清掉。原版共用这块内存（子弹的行会清掉导弹的行），但下一帧的复制会在任何读取之前把它恢复，所以游戏相同。`saveToRam` 仍按原版的样子写出这块内存。
7. 坦克、它的炮弹和直升机的导弹（`tank.h`、`tank.cpp`）：各一个结构，由河道保存（它们随河道滚动；从第 13 步起由 `Objects` 保存），带有各自的移动（`Tank::drive`、`Shell::move`、`Missile::move`）。
8. 槽位中的物体（`objects.h`、`objects.cpp`）：每种行为一个类（`Explosion`、`Jet`、`Helicopter`、`Balloon`、`Ship`、`Bridge`、`FuelDepot`）。槽位保存原版的字节；它的种类同时也是动画帧，被击中时会改变，所以 `objectKind(kind)` 每帧给出知道这个种类该做什么的那个类的对象：它的动画、它的移动（喷气机横穿屏幕；直升机、气球和船被唤醒后来回移动，到边缘掉头）、显示是否检查它碰到边缘。
9. 玩家（`players.h`）：`PlayerData`（另一位玩家），桥数为 `Bcd<2>`、分数为 `Bcd<3>`，当前玩家用同样的类型，`HighScores`（表和它的校验字节）；面板（`panel.h`）：它的 60 个字符，带 `print`（BCD 数字）、`showLives`、`markPlayers`、`showGameNumber`。
10. 声音（`sound.h`、`sound.cpp`）：`Sounds` 保存游戏请求的声音（开始提示音、事件声音及其时长、爆炸、射击、奖励、导弹、坠毁后下降的音高），它的 `drive()` 就是驱动（B2A5–B3D8），飞机、暂停、演示模式和炮弹通过 `SoundContext` 传给它；`PokeyRegs` 也搬到了那里。
11. 屏幕上的河（`river.h`、`river.cpp`）：它的各行、去掉装饰的同样各行、它们的线段、最上一行在原版屏幕内存中的地址，以及用于小存档的生成器状态；`push()` 加入生成器的下一行，`rebuild()` 在加载状态时重新生成它们。
12. 没有 `GameState`：它的字段成了 `Game` 自己的成员（`eachState()` 为保存、加载和重置列出它们）。
13. 河道与物体分开（`world.h`、`world.cpp`）：`RiverGen` 只生成河道（`RiverState`：河岸、岛、区段、LFSR）；槽位、坦克、它的炮弹和导弹属于 `Objects`，它在每行之后放置新物体（`afterLine`，$ABE1–$AE93），并告诉生成器正在出现的物体在哪里（`BankObstacle`），让河岸避开它。`World` 持有两者并逐行推进；它的 `State` 就是存档和河道各行状态所保存的内容。
14. 从类导出 RAM（`ram_map.h`、`ram_map.cpp`）：每个类在自己的 `mapRam(RamMap &)` 中列出自己的字节及其地址（飞机、子弹、声音、碰撞、玩家、最高分、面板、滚动字幕、河道、槽位、坦克、它的炮弹和导弹、屏幕最上一行）；`World` 和 `Game` 把这些列表合在一起，`RamMap` 负责加载、写入和比较。算出来而不是保存的部分（player-missile 内存、按显示列表得到的屏幕各行）留在 `game_ram.cpp` 和 `River` 中。
15. 文档：各 README 的文件表及其关于代码、生成器和验证的部分，HANDOFF（分支）、NOTES（存档）、HOW_IT_WORKS（碰撞），所有语言；源代码中仍提到 DLI 或说河道保存物体的注释。
16. 收尾：全部 17 段录制相同（700 000 帧），在启用 AddressSanitizer、UndefinedBehaviorSanitizer 和标准库断言的构建上也一样，没有任何报告；本文档的所有语言版本。
17. 按主题把 `game.cpp` 拆成多个文件，类本身不动：`game.cpp`（按原版顺序的一帧）、`game_plane.cpp`（飞机、爆炸、得分）、`game_players.cpp`（游戏与玩家、面板）、`game_reset.cpp`（开机、演示模式、新游戏、重置、下一架飞机）、`game_state.cpp`（存档）；它们共用的内容在 `game_internal.h`。

## 接下来

`Game` 保持它本来的角色——一帧的指挥；围绕它：一局游戏（`Match`：游戏编号、轮到谁、桥数、分数、生命、另一位玩家、最高分，以及它们在面板上的输出）；字段收起来，只留常量 getter 和一个“显示和播放什么”的结构；主机的模式（`Console`：帧计数、演示模式、暂停、SELECT），只在不必调整顺序就能分开的地方拆出；主循环按同样顺序分成有名字的步骤。每一步用 `compare.sh -quick` 检查，最后运行全部 17 段。
