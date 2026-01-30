#include <iostream>
#include <string>
#include <filesystem>
#include <vector>
#include <algorithm>
#include <windows.h>

// Windowsの正規化ライブラリをリンク
#pragma comment(lib, "Normaliz.lib")

namespace fs = std::filesystem;

// NFDからNFCへ正規化
std::wstring NormalizeToNFC(const std::wstring& input) {
    if (input.empty()) return input;
    int size = NormalizeString(NormalizationC, input.c_str(), -1, nullptr, 0);
    if (size <= 0) return input;

    std::wstring result(size, L'\0');
    if (NormalizeString(NormalizationC, input.c_str(), -1, &result[0], size) > 0) {
        result.resize(wcslen(result.c_str()));
        return result;
    }
    return input;
}

// 波ダッシュ(0x301C)を全角チルダ(0xFF5E)に置換
std::wstring FixWaveDash(std::wstring str) {
    for (auto& c : str) {
        if (c == 0x301C) c = 0xFF5E;
    }
    return str;
}

int main(int argc, char* argv[]) {
    setlocale(LC_ALL, "japanese");

    bool dryRun = false;
    std::vector<std::string> args;
    for (int i = 1; i < argc; ++i) args.push_back(argv[i]);

    // 引数チェック
    auto it = std::find(args.begin(), args.end(), "--dry-run");
    if (it != args.end()) {
        dryRun = true;
        args.erase(it);
    }

    if (args.empty()) {
        std::wcout << L"使用法: renNFC.exe <ディレクトリパス> [--dry-run]" << std::endl;
        std::wcout << L"  --dry-run : 実際のリネームは行わず、変更内容のみ表示します。" << std::endl;
        return 1;
    }

    fs::path targetDir = args[0];
    if (!fs::exists(targetDir) || !fs::is_directory(targetDir)) {
        std::wcerr << L"エラー: 有効なディレクトリを指定してください。" << std::endl;
        return 1;
    }

    if (dryRun) {
        std::wcout << L"--- DRY RUN MODE (NO CHANGES WILL BE MADE) ---" << std::endl;
    }

    std::vector<fs::directory_entry> items;
    for (const auto& entry : fs::recursive_directory_iterator(targetDir)) {
        items.push_back(entry);
    }

    // 深い階層から順にソート
    std::sort(items.begin(), items.end(), [](const fs::directory_entry& a, const fs::directory_entry& b) {
        return a.path().wstring().length() > b.path().wstring().length();
        });

    int count = 0;
    for (const auto& entry : items) {
        std::wstring oldName = entry.path().filename().wstring();
        std::wstring newName = FixWaveDash(NormalizeToNFC(oldName));

        if (oldName != newName) {
            fs::path newPath = entry.path();
            newPath.replace_filename(newName);

            std::wcout << (dryRun ? L"[PLAN] " : L"[DONE] ")
                << oldName << L" -> " << newName << std::endl;

            if (!dryRun) {
                try {
                    fs::rename(entry.path(), newPath);
                }
                catch (const fs::filesystem_error& e) {
                    std::wcerr << L"  Error: " << e.what() << std::endl;
                }
            }
            count++;
        }
    }

    std::wcout << L"-------------------------------------------" << std::endl;
    std::wcout << L"処理件数: " << count << L" 件" << std::endl;

    return 0;
}