# renNFD — ファイル名・フォルダ名をNFCへ正規化

指定フォルダ内の全ファイル・全サブフォルダの名前を再帰的に正規化するWindows用ツールです。名前はrenNFDですが、変換先はNFC（合成形）です。7-Zipは使用しません。

```powershell
renNFD.exe "C:\Files" --dry-run
renNFD.exe "C:\Files"
```

- `か`＋結合濁点を`が`、`e`＋結合アクセントを`é`のように合成します。
- 波ダッシュ`〜`を全角チルダ`～`へ統一します。
- 拡張子を含む名前全体を処理します。拡張子の制限はありません。
- 指定フォルダ自身とファイルの内容は変更しません。
- `--dry-run`は変更予定だけを表示します。引数省略時は使用法を表示して終了コード1を返します。

## 現行実装の制約

同名衝突を事前に検出・回避する処理はありません。まず`--dry-run`で予定を確認してください。個別リネームに失敗しても処理件数へ加算され、終了コードは0になります。表示される`[DONE]`もリネーム前の表示で、成功の保証にはなりません。

引数は狭い文字列で受け取るため、実行環境のコードページで表現できない入力パスには制約があります。コード内の使用法表示は`renNFC.exe`ですが、実行ファイル名は`renNFD.exe`です。専用回帰テストはありません。

## ビルド・配置

リポジトリのルートから、Visual Studioのx64 Native Tools環境で実行します。MSVC v145、Windows SDK、Unicode正規化APIを使用します。

```powershell
msbuild renNFD\renNFD.vcxproj /p:Configuration=Release /p:Platform=x64
```

出力は`x64/Release/renNFD.exe`、中間ファイルは`renNFD/out/obj/x64/Release`です。Debugは`Debug`へ読み替えます。出力・中間ファイル・個人設定はGit対象外です。

[ソリューション全体の説明](../README.md)
