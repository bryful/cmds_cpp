# rb — 角括弧内の表記を短縮

`[aaa(bbb)]作品.zip`を`[bbb]作品.zip`へリネームするWindows用ツールです。7-Zipは使用しません。

```powershell
rb.exe "C:\Downloads"
rb.exe "C:\Downloads\[aaa(bbb)]作品.zip"
```

引数省略時は作業フォルダを対象にします。フォルダ指定では直下のサブフォルダと`.zip .rar .7z .mp4 .mov .mpg .mpeg`を処理し、拡張子の大文字・小文字は区別しません。ファイル単体も同じ対応拡張子に限ります。再帰処理は行いません。

名前をNFC正規化してから角括弧のパターンを置き換えます。ただしパターンの置換がない場合、正規化だけのリネームは行いません。同名があれば拡張子の前に`_1`、`_2`などを付けます。対象フォルダ（単一ファイル指定では親フォルダ）へ`logYYYYMMDD_HHMM.txt`を追記します。ファイル内容は変更しません。

## 現行実装の制約

rhの衝突保護・リパースポイント除外・失敗集計の修正は未反映です。個別リネームの失敗は終了コードへ反映されず、終了コード0だけでは全件成功を判断できません。ログファイルの文字コードは実行環境のロケールに依存し、標準出力のリダイレクトはUTF-16です。専用回帰テストはありません。

## ビルド・配置

リポジトリのルートから、Visual Studioのx64 Native Tools環境で実行します。x64はMSVC v145、Win32はv143の設定です。Windows SDKとUnicode正規化APIを使用します。

```powershell
msbuild rb\rb.vcxproj /p:Configuration=Release /p:Platform=x64
```

出力は`x64/Release/rb.exe`、中間ファイルは`rb/out/obj/x64/Release`です。Debugは`Debug`へ読み替えます。出力・中間ファイル・個人設定はGit対象外です。実行時に作成される変更ログは利用者のデータです。

[ソリューション全体の説明](../README.md)
