# ThumFromZip — ZIPから代表画像を抽出

フォルダ以下のZIPを再帰的に検索し、各ZIPからJPG/JPEG/PNGを1枚抽出するWindows用ツールです。画像の縮小・再エンコードは行いません。7z.dllを直接利用し、bit7zや7z.exeは使用しません。

## 使い方

```powershell
ThumFromZip.exe "C:\Images"
ThumFromZip.exe --overwrite "C:\Images"
ThumFromZip.exe --help
```

- 引数省略時は現在の作業フォルダを検索します。フォルダは1つ指定できます。
- `--overwrite`または`-f`で上書きを許可します。`--`以降はフォルダ名として扱います。
- ZIPの拡張子は大文字・小文字を区別しません。
- 対応画像のファイル名をASCIIの大文字・小文字を区別せず比較し、先頭になる画像を選びます。同名ならZIP内のフルパスで比較します。数値順やZIPへの登録順ではありません。
- 出力はZIPの隣に`ZIP名.jpeg`または`ZIP名.png`として作成します。ZIP内のフォルダ構造は出力しません。
- 元ZIPは保持します。画像がないZIPはスキップします。

## 既存画像と失敗時の動作

通常はZIPと同名の`.jpg`・`.jpeg`・`.png`のいずれかがあればスキップします。上書きモードでは選択した画像の拡張子に対応する出力だけを置き換えます。別の拡張子の既存画像は削除しません。

上書き時は一時ファイルへの抽出が成功してから正式名へ移動します。通常モードでは正式名へ直接書き込み、検出した抽出失敗時に削除します。強制終了時には不完全な出力や一時ファイルが残る場合があります。

対象ZIP・選択画像・処理結果と集計を表示します。コンソールへはUnicode、リダイレクト時はUTF-8で出力します。処理エラーがあっても後続ZIPを続け、エラーが1件以上あれば終了コード1、それ以外は0を返します。パスワード入力には対応していません。

## 7z.dllの配置

実行ファイルと同じアーキテクチャのDLLが必要です。次の順に検索します。

1. 実行ファイルの隣の`7z.dll`
2. 隣にDLLがない場合、`%ProgramFiles%\7-Zip\7z.dll`

隣に存在するDLLが読み込めない場合はエラーにします。`--dll`や`.pref`指定はありません。DLLは複数ZIPの処理で再利用します。

## ビルド・配置

リポジトリのルートから、VS2026のx64 Native Tools環境で実行します。MSVC v145とWindows SDKが必要です。SDKヘッダーは`third_party/7zip`に同梱しています。ビルド時のネット接続は不要で、実行時のDLLは別途必要です。

```powershell
msbuild ThumFromZip\ThumFromZip.vcxproj /p:Configuration=Release /p:Platform=x64
```

出力は`x64/Release/ThumFromZip.exe`、中間ファイルは`ThumFromZip/out/obj/x64/Release`です。Debugは`Debug`へ読み替えます。Releaseは`/MT`、Debugは`/MTd`を使用します。出力・中間ファイル・個人設定はGit対象外です。

## 検証記録（2026-10-09）

VS2026のRelease・Debug x64ビルドを確認しました。Releaseでインストール済みDLLへのフォールバック、UTF-8の日本語ログ、抽出内容、日本語パス、再帰検索、既存画像保護、上書き、画像なしZIP、破損ZIPのエラー終了を確認しました。専用の回帰テストスクリプトはまだありません。

7-ZipのDLL・SDKには各配布物のライセンスが適用されます。

[ソリューション全体の説明](../README.md)
