# Windowsコマンドツール集

ファイル整理、リネーム、アーカイブ変換、画像変換のためのC++製コマンドラインツールです。ソリューションは`cmds_cpp.slnx`です。

全9プロジェクトの現行仕様・配置を記載しています（2026-10-09更新）。コマンド例のビルド・テストはリポジトリのルートで実行してください。各READMEの過去の速度比較・検証記録は、その記載環境での結果です。

## プロジェクト一覧

| プロジェクト | 用途 | 外部ツール・主なライブラリ |
|---|---|---|
| a2d | ZIP/RARをフォルダへ展開 | bit7z、7z.dll |
| d2z | フォルダの中身をZIPへ圧縮 | bit7z、7z.dll |
| r2z | RARを展開してZIPへ再圧縮 | bit7z、7z.dll |
| fs | 名前の角括弧を使ったファイル振り分け・移動 | なし |
| rh | 削除語・全角半角変換による名前の整理 | Windows Unicode正規化API |
| rb | `[aaa(bbb)]`を`[bbb]`へ変更 | Windows Unicode正規化API |
| renNFD | ファイル名・フォルダ名をNFCへ正規化 | Windows Unicode正規化API |
| wp | WebPをJPEGへ変換 | libwebp、libjpeg-turbo |
| ThumFromZip | ZIP内のJPG/PNGを1枚抽出 | 7z.dll、同梱7-Zip SDKヘッダー |

## アーカイブ関連

### a2d — ZIP/RARの展開

```powershell
a2d.exe "archive.zip" "data.rar"
```

bit7z経由で`7z.dll`を直接使用し、複数のZIP/RARを順に展開します。DLLは一括処理で再利用します。出力先はアーカイブと同じ場所にある、拡張子を除いた名前のフォルダです。展開結果が単一ルートフォルダなら、その中身を出力先へ配置し、二重フォルダを避けます。

- 全階層の`.scr`・`.SCR`を除外します。
- 一時フォルダへの展開が成功してから正式な出力先へ移動します。
- 既存の出力先には結合・上書きしません。
- 元アーカイブは保持します。失敗があれば終了コード1を返します。

DLLは`--dll`または`a2d.pref`で指定できます。旧`7z.exe`パスは同じ場所の`7z.dll`へ読み替えます。危険なパス・重複名・リンクや暗号化された保持対象項目は拒否します。

詳細・速度比較: [a2d/README.md](a2d/README.md)

### d2z — フォルダのZIP圧縮

```powershell
d2z.exe "C:\Images" "C:\Documents"
d2z.exe --fast "C:\Images"
d2z.exe --store "C:\Videos"
```

フォルダの隣に`フォルダ名.zip`を作成します。格納パスは圧縮対象からの相対パスです。入力直下にサブフォルダが1つだけあり、ほかに項目がなければ、そのサブフォルダの中身を圧縮します。

- bit7z経由で`7z.dll`を直接利用し、DLLを一括処理で再利用します。通常実行は標準圧縮です。
- `--fast`は高速圧縮、`--store`は圧縮なしのZIP作成です。
- 新しい一時ZIPを作成し、成功後に確定します。既存ZIPは更新・上書きしません。
- 元フォルダは保持します。失敗があれば終了コード1を返します。

処理前に件数と対象フォルダ、圧縮中に割合を表示します。DLLは`--dll`または`d2z.pref`で指定できます。

詳細・速度比較: [d2z/README.md](d2z/README.md)

### r2z — 7z.dllを使うRAR→ZIP変換

```powershell
r2z.exe "archive.rar" "another.RAR"
r2z.exe --fast "archive.rar"
r2z.exe --store "archive.rar"
```

bit7zを通じて`7z.dll`を直接利用し、RAR4/RAR5をZIPへ変換します。DLLは一括処理で再利用します。全階層の`.scr`を除外し、展開結果が単一フォルダならその中身を再圧縮します。

元RARは保持し、既存ZIPは上書きしません。一時ZIPが完成してから正式名へ移動し、失敗があれば終了コード1を返します。暗号化された保持対象項目や危険なパス・リンク・重複名は拒否します。

DLLは`--dll`または実行ファイルの隣の`r2z.pref`で指定できます。指定がなければ実行ファイルの隣、次に`C:\Program Files\7-Zip\7z.dll`を参照します。旧`.pref`の`7z.exe`は同じ場所の`7z.dll`へ読み替えます。

詳細・速度比較: [r2z/README.md](r2z/README.md)

## ファイル整理・リネーム

### fs — 角括弧による振り分け

```powershell
fs.exe "C:\Downloads"
fs.exe -r "C:\Downloads"
```

通常モードは、対象フォルダ直下のZIP/RARについて、名前の最初の`[...]`内の文字列をフォルダ名として振り分けます。

例: `[作者名]作品.zip` → `作者名\[作者名]作品.zip`

`-r`は直下の各サブフォルダにあるファイルを1階層上へ移動し、移動後に空になったフォルダを削除します。逆モードではZIP/RAR以外のファイルも対象です。引数を省略すると通常モードで作業フォルダを処理します。

同名項目を上書きせず、危険な振り分け先名を拒否します。ジャンクションなどのリンクはたどりません。失敗後も後続を処理し、失敗があれば終了コード1を返します。

詳細: [fs/README.md](fs/README.md)

### rh — 名前の整理と削除語の適用

```powershell
rh.exe "C:\Downloads"
```

フォルダ直下の全ファイル・サブフォルダ名を、renNFDと同じNFD→NFCと波ダッシュ統一で正規化します。拡張子も正規化します。全角英数字・括弧などの変換、削除語の除去、空白整理は、従来どおりフォルダと`.zip .rar .7z .mp4 .mov .mpg .mpeg`のファイル名本体に適用します。

- 削除語は実行ファイルの隣の`rh.lst`から読み込みます。UTF-8のBOMあり・なしに対応します。
- 同名になる場合は`_1`、`_2`などを付加し、既存項目を上書きしません。
- 非再帰処理です。引数省略時は作業フォルダを対象にします。
- 失敗理由と変更・変更不要・失敗の件数を表示します。

詳細: [rh/README.md](rh/README.md)

### rb — 角括弧内の表記を短縮

```powershell
rb.exe "C:\Downloads"
rb.exe "C:\Downloads\[aaa(bbb)]作品.zip"
```

名前に含まれる`[aaa(bbb)]`を`[bbb]`へ変換します。フォルダ指定では直下のサブフォルダと、rhと同じ対応拡張子のファイルを処理します。単一ファイルの指定にも対応します。

同名があれば連番を付加し、対象フォルダに`logYYYYMMDD_HHMM.txt`形式の変更ログを追記します。rhとは別のリネーム処理で、今回のrhの安全性・高速化修正は未反映です。

詳細・現行実装の制約: [rb/README.md](rb/README.md)

### renNFD — 名前をNFCへ正規化

```powershell
renNFD.exe "C:\Files" --dry-run
renNFD.exe "C:\Files"
```

名前はrenNFDですが、実際の変換先は**NFC（合成形）**です。指定フォルダ内の全ファイル・全サブフォルダを再帰的に処理し、結合文字を合成します。波ダッシュ`〜`も全角チルダ`～`へ統一します。

- 例: `か`＋結合濁点 → `が`、`e`＋結合アクセント → `é`。
- 拡張子の制限はなく、ファイルの内容は変更しません。指定フォルダ自身は対象外です。
- `--dry-run`で変更予定だけを表示します。
- 現行実装には同名衝突を保護する処理がありません。実行前に変更予定を確認してください。
- コード内の使用法表示は`renNFC.exe`ですが、プロジェクト名は`renNFD`です。

詳細・終了コードなどの制約: [renNFD/README.md](renNFD/README.md)

## 画像

### ThumFromZip — ZIPから代表画像を抽出

```powershell
ThumFromZip.exe "C:\Images"
ThumFromZip.exe --overwrite "C:\Images"
```

指定フォルダ以下を再帰的に検索し、各ZIPのJPG/JPEG/PNGから、ファイル名をASCIIの大文字小文字を区別せず比較して先頭になる1枚を抽出します。同名ならZIP内のフルパスで比較します。画像の縮小や再エンコードはしません。引数省略時は作業フォルダを対象にします。

- 出力先はZIPの隣です。JPEGは`.jpeg`、PNGは`.png`になります。
- 同名の`.jpg`・`.jpeg`・`.png`があれば通常はスキップします。
- `--overwrite`（`-f`）では、一時ファイルへの抽出成功後に対象画像を置き換えます。
- 元ZIPを保持し、処理前の対象名と結果を表示します。ログへのリダイレクトはUTF-8です。
- 実行ファイルと同じアーキテクチャの`7z.dll`が必要です。実行ファイルの隣を優先し、なければ`%ProgramFiles%\7-Zip\7z.dll`を探します。隣にあるDLLの読み込みに失敗した場合はエラーになります。
- bit7zは使用しません。`--dll`や`.pref`によるDLL指定には対応していません。

詳細・検証記録: [ThumFromZip/README.md](ThumFromZip/README.md)

### wp — WebP→JPEG変換

```powershell
wp.exe "image.webp"
wp.exe "C:\Images"
```

単一WebP、またはフォルダ直下のWebPを、品質75・色差サブサンプリング4:2:0のJPEGへ変換します。libwebpでデコードし、libjpeg-turboで圧縮します。

- **JPEGの書き込み・確定後に元WebPを削除します。** JPEGには透過情報を保持しません。
- 通常は拡張子を`.jpeg`へ変更します。`imgi_1_1.webp`などは`img001.jpeg`へ変換します。
- 既存JPEGや出力名の重複がある場合は、一括処理開始前に停止します。

詳細: [wp/README.md](wp/README.md)

## 実行環境・依存関係

- Windows向けです。現在の画像ライブラリ構成と検証はx64を中心としています。
- a2d・d2z・r2zはbit7zと、実行ファイルと同じアーキテクチャの`7z.dll`が必要です。
- ThumFromZipも`7z.dll`が必要です。同梱SDKヘッダーを使って直接呼び出します。
- wpは同梱の`packages/libwebp-1.6.0-windows-x64`と`packages/libjpeg-turbo64`を参照します。
- rh・rb・renNFDなどの名前変更ツールは7-Zipを使用しません。
- C++ランタイムの要否は各プロジェクトのビルド設定によって異なります。

### 7z.dllの場所を変更する

実行ファイルの隣に`a2d.pref`、`d2z.pref`、`r2z.pref`を配置し、1行目にDLLのパスを記述します。UTF-8（BOMあり・なし）とWindowsコードページに対応します。旧設定の`7z.exe`は同じ場所の`7z.dll`へ読み替えます。コマンドラインの`--dll`指定が優先されます。

```text
D:\Tools\7-Zip\7z.dll
```

設定がなければ実行ファイルの隣、次に`C:\Program Files\7-Zip\7z.dll`を参照します。rhの削除語設定`rh.lst`とは用途が異なります。

## ビルド

Visual Studioの「C++によるデスクトップ開発」とWindows SDKが必要です。プロジェクトにはMSVCツールセット`v145`と一部`v143`の指定が混在しています。指定されたツールセットを用意するか、使用環境に合わせてリターゲットしてください。

1. `.slnx`形式に対応するVisual Studioで`cmds_cpp.slnx`を開きます。
2. 必要な個別プロジェクトを選択します。
3. 原則として`Release | x64`を選択します。
4. a2d・d2z・r2zはCMakeを呼び出して依存ライブラリを取得します。初回はネット接続が必要です。
5. ThumFromZipは同梱ヘッダーでビルドできます。Releaseは`/MT`、Debugは`/MTd`です。実行時の7z.dllは別途必要です。

x64 Native Tools環境から個別にビルドする例:

```powershell
msbuild rh\rh.vcxproj /p:Configuration=Release /p:Platform=x64
```

各プロジェクトの依存関係は異なるため、ソリューション全体がそのままビルドできることは保証していません。

## ファイルとフォルダの配置

| 配置 | 用途 |
|---|---|
| 各プロジェクトの`.cpp`・`.h`・`.vcxproj`・`.filters` | ソースとVisual Studioのプロジェクト設定 |
| a2d・d2z・r2zの`CMakeLists.txt`・`build.ps1` | bit7zの取得・ビルド。アプリのビルドに必要 |
| `test-*.ps1`・`RarFixture.cs` | 回帰テストとRARテストデータ生成。アプリの実行時には不要 |
| 各プロジェクトの`out/` | CMakeキャッシュ、取得した依存ソース、コンパイル中間ファイル。Git対象外 |
| ルートの`x64/Release`・`x64/Debug`など | 共通の実行ファイル出力先。Git対象外 |
| `packages/libwebp-1.6.0-windows-x64`・`packages/libjpeg-turbo64` | wpのビルド・検証に使う同梱ライブラリ |
| `.vs/`・`*.vcxproj.user` | Visual Studioのローカル設定。Git対象外 |

中間ファイルは`out/obj/<Platform>/<Configuration>`へ統一しています。a2d・d2z・r2zのCMake生成物は`out/build/<Platform>`に置きます。`out/`を削除した後の初回ビルドでは依存ライブラリを再取得するため、ネット接続が必要です。実行ファイルの隣の`.lst`・`.pref`は利用者の設定なので、出力フォルダの掃除時には残してください。

a2d・d2z・r2zの`build.ps1`を直接実行し、`-OutputDirectory`を省略した場合の実行ファイルは、各プロジェクトの`out/build/<Platform>/<Configuration>`にあります。Visual Studio経由では共通出力先にもコピーします。各ツールのビルドで7z.dllを自動取得・配置する処理はありません。

## 回帰テストと詳細資料

a2d・d2z・fs・r2z・rh・wpには、テスト用のコピーや生成ファイルを使うPowerShellスクリプトがあります。`-Exe`にビルド済み実行ファイルのパスを指定します。

```powershell
powershell -File rh\test-rh.ps1 -Exe "C:\build\rh.exe"
```

| 対象 | 詳細資料 | テスト |
|---|---|---|
| a2d | [README](a2d/README.md) | [test-a2d.ps1](a2d/test-a2d.ps1) |
| d2z | [README](d2z/README.md) | [test-d2z.ps1](d2z/test-d2z.ps1) |
| fs | [README](fs/README.md) | [test-fs.ps1](fs/test-fs.ps1) |
| r2z | [README](r2z/README.md) | [test-r2z.ps1](r2z/test-r2z.ps1) |
| rh | [README](rh/README.md) | [test-rh.ps1](rh/test-rh.ps1) |
| wp | [README](wp/README.md) | [test-wp.ps1](wp/test-wp.ps1) |
| rb | [README](rb/README.md) | 専用スクリプトなし |
| renNFD | [README](renNFD/README.md) | 専用スクリプトなし |
| ThumFromZip | [README](ThumFromZip/README.md) | 手動検証記録あり。専用スクリプトなし |

これら6プロジェクトの修正・検証結果を、ほかのプロジェクトにそのまま適用できるわけではありません。ファイルの移動・リネーム・削除を行うツールは、まずコピーしたデータで動作を確認してください。

## ライセンス・作者

本リポジトリは[MIT License](LICENSE)です。同梱ライブラリにはそれぞれのライセンスが適用されます。

bry-ful（Hiroshi Furuhashi） — [bryful](https://twitter.com/bryful)
