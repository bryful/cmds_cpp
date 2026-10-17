# 開発

Visual Studio 2026、MSVC v145、Windows SDK、C++20を使用します。WICでPNGをデコードし、乗算済みアルファのRGBを黒背景としてJPEGへ書き込みます。JPEGのImageQualityは0.90です。画像は行単位で処理します。

```powershell
msbuild cmds_cpp.slnx /t:png2jpeg /p:Configuration=Debug /p:Platform=x64 /p:PreferredToolArchitecture=x64
msbuild cmds_cpp.slnx /t:png2jpeg /p:Configuration=Release /p:Platform=x64 /p:PreferredToolArchitecture=x64
```

`cmds_cpp.slnx` を開いてビルドします。中間・出力ディレクトリはVisual Studioの既定値を使用します。

- 実行ファイル: `x64/<Configuration>/png2jpeg.exe`
- 中間ファイル: `png2jpeg/x64/<Configuration>/`

プロジェクト単体でのMSBuild実行はソリューションの場所が異なる扱いになるため、上記のソリューション経由のコマンドを使用してください。

一時JPEGを同じディレクトリで完成させ、WICのファイルハンドルを解放してからMoveFileExWで正式名へ移動します。上書き指定がなければ、処理中に同名ファイルが作られた場合も上書きしません。移動成功後のみ元PNGを削除します。

回帰テストは `powershell -File png2jpeg/test-png2jpeg.ps1 -Exe <実行ファイルの絶対パス>` で実行します。生成したテスト用PNGのみを使用します。
