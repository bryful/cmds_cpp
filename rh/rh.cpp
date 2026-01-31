// rh.cpp : このファイルには 'main' 関数が含まれています。プログラム実行の開始と終了がそこで行われます。

#include <iostream>
#include <vector>
#include <string>
#include <filesystem>
#include <algorithm>
#include "HeaderRename.h"
#include <cwctype>
#include <locale>
#include <codecvt>
#include <Windows.h>
#include <io.h>
#include <fcntl.h>

namespace fs = std::filesystem;



// wstring → UTF-8 string 変換関数（Windows専用）
std::string WStringToUtf8(const std::wstring& wstr) {
    if (wstr.empty()) return "";
    int size_needed = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), (int)wstr.size(), NULL, 0, NULL, NULL);
    std::string str(size_needed, 0);
    WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), (int)wstr.size(), &str[0], size_needed, NULL, NULL);
    return str;
}

void Usage() {
    std::wcout << L"Usage: rh <path>" << std::endl;
}

int wmain(int argc, wchar_t* argv[])
{
    // wcout でワイド文字を正しく出力するための設定
    _setmode(_fileno(stdout), _O_U16TEXT);

    HeaderRename headerRename;

    // exeパス取得
    std::wstring exePath = std::filesystem::path(argv[0]).wstring();

    // .lstファイル名生成
    std::wstring lstf = exePath;
    size_t dot = lstf.find_last_of(L'.');
    if (dot != std::wstring::npos) {
        lstf = lstf.substr(0, dot) + L".lst";
    } else {
        lstf += L".lst";
    }

    if (!headerRename.LoadWords(lstf)) {
        headerRename.SaveWords(lstf);
    }

    // 対象パス取得
    std::wstring targetPath;
    if (argc < 2) {
        targetPath = fs::current_path().wstring();
    } else {
        targetPath = argv[1];
    }
    if (!fs::exists(targetPath) || !fs::is_directory(targetPath)) {
        std::wcout << L"Error: Path not found." << std::endl;
        Usage();
        return 1;
    }

    // ディレクトリ内のファイル列挙
    std::filesystem::path target = std::filesystem::path(targetPath);
    std::vector<std::filesystem::path> files;
    
    for (const auto& entry : std::filesystem::directory_iterator(target)) {
        // ★ここでは正規化（NormalizePathUnicode）を行わない！
        // entry.path() は OS が見つけた「生」の状態を維持している

        if (entry.is_regular_file()) {
            std::wstring ex = toLowwer(entry.path().extension().wstring());
            // null除去
            ex.erase(std::remove(ex.begin(), ex.end(), L'\0'), ex.end());

            if (ex == L".zip" || ex == L".rar") {
                files.push_back(entry.path()); // 生のパスを保存
            }
        }
        else if (entry.is_directory()) {
            files.push_back(entry.path()); // 生のパスを保存
        }
    }
    if (files.empty()) {
        std::wcout << L"No files found." << std::endl;
        return 0;
    }

    for (const auto& file : files) {
        try {
                headerRename.Rename(file);
        } catch (const std::filesystem::filesystem_error& e) {
            std::wcerr << L"ファイルパス変換エラー: " << e.what() << std::endl;
        }
    }
    
    return 0;
}

