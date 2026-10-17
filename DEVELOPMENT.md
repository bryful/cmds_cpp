# 開発ガイド

## ビルド

Visual Studioの「C++によるデスクトップ開発」とWindows SDKが必要です。プロジェクトにはMSVCツールセット`v145`と一部`v143`の指定が混在しています。指定されたツールセットを用意するか、使用環境に合わせてリターゲットしてください。

1. `.slnx`形式に対応するVisual Studioで`cmds_cpp.slnx`を開きます。
2. 必要な個別プロジェクトを選択します。
3. 原則として`Release | x64`を選択します。
4. a2d・d2z・r2zはCMakeを呼び出して依存ライブラリを取得します。初回はネット接続が必要です。
5. ThumFromZipは同梱ヘッダーでビルドできます。Releaseは`/MT`、Debugは`/MTd`です。実行時の7z.dllは別途必要です。

x64 Native Tools環境から個別にビルドする例:

```powershell
msbuild cmds_cpp.slnx /t:rh /p:Configuration=Release /p:Platform=x64 /p:PreferredToolArchitecture=x64
```

各プロジェクトの依存関係は異なるため、ソリューション全体がそのままビルドできることは保証していません。

## ファイルとフォルダの配置

| 配置 | 用途 |
|---|---|
| 各プロジェクトの`.cpp`・`.h`・`.vcxproj`・`.filters` | ソースとVisual Studioのプロジェクト設定 |
| a2d・d2z・r2zの`CMakeLists.txt`・`build.ps1` | bit7zの取得・ビルド。アプリのビルドに必要 |
| `test-*.ps1`・`RarFixture.cs` | 回帰テストとRARテストデータ生成。アプリの実行時には不要 |
| 各プロジェクトの`x64/Debug`・`x64/Release` | 標準の中間ファイル。CMake使用プロジェクトは配下の`cmake/`に依存ソースとキャッシュも保存。Git対象外 |
| ルートの`x64/Release`・`x64/Debug`など | 共通の実行ファイル出力先。Git対象外 |
| `packages/libwebp-1.6.0-windows-x64`・`packages/libjpeg-turbo64` | wpのビルド・検証に使う同梱ライブラリ |
| `.vs/`・`*.vcxproj.user` | Visual Studioのローカル設定。Git対象外 |

全プロジェクトでOutDir・IntDirの独自指定を外し、Visual Studioの既定値を使用します。ソリューション経由のx64ビルドでは、中間ファイルは各プロジェクトの`x64/<Configuration>/`、実行ファイルはルートの`x64/<Configuration>/`です。a2d・d2z・r2zのCMake生成物は中間ディレクトリ配下の`cmake/`に保存します。既存の`out/`内のビルドキャッシュは使用しません。実行ファイルの隣の`.lst`・`.pref`は利用者の設定なので、出力フォルダの掃除時には残してください。

a2d・d2z・r2zの`build.ps1`を直接実行した場合も、既定では同じ配置を使用します。`-IntermediateDirectory`・`-OutputDirectory`はVisual StudioのIntDir・OutDirを受け渡すための引数です。各ツールのビルドで7z.dllを自動取得・配置する処理はありません。

## 回帰テストと詳細資料

a2d・d2z・findName・fs・r2z・rb・rh・wpには、テスト用のコピーや生成ファイルを使うPowerShellスクリプトがあります。`-Exe`にビルド済み実行ファイルのパスを指定します。

```powershell
powershell -File rh\test-rh.ps1 -Exe "C:\build\rh.exe"
```

| 対象 | 詳細資料 | テスト |
|---|---|---|
| a2d | [README](a2d/README.md) | [test-a2d.ps1](a2d/test-a2d.ps1) |
| d2z | [README](d2z/README.md) | [test-d2z.ps1](d2z/test-d2z.ps1) |
| findName | [README](findName/README.md) | [test-findName.ps1](findName/test-findName.ps1) |
| fs | [README](fs/README.md) | [test-fs.ps1](fs/test-fs.ps1) |
| r2z | [README](r2z/README.md) | [test-r2z.ps1](r2z/test-r2z.ps1) |
| rh | [README](rh/README.md) | [test-rh.ps1](rh/test-rh.ps1) |
| wp | [README](wp/README.md) | [test-wp.ps1](wp/test-wp.ps1) |
| rb | [README](rb/README.md) | [test-rb.ps1](rb/test-rb.ps1) |
| renNFD | [README](renNFD/README.md) | 専用スクリプトなし |
| ThumFromZip | [README](ThumFromZip/README.md) | 手動検証記録あり。専用スクリプトなし |

これら8プロジェクトの修正・検証結果を、ほかのプロジェクトにそのまま適用できるわけではありません。ファイルの移動・リネーム・削除を行うツールは、まずコピーしたデータで動作を確認してください。

