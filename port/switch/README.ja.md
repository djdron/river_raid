# Nintendo Switch の River Raid（homebrew）

[English](README.md) | [Русский](README.ru.md) | [Українська](README.uk.md) | [Deutsch](README.de.md) | [Polski](README.pl.md) | [Italiano](README.it.md) | [Español](README.es.md) | [中文](README.zh.md) | 日本語

ゲーム（ビューアは含まない）を homebrew アプリとして：Homebrew Launcher で homebrew を動かせる Switch 用の `.nro` です。devkitPro とその SDL2 でビルドします（SDL 本体は Switch に対応していません）。ビルドはできます。本体ではまだ試していません。

## 必要なもの
- Switch 用パッケージ `switch-dev` と `switch-sdl2`（`dkp-pacman` で入れる）の入った devkitPro、またはその Docker イメージ `devkitpro/devkita64`。
- あなた自身のカートリッジのダンプを `rom/River_Raid.rom`（8192 バイト。別のパス：`-DRR_SWITCH_ROM=...`）として。これは `.nro` の**中**（その RomFS）に入るので、その `.nro` はあなた専用です。人に渡さないでください。

## ビルド
リポジトリのルートから：
```
cmake -S port -B build-switch -DCMAKE_TOOLCHAIN_FILE=$DEVKITPRO/cmake/Switch.cmake -DCMAKE_BUILD_TYPE=Release
cmake --build build-switch
```
`build-switch/river_raid.nro` ができます。ROM がなくてもビルドでき、そのときは SD カードの ROM を探します（下記）。CI（`.github/workflows/switch.yml`）は `port/` を変えるプッシュのたびにこれをビルドし、（ROM なしで）実行の成果物として残します。

## 本体で
`river_raid.nro` を SD カード（例えば `/switch/river_raid/`）にコピーし、Homebrew Launcher から River Raid (port) を起動します。ROM なしでビルドした `.nro` は、隣の `rom` フォルダーから ROM を読みます：
```
/switch/river_raid/river_raid.nro
/switch/river_raid/rom/River_Raid.rom
```
ROM がないとゲームはそう表示して + を待ちます。

操作は PC のゲームパッドと同じです（[../README.ja.md](../README.ja.md) を参照）：左スティックか十字ボタンで操縦、A、B、Y か ZR で発射、X でポーズ、R で状態を保存し L でそこに戻る、ZL で拡大の方式を切り替え。− が SELECT、+ が START で、両方同時に押すと NTSC と PAL を切り替えます。ゲームは HOME ボタンで終わります（ソフトを終了）。Switch の画面とテレビ出力は 60 Hz だけなので、PAL 機は自分の 1 秒 49.86 フレームで動き、画面とは同期しません。
