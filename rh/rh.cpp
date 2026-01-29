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

// Unicode正規化関数（wpと同じ）
std::wstring NormalizePathUnicode(const std::wstring& path) {
    int len = NormalizeString(NormalizationC, path.c_str(), -1, NULL, 0);
    if (len <= 0) return path;
    std::wstring normalized(len, 0);
    NormalizeString(NormalizationC, path.c_str(), -1, &normalized[0], len);
    // 末尾のnull文字を削除
    if (!normalized.empty() && normalized.back() == L'\0') {
        normalized.pop_back();
    }
    return normalized;
}

std::string toLowwer(std::string s)
{
    std::transform(s.begin(), s.end(), s.begin(),
        [](unsigned char c) { return std::tolower(c); });
    return s;
}

std::wstring toLowwer(std::wstring s)
{
    std::transform(s.begin(), s.end(), s.begin(),
        [](wchar_t c) { return std::towlower(c); });
    return s;
}

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
        if (!entry.is_regular_file()) continue;
        
        // ★パスを正規化
        std::wstring normalized = NormalizePathUnicode(entry.path().wstring());
        fs::path normalizedPath(normalized);
        
        // ★拡張子を取得してnull文字を削除
        std::wstring ex = normalizedPath.extension().wstring();
        ex.erase(std::remove(ex.begin(), ex.end(), L'\0'), ex.end());
        ex = toLowwer(ex);
        
        if (ex == L".zip" || ex == L".rar") {
            files.push_back(normalizedPath);
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

