# Xbox の River Raid（UWP）

[English](README.md) | [Русский](README.ru.md) | [Українська](README.uk.md) | [Deutsch](README.de.md) | [Polski](README.pl.md) | [Italiano](README.it.md) | [Español](README.es.md) | [中文](README.zh.md) | 日本語

ゲーム（ビューアは含まない）を UWP アプリとして：Windows 10/11 と、Dev Mode の Xbox One または Series で動きます。SDL3 はもう UWP をサポートしないので、SDL2 でビルドします。Windows 11 と Xbox Series S で試し、Visual Studio 2026 でビルドしました。

## 必要なもの
- Windows 10/11 と Visual Studio 2022 または 2026、C++ の UWP ツール付き。2022：ワークロード「C++ によるデスクトップ開発」と「ユニバーサル Windows プラットフォーム開発」（C++ UWP ツールを含む）。2026：「C++ によるデスクトップ開発」と、ワークロード「WinUI アプリケーション開発」のコンポーネント「C++ Universal Windows Platform tools」。
- CMake 3.16 以降（Visual Studio 付属のもので可）と git：SDL2 はゲームと一緒にダウンロードしてビルドされます。
- Xbox 用：開発者アカウントと、Dev Mode にした本体（Xbox の「Dev Mode Activation」アプリ）。
- あなた自身のカートリッジのダンプを `rom/River_Raid.rom`（8192 バイト）として：これは**パッケージの中に**入るので、パッケージはあなた専用です。配布しないでください。

## ビルド
```
cmake -S port -B build-uwp -G "Visual Studio 17 2022" -A x64 -DCMAKE_SYSTEM_NAME=WindowsStore -DCMAKE_SYSTEM_VERSION=10.0
```
Visual Studio 2026 ではジェネレーターは `-G "Visual Studio 18 2026"`（CMake 4.2 以降。付属のもので可）。

`build-uwp/river_raid_port.sln` を開き、Release と x64 を選び、`river_raid` をスタートアップ プロジェクトにします。まずローカル コンピューター（Windows）で実行してください：ウィンドウにゲームが出るはずです。

パッケージは CMake 付属のテスト証明書（`CN=CMake Test Cert`）で署名されます。自分の証明書は：`-DRR_UWP_CERTIFICATE=my.pfx -DRR_UWP_PUBLISHER="CN=..."`（証明書のサブジェクト）。ROM の別のパス：`-DRR_UWP_ROM=...`。

## Xbox へ
どちらの方法でも構いません：
- **Visual Studio から：**デバッグ先を「リモート コンピューター」、Xbox の IP アドレス（Dev Home に表示）、認証を「Universal (Unencrypted Protocol)」にし、Dev Home に表示される PIN でペアリングします。
- **Device Portal でパッケージを：**Visual Studio で プロジェクト → 公開 → アプリ パッケージの作成 → サイドローディング、x64。次に PC のブラウザーで `https://<xbox の ip>:11443`（Device Portal、Dev Home でオンにする）を開き、Add で `.msix` とそのフォルダーにある依存パッケージ（VCLibs）を選びます。

その後 Dev Home でアプリを選び、View details で App type を **Game** にします：そうしないとアプリが使える本体の資源が少なくなります。 Xbox で誰かがサインインしている必要があります：そうでないとアプリが起動しません（Visual Studio：「Unable to activate Windows Store app … 0x8004090a」）。

ゲームパッドは PC と同じように使えます（[../README.ja.md](../README.ja.md) を参照）。X はポーズです。キーボードがないので、ほかの設定は既定のままです：PAL の色、Altirra、ゲーム画面のみ、くっきりした拡大（LT で切り替え）。B はシステムの戻るボタンでもあります。ゲームがこれを受け取るので、アプリは閉じません。Xbox ボタンでゲームを離れます。

ゲームの起動中に LT を押し続けると PAL 機として動き（`-pal` と同じ：1 秒 50 フレーム、約 17% 遅い）、本体が許可していればテレビを 50 Hz に切り替え（Settings → TV & display options → Video modes → Allow 50 Hz）、終了時に元に戻します。50 Hz がなくても PAL として動きますが、テレビとは同期しません。

PC と同じオプション（[../README.ja.md](../README.ja.md) を参照）は起動時にも指定できます：Dev Home でゲームを選び、Launch with parameters で Use custom protocol にチェックを入れて、例えば `riverraid:-pal -bridge 500` と入力します。アプリはプロトコル `riverraid` を宣言しており、その後に続く部分がコマンドラインになります。
