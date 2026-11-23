# オブジェクト指向への書き直し（ブランチ `oop`）

[English](OOP.md) | [Русский](OOP.ru.md) | [Українська](OOP.uk.md) | [Deutsch](OOP.de.md) | [Polski](OOP.pl.md) | [Italiano](OOP.it.md) | [Español](OOP.es.md) | [中文](OOP.zh.md) | 日本語

ブランチ `oop`、`dli_removed` から。目標：移植版のゲームを、アドレスで並んだオリジナルの RAM ではなく、名前のあるオブジェクト（川、飛行機、その弾、戦車とその砲弾、ヘリコプターのミサイル、スロットの中の船・ヘリコプター・ジェット機・気球・燃料基地・橋・爆発、プレイヤーとパネル、音）として書くこと。その間もゲームは `dli_removed` とまったく同じに、フレームごと、ビットごとに動きます。ZX Spectrum 移植版は `dli_removed` に従い、それと照合されています。`oop` は同じゲームの書き直しで、新しいゲームではありません。

## 方針

- **同じゲーム。** 毎フレーム、`dli_removed` と同じ状態、同じ音のレジスター、同じ絵。float は使いません：位置、速度、燃料、タイマーは一周するバイトのままで、それぞれの小数部のバイトを持ちます。得点、橋、ハイスコアは BCD のままです。癖もそのまま残します：フレーム内での処理の順序、スロットとその順序、物体が別の物体に変わること、RANDOM を読む回数とタイミング、リセットのフレーム、2 人のプレイヤーの交代。
- **各段階の後に確認** します。`tools/trace/compare.sh`（下記）ですべての走行が同じでなければ、その段階は終わっていません。
- **一度に一つずつ**、それぞれ別のコミットで：新しい型やクラスはまず状態をそのまま包み、次にコードがそこへ移り、最後に古い状態がなくなります。
- **オリジナルの RAM には引き続き届きます**：`saveToRam` / `loadFromRam` / `diffWithRam`（`ram_map.h`、`game_ram.cpp`）がオリジナルのレイアウトを保ちます。`game_check` と ZX 移植版の確認（`nxcheck`、`tscheck`）のためで、クラスが内部で何を持っていても変わりません。

## 確認

`port/tools/game_trace` は、ゲームが何をしているかを名前で 1 フレーム 1 行に書きます：飛行機、弾、戦車、砲弾、ミサイル、各スロット、得点、残機、もう一人のプレイヤー、ハイスコア、川の生成器、音のレジスター、衝突の結果、絵のハッシュ（川とパネル）。`display_bench` の自動操縦で遊びます。`-ghost` なしでは飛行機が墜落し、燃料も切れるので、残機、復活、ゲームオーバーが走行に含まれます。

`port/tools/trace/runs.txt` には 17 本の走行、700 000 フレームがあります：デモモード、NTSC と PAL のゲーム 1–8、2 人プレイ、`-wild`（ランダムな操縦、岸の上も）、橋 60 と 100 からの長い `-ghost` 走行。`golden.txt` には、`dli_removed` が遊んだときの各走行の行のハッシュがあり、`REFERENCE` のコミットで作ったものです。

```
port/tools/trace/compare.sh <rom> [build dir] [runs...]
```

はそれらを再生し（約 2 分）比べます。違いがあると、`REFERENCE` のコミットを一時的な worktree でビルドし、そこでもその走行を再生して、最初に違うフレームの 2 行を表示します。`tools/fixed_check` は数値型（下記）を、置き換えたコードとすべての入力で照合します。

## 完了

1. `game_trace`、走行、基準。
2. `PokeyRandom` から約 1 MB の表をなくしました：カウンターを n サイクル進めるのは GF(2) 上の線形写像なので、17 個の写像（2^k ステップ、1.2 KB）から組み立てます。ゲームが使う長さは、ZX Spectrum 移植版と同じく 4 ビットずつ読む写像として持ちます（1 回の読み取りで表を 5 回引く）。古いクラスと 600 万回の読み取りで照合済み。
3. 表示は幾何だけ：DLI のモデル（`displayBeam`、`displaySimple`、`RR_DISPLAY` の各モード、`RR_GEOMCHECK`、`Beam` クラス、実験のスイッチ）はこのブランチから取り除きました。`dli_removal` には残っています。`display.cpp`：1331 行から 276 行に。
4. `src/fixed.h`：`Fixed8`（1 バイトとその小数部のバイト、桁上がり付きの加算 / 減算）、`Bcd<N>`（BCD カウンター：上の桁への桁上がり付き加算、オリジナルの比較）、`addBcd`。
5. 弾（`bullet.h`）と飛行機（`plane.h`：操縦、速度、燃料）を独立した型に。
6. プレイヤー/ミサイルのメモリーはなし：スプライトの各行はその状態から決まります（`Game::spriteRow`：各形は ROM からその行に、ミサイルは 1 行、弾は 8 行）。各フレームの初めから、リセットで消えるまで表示されます。オリジナルではこのメモリーを共有していて（弾の行がミサイルの行を消す）、それは誰かが読む前に次のフレームのコピーで元に戻るので、ゲームは同じです。`saveToRam` は今もこのメモリーをオリジナルの形で書き出します。
7. 戦車、その砲弾、ヘリコプターのミサイル（`tank.h`、`tank.cpp`）：それぞれ 1 つの構造体で、川が持ちます（川といっしょにスクロールするため。段階 13 からは `Objects` が持つ）。それぞれの動き（`Tank::drive`、`Shell::move`、`Missile::move`）も。
8. スロットの中の物体（`objects.h`、`objects.cpp`）：振る舞いの種類ごとのクラス（`Explosion`、`Jet`、`Helicopter`、`Balloon`、`Ship`、`Bridge`、`FuelDepot`）。スロットはオリジナルのバイトを持ちます。その種類はアニメーションのコマでもあり、撃たれると変わるので、`objectKind(kind)` が毎フレーム、その種類が何をするかを知っているクラスのオブジェクトを返します：アニメーション、動き（ジェット機は画面を横切る。ヘリコプター、気球、船は目覚めると行き来し、端で向きを変える）、表示が端との接触を調べるかどうか。
9. プレイヤー（`players.h`）：`PlayerData`（もう一人のプレイヤー）は橋が `Bcd<2>`、得点が `Bcd<3>`、現在のプレイヤーも同じ型、`HighScores`（表とそのチェックバイト）。パネル（`panel.h`）：60 文字と `print`（BCD の数字）、`showLives`、`markPlayers`、`showGameNumber`。
10. 音（`sound.h`、`sound.cpp`）：`Sounds` はゲームが求める音（開始の音、イベントの音とその長さ、爆発、発射、ボーナス、ミサイル、墜落後に下がっていく音程）を持ち、その `drive()` がドライバー（B2A5–B3D8）です。飛行機、ポーズ、デモモード、砲弾は `SoundContext` で渡します。`PokeyRegs` もそこへ移りました。
11. 画面上の川（`river.h`、`river.cpp`）：そのライン、装飾を除いた同じライン、その区間、オリジナルの画面メモリーでの最上ラインのアドレス、小さなセーブのための生成器の状態。`push()` が生成器の次のラインを加え、`rebuild()` が状態を読み込むときにそれらを作り直します。
12. `GameState` はなし：そのフィールドは `Game` 自身のメンバーになりました（`eachState()` が保存、読み込み、リセットのためにそれらを列挙）。
13. 川を物体から分離（`world.h`、`world.cpp`）：`RiverGen` は川だけを作ります（`RiverState`：岸、島、区間、LFSR）。スロット、戦車、その砲弾とミサイルは `Objects` で、各ラインの後に新しい物体を置き（`afterLine`、$ABE1–$AE93）、上端から入ってくる最中の物体の位置を生成器に伝えて（`BankObstacle`）、岸がそれを避けるようにします。`World` が両方を持って 1 ラインずつ進めます。その `State` が、セーブと川の各ラインの状態として残すものです。
14. クラスからの RAM の書き出し（`ram_map.h`、`ram_map.cpp`）：各クラスは自分の `mapRam(RamMap &)` で、自分のバイトとそのアドレスを列挙します（飛行機、弾、音、衝突、プレイヤー、ハイスコア、パネル、スクロールする行、川、スロット、戦車、その砲弾とミサイル、画面の最上ライン）。`World` と `Game` がその一覧をまとめ、`RamMap` が読み込み、書き出し、比較を行います。持つのではなく計算で出すもの（プレイヤー/ミサイルのメモリー、ディスプレイリストからの画面のライン）は `game_ram.cpp` と `River` に残っています。
15. 文書：各 README のファイル表と、コード・生成器・検証の部分、HANDOFF（ブランチ）、NOTES（セーブ）、HOW_IT_WORKS（衝突）を全言語で。ソースのコメントで、まだ DLI のことや、川が物体を持つことを書いていたところも。
16. 仕上げ：17 本の走行がすべて同じ（700 000 フレーム）。AddressSanitizer、UndefinedBehaviorSanitizer、標準ライブラリーのアサーションを有効にしたビルドでも同じで、報告は一つもありません。この文書を全言語に。
17. `game.cpp` をテーマごとのファイルに分けました。クラスはそのまま：`game.cpp`（オリジナルの順序どおりのフレーム）、`game_plane.cpp`（飛行機、爆発、得点）、`game_players.cpp`（ゲームとプレイヤー、パネル）、`game_reset.cpp`（電源投入、デモモード、新しいゲーム、リセット、次の飛行機）、`game_state.cpp`（セーブ）。共通のものは `game_internal.h` に。
18. 対戦（`match.h`、`match.cpp`）：`Match` はゲーム番号、誰の番か、両プレイヤーの橋・得点・残機、`respawned`、ハイスコアを持ち、`setUp`（sub $AF80）、`selectNext`、`swapPlayers`（川の状態を受け取る：レベルと再開地点も相手に移る）、`updateHighScore`、`addPoints`（A547：得点、ボーナスの飛行機、999 999 を超えたときの終了）、パネルの数字（デモモードでは表示しない。それは `Game` が伝える）を備えます。バイトは以前のフィールドと同じ順序なので、セーブは変わりません。途中で見つけたこと：`World::State` に 2 バイトの詰め物があり、メモリーにたまたまあった値で保存されていました。今は明示的なゼロのバイトで、同じ状態はいつも同じバイトで保存されます。
19. `Game` のフィールドを private に（`name_`）。外からは const のゲッター（`world()`、`plane()`、`match()`、`pokey()`…）、`output()`（フレームが見せて鳴らすもの：パネル、燃料、スクロールする行、ロゴ、画面オフ、音のレジスター）、そしてフィールドへの書き込みの代わりにいくつかの名前付きの操作：`setRiver`、`keepLineStates`、`skipState` / `setSkipState`（川ビューアーのアンカー）、`playSound` と `stayAwake`（ビューアー）、`ghost`（ツールのゴーストモード）、`setPokey`（`sound_check`）。`sound_check` はドライバーが読むバイトを `mapRam` 経由で読み込みます。以前と同じバイトです。
20. 本体（`console.h`、`console.cpp`）：`Console` はフレームカウンター、デモモードのフラグとタイマー、無操作の時間、ポーズ、SELECT の遅延、`powerOnDone`、`screenOn` を持ち、`keys`（A19E-A1F5：操作の有無、ポーズ、画面オフ）、`attractTimerTick`（A2F5）、`countFrame`（VBI のカウンター。`skipFrame` でも使う）、`selectRepeat`（A699）を備えます。フレームの順序がそれらを他の処理と混ぜるところ（新しいゲーム、ゲームの選択、ゲームオーバー、ゲームオーバー前の PAL の VBI）では、今も `Game` がフィールドを直接設定します。同じバイトが同じ順序なので、セーブは変わりません。ポーズ、SELECT、OPTION、START、画面オフは、こうしたキー入力のある 390 000 フレームで前のコミットと照合しました。
21. メインループを、オリジナルの順序のまま何も動かさずに名前付きの手順にしました：`mainLoop` は `console_.keys`（A19E-A1F5）、`wakeObject`（A1FF）、`driveTank`（A21B）、`moveObjects`（A26C）、`attractTimerRunsOut`（A2F5）、`readStick`（A312）、`updatePlane`（A33F）。復活がそこから続く `mainLoopEnd` は `moveMissile`（A5B7）、`moveShell`（A607）、`consoleKeys`（A673）、`scroll`（A6DE）。リセットを始める手順は true を返し、ループはそこで止まります（以前と同じ）。最後に 17 本すべてが同じで、サニタイザー付きのビルドでも同じです。
22. 小さな片付け。`game.h` で川ビューアーとツールだけが使うもの（`frame()` の各部分、`skipFrame` と `SkipState`、`attractSound`、`playSound`、`stayAwake`、`ghost`、RAM のイメージ、`setPokey`）を、ゲーム自身のインターフェースの後の一つのブロックにまとめ、`resetState` と `eachState` は private にしました。ビューアー用のデモモードの音の設定は一つの `AttractSound`（sound.h）。`game_internal.h` はヘッダー内の無名名前空間をやめて `namespace rr::game_detail` と `inline constexpr` に。パネルに出す対戦の数字は `Scoreboard`（match.h）で、対戦、パネル、デモモードは一度だけ渡します。`showOtherScore` は `player2Score` に改名し、なぜ番のプレイヤーの得点（$63-$65）を表示するのかを書きました（sub $B068 がそうしています）。

## 次に

予定はありません。`Game` はフレームの指揮者で、持っているものにはそれぞれのクラスがあります（`World`、`Match`、`Console`、`Plane`、`Sounds`…）。これからの変更も同じように確認します：各段階の後に `compare.sh -quick`、最後に 17 本すべて。
