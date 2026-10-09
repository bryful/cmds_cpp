# r2z — RARからZIPへの変換

bit7z 4.1.0を通じて`7z.dll`を直接利用するWindows用ツールです。`7z.exe`の子プロセスは起動しません。RAR4/RAR5を展開し、元RARの隣に同名のZIPを作成します。

```powershell
r2z.exe "archive.rar" "another.RAR"
r2z.exe --fast "archive.rar"
r2z.exe --store "archive.rar"
r2z.exe --dll "D:\Tools\7-Zip\7z.dll" "archive.rar"
```

- 通常は標準圧縮、`--fast`は高速圧縮、`--store`は無圧縮です。DLLは一括処理につき1回だけ読み込みます。
- 全階層の`.scr`・`.SCR`を展開対象から除外します。除外後に項目がなければ失敗します。
- 展開結果が単一フォルダだけなら、その中身をZIPへ格納します。
- 元RARは変更しません。既存ZIPへの追加・上書きは行いません。
- アーカイブの隣に実行ごとに異なる一時フォルダを作り、ZIPの完成・書き込み完了後に正式名へ移動します。失敗時は一時データを削除します。削除に失敗した場合は場所を報告します。
- 失敗した入力があっても残りを処理し、1件以上の失敗で終了コード1を返します。
- 暗号化された保持対象項目、リンク、絶対パス・親参照・不正なパス、Windows上で大文字小文字だけが異なる重複名は拒否します。自己解凍RARは対象外です。分割RARは未検証です。

## 進捗表示

処理前に「何件目／全件数」と対象RARを表示し、展開・圧縮の割合をそれぞれ10%単位で表示します。小さな入力では途中の割合を飛ばす場合があります。割合を取得できない場合も、段階の開始を表示します。

```text
[1/3] Converting: archive.rar
  Extracting: starting
  Extracting: 100%
  Checking extracted files
  Compressing: starting
  Compressing: 100%
  Finalizing ZIP
Created: archive.zip, excluded .scr: 0
```

100%は各段階の処理量を示します。ZIPのフラッシュ・正式名への確定まで完了すると`Created:`を表示します。

## DLLの設定

実行ファイルとDLLのアーキテクチャを合わせてください。x64版にはx64の`7z.dll`が必要です。検索順序は次のとおりです。

1. `--dll`で明示したパス
2. 実行ファイルの隣の`r2z.pref`の1行目
3. 実行ファイルの隣の`7z.dll`
4. `C:\Program Files\7-Zip\7z.dll`

`r2z.pref`はUTF-8（BOMあり・なし）とWindowsコードページに対応します。明示した設定先が存在しなければエラーになります。旧設定の`7z.exe`パスは同じ場所の`7z.dll`へ読み替えます。

```text
D:\Tools\7-Zip\7z.dll
```

## ビルド

Visual StudioのC++デスクトップ開発、Windows SDK、CMake 3.24以上が必要です。初回はbit7zと7-Zip SDKの取得にネット接続が必要です。bit7zのバージョンとダウンロードのSHA-256をCMakeで固定しています。

```powershell
powershell -File r2z\build.ps1 -Configuration Release -Platform x64
```

出力は`r2z\out\build\x64\Release\r2z.exe`です。`r2z.vcxproj`は同じスクリプトを呼び出すMakefileプロジェクトで、ソリューションからもビルドできます。依存ライブラリを手動でリンクする必要はありません。実行時の`7z.dll`は別途配置してください。

この検証環境ではPDB生成時にMSPDB関連の問題があったため、`-DisablePdb`を指定してビルドしました。この指定はCMakeキャッシュに保存されます。通常の環境では不要です。

## コードレビューで修正した点

- RARごとの外部プロセス起動を廃止し、同じDLLインスタンスを再利用します。
- 展開前に項目情報を調べ、危険なパス・リンク・重複名を拒否します。再帰的な`.scr`除外も展開時に実施します。
- 固定の作業フォルダ、作業ディレクトリの変更、元RARのリネームを使いません。
- 既存ZIPを保護し、不完全な出力を正式名で残さないようにしました。
- 大文字の`.RAR`、日本語・空白を含む入力パス、失敗時の終了コードを修正しました。

## 検証・速度比較

```powershell
powershell -File r2z\test-r2z.ps1 -Exe r2z\out\build\x64\Release\r2z.exe
```

生成した真正な格納RAR4で10ケースを検証しています。内容一致・元RAR保持、単一ルート解除、再帰的SCR除外、既存ZIP保護、CRC不一致、不正入力を含む一括処理、全項目除外、親参照、重複名、圧縮モード、ロックされた入力を確認します（複数の確認を含むケースがあります）。別途、libarchiveの圧縮RAR4・RAR5・ソリッドRAR5の3フィクスチャで変換前後の全ファイルのSHA-256一致を確認しました。

2026-10-09、同一環境のRelease x64、7-Zip 19.00のexe/DLLで、小さな格納RAR（テキスト2ファイル）30個を一括変換し、3回の中央値を比較しました。

| 実装 | 中央値 |
|---|---:|
| 旧7z.exe版 | 2.36秒 |
| 新7z.dll版・通常圧縮 | 0.40秒 |

この条件では約5.9倍（所要時間約83%減）でした。小さなアーカイブではプロセス起動削減が効きます。大容量・高圧縮率のRARで同じ倍率になることは保証しません。展開・再圧縮自体は引き続き必要です。

## 依存ライブラリ

- [bit7z](https://github.com/rikyoz/bit7z): MPL-2.0。配布時はライセンス条件を確認してください。
- [7-Zip](https://www.7-zip.org/): DLLとSDKにはそれぞれのライセンスが適用されます。RAR展開部分にはunRARの制限があります。
- 実データ検証用フィクスチャ: [libarchive](https://github.com/libarchive/libarchive/tree/master/libarchive/test)（取得したフィクスチャは本リポジトリに同梱していません）。
