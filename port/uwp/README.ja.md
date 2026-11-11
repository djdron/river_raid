# Xbox の River Raid（UWP）

[English](README.md) | [Русский](README.ru.md) | [Українська](README.uk.md) | [Deutsch](README.de.md) | [Polski](README.pl.md) | [Italiano](README.it.md) | [Español](README.es.md) | [中文](README.zh.md) | 日本語

ゲーム（ビューアは含まない）を UWP アプリとして：Windows 10/11 と、Dev Mode の Xbox One または Series で動きます。SDL3 はもう UWP をサポートしないので、SDL2 でビルドします。**まだ試していません**：手元に Windows がない状態で書きました。手順が説明どおりに動かなければ、何が起きたか知らせてください。

## 必要なもの
- Windows 10/11 と Visual Studio 2022。ワークロード「C++ によるデスクトップ開発」と「ユニバーサル Windows プラットフォーム開発」（C++ UWP ツール、v143 を含む）。
- CMake 3.16 以降（Visual Studio 付属のもので可）と git：SDL2 はゲームと一緒にダウンロードしてビルドされます。
- Xbox 用：開発者アカウントと、Dev Mode にした本体（Xbox の「Dev Mode Activation」アプリ）。
- あなた自身のカートリッジのダンプを `rom/River_Raid.rom`（8192 バイト）として：これは**パッケージの中に**入るので、パッケージはあなた専用です。配布しないでください。

## ビルド
```
cmake -S port -B build-uwp -G "Visual Studio 17 2022" -A x64 -DCMAKE_SYSTEM_NAME=WindowsStore -DCMAKE_SYSTEM_VERSION=10.0
```
`build-uwp/river_raid_port.sln` を開き、Release と x64 を選び、`river_raid` をスタートアップ プロジェクトにします。まずローカル コンピューター（Windows）で実行してください：ウィンドウにゲームが出るはずです。

パッケージは CMake 付属のテスト証明書（`CN=CMake Test Cert`）で署名されます。自分の証明書は：`-DRR_UWP_CERTIFICATE=my.pfx -DRR_UWP_PUBLISHER="CN=..."`（証明書のサブジェクト）。ROM の別のパス：`-DRR_UWP_ROM=...`。

## Xbox へ
どちらの方法でも構いません：
- **Visual Studio から：**デバッグ先を「リモート コンピューター」、Xbox の IP アドレス（Dev Home に表示）、認証を「Universal (Unencrypted Protocol)」にし、Dev Home に表示される PIN でペアリングします。
- **Device Portal でパッケージを：**Visual Studio で プロジェクト → 公開 → アプリ パッケージの作成 → サイドローディング、x64。次に PC のブラウザーで `https://<xbox の ip>:11443`（Device Portal、Dev Home でオンにする）を開き、Add で `.msix` とそのフォルダーにある依存パッケージ（VCLibs）を選びます。

その後 Dev Home でアプリを選び、View details で App type を **Game** にします：そうしないとアプリが使える本体の資源が少なくなります。

## Xbox での操作
| ゲームパッド | |
|---|---|
| 十字キーまたは左スティック | ジョイスティック |
| A、B、Y または右トリガー | 発射 |
| X | ポーズのキー（Xbox にはキーボードがない） |
| Menu | START |
| View | SELECT |
| RB / LB | 状態を保存 / そこへ戻る |

色、パレット、枠、拡大は既定のまま（PAL の色、Altirra、ゲーム画面のみ、くっきり）。B はシステムの戻るボタンでもあります。ゲームがこれを受け取るので、アプリは閉じません。Xbox ボタンでゲームを離れます。

## まだ必要かもしれないこと
- テレビのセーフエリア：Xbox の UWP アプリは画面の端から余白をとって描かれることがあります。画面はもともと黒に囲まれていますが、余白が邪魔なら、アプリは画面全体を要求できます（`ApplicationView::SetDesiredBoundsMode`）。
- ファイルの組み立て方（`uwp/uwp.cmake`、`uwp/Package.appxmanifest.in`）：CMake の UWP プロジェクト対応と SDL2 の WinRT エントリポイント（`SDL_winrt_main_NonXAML.cpp`、C++/CX としてコンパイル）はドキュメントどおりに使っていますが、ここではビルドしていません。
