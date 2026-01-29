#include <iostream>
#include <string>
#include <filesystem>
#include <algorithm>
#include <vector>
#include <Windows.h>
#include <io.h>
#include <fcntl.h>
#include "fsort.h"

namespace fs = std::filesystem;



void Usage() {
    std::wcout << L"Usage: fs <path>" << std::endl;
}

int wmain(int argc, wchar_t* argv[]) {
    // wcout でワイド文字を正しく出力するための設定
    _setmode(_fileno(stdout), _O_U16TEXT);

    std::wstring targetPath;
    if (argc < 2) {
        targetPath = fs::current_path().wstring();
    } else if (argc == 2) {
        targetPath = argv[1];
    } else if (argc > 2) {
        std::wstring op = argv[1];
        std::transform(op.begin(), op.end(), op.begin(),
            [](wchar_t c) { return ::towlower(c); });

        if (op == L"-r" || op == L"-rev" || op == L"/r" || op == L"/rev") {
            targetPath = argv[2];
            // FsortDir が std::string を受け取る場合は UTF-8 に変換
            std::string targetPathUtf8 = WStringToUtf8(targetPath);
            std::string res = FsortDir(targetPathUtf8);
            // 結果が UTF-8 の場合は wstring に変換して出力
            std::wcout << Utf8ToWString(res) << std::endl;
            return 0;
        } else {
            Usage();
            return 1;
        }
    }

    if (!fs::exists(targetPath) || !fs::is_directory(targetPath)) {
        std::wcout << L"Error: Path not found." << std::endl;
        Usage();
        return 1;
    }

    bool found = false;
    for (const auto& entry : fs::directory_iterator(targetPath)) {
        if (!entry.is_regular_file()) continue;

        try {
            // ★entry.path()から直接wstringを取得
            std::wstring pathWStr = entry.path().wstring();
            
            // ★パスを正規化 - 正規化は不要なので削除
            // std::wstring normalized = NormalizePathUnicode(pathWStr);
            // fs::path normalizedPath(normalized);

            // ★pathをUTF-8 stringに変換
            std::string pathUtf8 = WStringToUtf8(pathWStr);

            found = true;
            // FSort が std::string を受け取る場合は UTF-8 を渡す
            std::string res = FSort(pathUtf8);
            // 結果が UTF-8 の場合は wstring に変換して出力
            std::wcout << Utf8ToWString(res) << std::endl;
        }
        catch (const std::exception& e) {
            std::wcerr << L"Error processing file: " << Utf8ToWString(e.what()) << std::endl;
        }
    }

    if (!found) {
        std::wcout << L"No files found." << std::endl;
    }
    return 0;
}
