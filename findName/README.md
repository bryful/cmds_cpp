# findName

指定ディレクトリ以下を再帰的に探索し、ファイル名に指定したすべての検索文字列を含むファイルのフルパスを1行ずつ表示します。

```text
findName <探索パス> <検索文字列> [検索文字列...] [-e <拡張子>] [-bom]
```

```powershell
findName "D:\Archive" sample
findName "D:\Archive" sample backup
findName "D:\Documents" sample backup -e txt
findName "D:\Archive" "project backup" -e .zip
findName --help
```

- 検索文字列はAND検索。指定したすべての文字列を含むファイルが一致します。拡張子を含むファイル名だけを検索し、親ディレクトリ名は対象外です。
- ファイル名・拡張子の比較では大文字・小文字を区別しません。日本語のパス・検索文字列にも対応します。
- 拡張子は省略時 `.zip`。`-e txt` と `-e .txt` の両方を受け付けます。指定できる拡張子は1つです。
- 空白を含む引数は引用符で囲みます。`-` で始まる検索文字列は `--` の後に指定してください（拡張子指定はその前に置きます）。
- 同じファイルが複数の検索文字列に一致しても1回だけ表示します。出力順序は未指定です。
- ディレクトリのシンボリックリンクやジャンクションはたどりません。
- 読み取りに失敗した場合は標準エラーに通知し、探索可能な残りのディレクトリを続けます。
- 標準出力・標準エラーは、コンソールではUnicode、ファイルやパイプにはUTF-8（BOMなし）で直接書き込みます。Windows PowerShell 5.1の `>` はシェル側でUTF-16に再変換されます。UTF-8で保存する場合は `findName ... | Out-File -Encoding utf8 result.txt` を使用してください。
- `-bom` を指定すると、ファイル・パイプへの標準出力の先頭にUTF-8 BOM（`EF BB BF`）を1回付けます。省略時はBOMなしです。一致なしでもBOMの3バイトを出力します。コンソール表示と標準エラーにはBOMを付けません。`-e` と併用でき、オプションは `--` より前に指定します。
- 終了コード: `0` 正常終了（一致なしを含む）、`1` 探索・出力エラー、`2` 引数エラー。

## AND検索の例

```powershell
findName "D:\Archive" sample backup
```

`SampleBackup.zip` や `backup_sample.ZIP` は一致します。`Sample.zip` と `Backup.zip` は、それぞれ一方の文字列しか含まないため一致しません。文字列の順番は問いません。

## 結果をファイルに保存

コマンドプロンプトでは次のようにUTF-8（BOMなし）で保存できます。

```cmd
findName "D:\Archive" sample backup > result.txt
```

BOM付きUTF-8で保存する場合（コマンドプロンプト）:

```cmd
findName "D:\Archive" sample backup -bom > result.txt
```

Windows PowerShell 5.1でUTF-8を指定して保存する場合:

```powershell
findName "D:\Archive" sample backup | Out-File -Encoding utf8 result.txt
```

Windows PowerShell 5.1の `Out-File -Encoding utf8` はBOM付きUTF-8になります。一致するファイルがない場合、検索結果は空です（`-bom` 指定時はBOMのみ）。エラーは標準エラーに出力します。

## ビルド

Visual Studioで `cmds_cpp.slnx` を開き、`findName` をビルドします。外部ライブラリは不要です。出力先はVisual Studioの既定値を使用します。

- 実行ファイル（ソリューションからビルド）: `$(SolutionDir)x64\Release\findName.exe`
- 中間ファイル: `$(ProjectDir)x64\Release\`
- Debugでは `Release` が `Debug` になります。Win32では `x64\` は付きません。
- プロジェクト単体でビルドし、`SolutionDir` が未指定の場合は、実行ファイルもプロジェクト配下に出力されます。

```powershell
msbuild cmds_cpp.slnx /t:findName /p:Configuration=Release /p:Platform=x64 /p:PreferredToolArchitecture=x64
```

環境内の `MSPDB140.DLL` 不整合でリンクに失敗する場合は、次の設定でデバッグ情報とプログラム全体の最適化を無効にできます。

```powershell
msbuild cmds_cpp.slnx /t:findName /p:Configuration=Release /p:Platform=x64 /p:FindNameDisablePdb=true /p:WholeProgramOptimization=false /p:PreferredToolArchitecture=x64
```

## 動作確認

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File findName\test-findName.ps1 -Exe ".\x64\Release\findName.exe"
```

再帰探索、AND検索、重複抑制、既定・指定拡張子、日本語、英字とアクセント付き文字の大小文字比較、親ディレクトリ名の除外、引数エラー、ヘルプ、cmd.exeとPowerShellのファイルリダイレクト、標準エラーのリダイレクト、BOMの先頭バイト・単一出力・一致なし・拡張子指定との併用を検証します。テストデータは `findName/out/` 内に作成します。

2026-10-11にRelease/x64をビルドし、全21項目の動作確認に成功しています。
