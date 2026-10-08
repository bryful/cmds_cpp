# Windowsコマンドツール集

ファイル整理、リネーム、アーカイブ変換、画像変換のためのC++製コマンドラインツールです。ソリューションは`cmds_cpp.slnx`です。

## プロジェクト一覧

| プロジェクト | 用途 | 外部ツール・主なライブラリ |
|---|---|---|
| a2d | ZIP/RARをフォルダへ展開 | 7z.exe |
| a2dir | ソリューションに登録あり。現在のチェックアウトにはソース・プロジェクトファイルなし | 未確認 |
| d2z | フォルダの中身をZIPへ圧縮 | 7z.exe |
| Dir2z | フォルダの中身をZIPへ圧縮するbit7z版 | bit7z、7z.dll |
| r2z | RARを展開してZIPへ再圧縮 | 7z.exe |
| r2zip | RAR→ZIP変換のbit7z版 | bit7z、7z.dll |
| fs | 名前の角括弧を使ったファイル振り分け・移動 | なし |
| rh | 削除語・全角半角変換による名前の整理 | Windows Unicode正規化API |
| rb | `[aaa(bbb)]`を`[bbb]`へ変更 | Windows Unicode正規化API |
| renNFD | ファイル名・フォルダ名をNFCへ正規化 | Windows Unicode正規化API |
| RenSeq | ファイル名末尾の番号を保持して接頭辞を変更 | なし |
| wp | WebPをJPEGへ変換 | libwebp、libjpeg-turbo |
| xmlv | XMLの要素・属性・テキストを表示 | pugixml |

## アーカイブ関連

### a2d — ZIP/RARの展開

```powershell
a2d.exe "archive.zip" "data.rar"
```

複数のZIP/RARを順に展開します。出力先はアーカイブと同じ場所にある、拡張子を除いた名前のフォルダです。展開結果が単一ルートフォルダなら、その中身を出力先へ配置し、二重フォルダを避けます。

- 全階層の`.scr`・`.SCR`を除外します。
- 一時フォルダへの展開が成功してから正式な出力先へ移動します。
- 既存の出力先には結合・上書きしません。
- 元アーカイブは保持します。失敗があれば終了コード1を返します。

詳細: [a2d/README.md](a2d/README.md)

### d2z — フォルダのZIP圧縮

```powershell
d2z.exe "C:\Images" "C:\Documents"
d2z.exe --fast "C:\Images"
d2z.exe --store "C:\Videos"
```

フォルダの隣に`フォルダ名.zip`を作成します。格納パスは圧縮対象からの相対パスです。入力直下にサブフォルダが1つだけあり、ほかに項目がなければ、そのサブフォルダの中身を圧縮します。

- 通常実行では7-Zipの既定圧縮設定を使います。
- `--fast`は高速圧縮、`--store`は圧縮なしのZIP作成です。
- 新しい一時ZIPを作成し、成功後に確定します。既存ZIPは更新・上書きしません。
- 元フォルダは保持します。失敗があれば終了コード1を返します。

詳細: [d2z/README.md](d2z/README.md)

### Dir2z — bit7zを使うZIP圧縮

```powershell
Dir2z.exe "C:\Images" "C:\Documents"
```

`7z.exe`を起動せず、bit7zを通じて`7z.dll`で圧縮します。指定フォルダの中身を格納し、進捗を表示します。既存の出力ZIPはスキップします。

d2zとは別実装です。出力名は入力パスの拡張子を`.zip`へ置換して作るため、`Images.v1`というフォルダは`Images.zip`になります。d2zの高速モードや一時ZIPによる確定処理は、このプロジェクトには実装していません。

### r2z — 7z.exeを使うRAR→ZIP変換

```powershell
r2z.exe "archive.rar" "another.rar"
```

RARを一時フォルダに展開し、同じ場所へZIPを作成します。展開結果が単一サブフォルダだけなら、その中身を再圧縮します。元RARは保持し、一時展開フォルダを削除します。

`.scr`を検出して除外する処理がありますが、a2dで修正した再帰除外や、d2zの既存ZIP保護は未反映です。既存ZIPがある場合は追加更新になるため、出力先を確認して使用してください。

### r2zip — bit7zを使うRAR→ZIP変換

```powershell
r2zip.exe "archive.rar" "another.rar"
```

bit7zと`7z.dll`でRARを展開し、`.scr`を除外するフィルタを指定してZIPへ再圧縮します。処理中は元RARを`processing_target.tmp`へ一時リネームし、成功後に元の名前へ戻します。

作業フォルダ名は`T_EXTRACT_WORK`で固定です。同じ場所の既存作業ファイル・フォルダを削除する実装があるため、同じ場所での同時実行は避けてください。a2d・d2zとは安全性や失敗時の動作が異なります。

### a2dir — ソース未確認

`cmds_cpp.slnx`には`a2dir/a2dir.vcxproj`の登録がありますが、現在のチェックアウトには該当フォルダがありません。実装内容は確認できず、この参照を含むソリューション全体のビルドには、プロジェクトの復元または参照の整理が必要です。

## ファイル整理・リネーム

### fs — 角括弧による振り分け

```powershell
fs.exe "C:\Downloads"
fs.exe -r "C:\Downloads"
```

通常モードは、対象フォルダ直下のZIP/RARについて、名前の最初の`[...]`内の文字列をフォルダ名として振り分けます。

例: `[作者名]作品.zip` → `作者名\[作者名]作品.zip`

`-r`は直下の各サブフォルダにあるファイルを1階層上へ移動し、移動後に空になったフォルダを削除します。逆モードではZIP/RAR以外のファイルも対象です。引数を省略すると通常モードで作業フォルダを処理します。

### rh — 名前の整理と削除語の適用

```powershell
rh.exe "C:\Downloads"
```

フォルダ直下のサブフォルダと、`.zip .rar .7z .mp4 .mov .mpg .mpeg`のファイル名を整理します。NFC正規化、全角英数字・括弧などの変換、削除語の除去、空白の整理を行います。ファイルの拡張子は保持します。

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

### RenSeq — 末尾の番号を保持して名前を変更

```powershell
RenSeq.exe "AAA_0001.tga" "BBB_"
RenSeq.exe -d "C:\Frames" ".tga" "NewName_"
RenSeq.exe -d "C:\Frames" "*" "NewName_"
```

拡張子の前にある末尾の数字とゼロ埋めを維持し、それより前の部分を指定した接頭辞へ置換します。

例: `AAA_0001.tga` → `BBB_0001.tga`

連番を新しく振り直すツールではありません。末尾に番号がないファイルは処理せず、出力名が既に存在する場合も変更しません。`-d`はフォルダ直下のファイルを拡張子で選び、自然順で処理します。拡張子はワイルドカード式ではなく`.tga`などの一致指定か、全対象の`*`です。

## 画像・XML

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

### xmlv — XML構造の表示

```powershell
xmlv.exe
```

作業フォルダにある`data.xml`をpugixmlで読み込み、要素名・属性・空白だけではないテキストを階層に応じた字下げで表示します。現在は入力ファイル名が固定で、コマンドライン引数による指定には対応していません。

## 実行環境・依存関係

- Windows向けです。現在の画像ライブラリ構成と検証はx64を中心としています。
- a2d・d2z・r2zは`7z.exe`を使用します。既定パスは`C:\Program Files\7-Zip\7z.exe`です。
- Dir2z・r2zipはbit7zと、読み込み可能な場所にある`7z.dll`が必要です。`7z.exe`の設定は使いません。
- wpは同梱の`libwebp-1.6.0-windows-x64`と`libjpeg-turbo64`を参照します。
- rh・rb・renNFDなどの名前変更ツールは7-Zipを使用しません。
- C++ランタイムの要否は各プロジェクトのビルド設定によって異なります。

### 7z.exeの場所を変更する

実行ファイルと同じ場所に、実行ファイルと同名の`.pref`を配置します。対象は`a2d.pref`、`d2z.pref`、`r2z.pref`です。1行目に7z.exeのパスを記述します。

例: `d2z.pref`

```text
D:\Tools\7-Zip\7z.exe
```

a2d・d2zはUTF-8（BOMあり・なし）と従来のWindowsコードページに対応しています。r2zの設定読み込みは別実装です。rhの削除語設定`rh.lst`とは用途が異なります。

## ビルド

Visual Studioの「C++によるデスクトップ開発」とWindows SDKが必要です。プロジェクトにはMSVCツールセット`v145`と一部`v143`の指定が混在しています。指定されたツールセットを用意するか、使用環境に合わせてリターゲットしてください。

1. `.slnx`形式に対応するVisual Studioで`cmds_cpp.slnx`を開きます。
2. a2dirの欠落した参照を復元するか、必要な個別プロジェクトを開きます。
3. 原則として`Release | x64`を選択します。
4. Dir2z・r2zipをビルドする場合はbit7zのヘッダ・リンク設定も用意します。

x64 Native Tools環境から個別にビルドする例:

```powershell
msbuild rh\rh.vcxproj /p:Configuration=Release /p:Platform=x64
```

各プロジェクトの依存関係は異なるため、ソリューション全体がそのままビルドできることは保証していません。

## 回帰テストと詳細資料

a2d・d2z・rh・wpには、テスト用のコピーや生成ファイルを使うPowerShellスクリプトがあります。`-Exe`にビルド済み実行ファイルのパスを指定します。

```powershell
powershell -File rh\test-rh.ps1 -Exe "C:\build\rh.exe"
```

| 対象 | 詳細資料 | テスト |
|---|---|---|
| a2d | [README](a2d/README.md) | [test-a2d.ps1](a2d/test-a2d.ps1) |
| d2z | [README](d2z/README.md) | [test-d2z.ps1](d2z/test-d2z.ps1) |
| rh | [README](rh/README.md) | [test-rh.ps1](rh/test-rh.ps1) |
| wp | [README](wp/README.md) | [test-wp.ps1](wp/test-wp.ps1) |

これら4プロジェクトの修正・検証結果を、ほかのプロジェクトにそのまま適用できるわけではありません。ファイルの移動・リネーム・削除を行うツールは、まずコピーしたデータで動作を確認してください。

## ライセンス・作者

本リポジトリは[MIT License](LICENSE)です。同梱ライブラリにはそれぞれのライセンスが適用されます。

bry-ful（Hiroshi Furuhashi） — [bryful](https://twitter.com/bryful)
