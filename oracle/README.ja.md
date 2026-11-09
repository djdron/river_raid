# rr_oracle — River Raid の基準実行

[English](README.md) | [Русский](README.ru.md) | [Українська](README.uk.md) | [Deutsch](README.de.md) | [Polski](README.pl.md) | [Italiano](README.it.md) | [Español](README.es.md) | [中文](README.zh.md) | 日本語

オリジナルのカートリッジを libatari800 の上でウィンドウなしに動かします（Atari 800、標準は NTSC、AltirraOS 内蔵、BASIC は無効）。C++ 移植をオリジナルとフレームごとに比べるために使います。

## ビルド
```
./build.sh
```
スクリプトは atari800 をクローンし（確認したコミット 98cc2dc）、`libatari800.a` をビルドして `rr_oracle` をリンクします。git、C コンパイラ（gcc または clang）、make、autoconf、automake が必要です。Linux、macOS（Xcode のコマンドラインツール、`brew install autoconf automake`）、WSL、MSYS2 で動きます。基準実行はエミュレータの 3 つの関数（POKEY への書き込み、RANDOM の読み取り、垂直帰線の NMI）を横取りします。スクリプトは `pokey.c` と `cpu.c` をそれらの関数の名前を変えてもう一度コンパイルするので、リンカーの小技は要りません。

## 実行
```
./rr_oracle <rom> <script> <frames> <outdir> [-pal] [-seed N] [-bot|-bot-nofire] [-ghost] [-noram] [-ram A:B] [-strip Y] [-screens A:B] [-shot N,N,...]
./rr_oracle ../rom/River_Raid.rom scripts/start_and_fly.txt 1000 out -shot 240,500
./rr_oracle ../rom/River_Raid.rom scripts/start_only.txt 8000 out -bot      # bot
./rr_oracle ../rom/River_Raid.rom scripts/bot_run_8000.txt 8000 out         # replay of a bot run
```

## ボット（`-bot`）
最初にスクリプトが操作し（たとえば `start_only.txt`：電源投入、START、待機）、その後は自動操縦が引き継ぎます。前方の水面の中央へ向かい、近くの物体をよけ、ずっと撃ち続け、爆発中や橋で待っているあいだは「上」を押します。ボットは単純で、長い記録を取るためだけのもので、ハイスコア向けではありません。燃料を探さず、横から来るヘリにやられます。

`-bot-nofire`：撃たず、よけもしない同じ自動操縦。

どの実行でも、実際に与えた入力はスクリプト形式で `inputs.txt` に書き出されます。それを `-bot` なしで与え直すと、バイト単位で同一の記録になります（8000 フレームで確認）。

## レベルを撮影するためのオプション
- `-ghost` — 無敵の飛行機（エミュレートしたメモリに 5 バイトのコードパッチ。撮影専用）。
- `-strip Y` — 各フレームの画面の Y 行目を `strip.bin` に、パレットを `palette.bin` に。
- `-screens A:B` — フレーム A…B の画面全体を `screens.bin` に。
- `-noram` — `ram.bin` を書かない（長い実行向け）。
- `-ram A:B` — フレーム A…B についてだけ `ram.bin` を書く（フレーム A はオフセット 0）。
- `-wav` — エミュレータの音声出力を `sound.wav` に。
- `-vbi` — `vbi.bin`：各垂直帰線割り込みの開始時の RAM（4 バイトのフレーム番号 + 16 384 バイトのレコード）。サウンドドライバーが見る状態です。
- `frames.csv` の列は `$60, $2F, $5D, $AA, $AB, $AC, $76, $52, $53, $54, $2D, $4E` です。

## スクリプト
| ファイル | 内容 |
|---|---|
| `start_and_fly.txt` | START、加速、旋回、射撃（1000 フレーム） |
| `attract_fire_only.txt` | タイトル画面で発射（ゲームは始まらない） |
| `start_only.txt` | 電源投入、START、待機。`-bot` 用 |
| `probe_lr.txt`、`probe_hold.txt` | 変数を探すための短い押下 / 長押し |
| `bot_run_8000.txt` | 録画したボットの実行：橋 2 つ、墜落 5 回、ゲームオーバー、再スタート |
| `refuel_first_depot.txt` | 撃たずに最初の補給所の上を飛ぶ（給油） |
| `attract_idle.txt` | 電源投入して入力なし：デモモード（ゲーム 1 の川、流れるクレジット） |
| `two_players.txt` | SELECT でゲーム 2、START。2 人とも全機を失うまでまっすぐ飛んで撃つ。一時停止キーを 2 回 |
| `ghost_film.txt` | スタートして入力なしで飛ぶ。`-ghost -noram -strip 20` と組み合わせてレベルの地図を撮る |

## 入力スクリプト
各行：`<フレーム数> <ジョイスティック> <発射> [start] [select] [option] [ジョイスティック2] [発射2] [キー]`。`#` からはコメントです。最後の行は実行の終わりまで繰り返されます。`キー` はスペースキーを押し続けます（どのキーでも River Raid は一時停止します）。
ジョイスティックのビット：1 上、2 下、4 左、8 右、0 中央。
ゲームは START キーで始まります。タイトル画面での発射では始まりません。

## `<outdir>` に出力されるもの
| ファイル | 内容 |
|---|---|
| `ram.bin` | 各フレーム後の $0000–$3FFF（16 KB）。フレーム N はオフセット N*16384 |
| `pokey.log` | POKEY への書き込みすべて：`フレーム 走査線 サイクル レジスタ 値` |
| `random.log` | RANDOM の読み取りすべて：`フレーム 走査線 サイクル 値`。走査線 ~249 はサウンド（VBI）、それ以外の走査線はゲームロジック（A205） |
| `frames.csv` | 与えた入力（ジョイスティック、発射、START、CPU の列の後に SELECT、OPTION、ジョイスティック 2、発射 2、キー）とフレーム終了時の CPU レジスタ |
| `inputs.txt` | 与えた入力をスクリプト形式で（再生用） |
| `fNNNNN.ppm` | 384×240 のスクリーンショット |

## 決定性
atari800 自体は POKEY の生成器をシステムの時計から始めるので、実行は繰り返せません。`rr_oracle` はカウンターを明示的に設定します（`-seed`、標準 0）。同じ ROM、スクリプト、シードなら、すべてのファイルがバイト単位で同一です（確認済み）。

ライセンス：libatari800 は GPL-2.0+ なので、それとリンクしたプログラムである `rr_oracle` も同じです。これは開発用ツールで、C++ 移植はこれとリンクせず、出力ファイルを読むだけです。
