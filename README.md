# コマンド集

このリポジトリは複数のコマンドツールをまとめたコレクションです。

## コマンド一覧

### d2z - Directory to Zip
ディレクトリをZIPファイルに圧縮するツール

**使用例:**
```bash
d2z.exe "C:\MyFolder"
```

**機能:**
- 指定ディレクトリを同名のZIPファイルに圧縮
- 7-Zipを使用した高圧縮
- 複数のディレクトリを一括処理可能

---

### a2d - Archive to Directory
アーカイブ（ZIP/RAR）をディレクトリに展開するツール

**使用例:**
```bash
a2d.exe "archive.zip" "data.rar"
```

**機能:**
- ZIP/RARファイルを展開
- 複数のアーカイブを一括処理可能

---

### r2z - RAR to Zip
RARファイルをZIPファイルに変換するツール

**使用例:**
```bash
r2z.exe "archive.rar"
```

**機能:**
- RARをZIPに変換
- 一時ディレクトリを自動削除

---

### fs - File Sort
ファイルやディレクトリを整理・ソートするツール<br>
[xxx]aaa.zipというファイルがあれば"xxx"というフォルダを作成し、その中にaaa.zipを移動させるなどの整理を行います。<br>
-r オプションで逆の動作（フォルダ内のファイルを親ディレクトリに移動）も可能です。


**使用例:**
```bash
fs.exe "C:\Downloads"
```

---

### rh - Rename Header
ファイル名や拡張子を機寝られた規則で一括リネームするツール

**使用例:**
```bash
rh.exe "C:\Photos"
```

**機能:**
- ファイル名の一括変更

---

### wp - WebP Converter
WebP画像をJPEG形式に変換するツール

**使用例:**
```bash
wp.exe "image.webp"
```

**機能:**
- WebP → JPEG変換
- 一括変換対応

---

## システム要件

- **OS:** Windows (x64推奨)
- **7-Zip:** アーカイブ関連ツール（d2z/a2d/r2z）に必要
- **C++ランタイム:** Visual C++ 2022以降

## ビルド方法

Visual Studio 2022でソリューションを開き、各プロジェクトをビルドしてください。

## 設定ファイル

アーカイブ関連ツール（d2z/a2d/r2z）では、実行ファイルと同じディレクトリに `.pref` ファイルを作成することで、7z.exeのパスをカスタマイズできます。

**例: d2z.pref**
```
D:\Tools\7-Zip\7z.exe
```

デフォルトは `C:\Program Files\7-Zip\7z.exe` です。

## ライセンス

個人利用・商用利用ともに自由に使用できます。


## Authors

bry-ful(Hiroshi Furuhashi)<br>
twitter:[bryful](https://twitter.com/bryful)<br>

