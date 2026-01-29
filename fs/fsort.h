#pragma once
// C# FileSortクラスのC++17移植版
#include <iostream>
#include <filesystem>
#include <regex>
#include <string>
#include <algorithm>
#include <Windows.h>

namespace fs = std::filesystem;

// wstring → UTF-8 string 変換関数（Windows専用）
inline std::string WStringToUtf8_fsort(const std::wstring& wstr) {
    if (wstr.empty()) return "";
    int size_needed = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), (int)wstr.size(), NULL, 0, NULL, NULL);
    std::string str(size_needed, 0);
    WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), (int)wstr.size(), &str[0], size_needed, NULL, NULL);
    return str;
}

// UTF-8 string → wstring 変換関数（Windows専用）
inline std::wstring Utf8ToWString_fsort(const std::string& str) {
    if (str.empty()) return L"";
    int size_needed = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), (int)str.size(), NULL, 0);
    std::wstring wstr(size_needed, 0);
    MultiByteToWideChar(CP_UTF8, 0, str.c_str(), (int)str.size(), &wstr[0], size_needed);
    return wstr;
}

// [xxxx]のxxxx部分を取得
std::string GetBracketContent(const std::string& src) {
    std::smatch m;
    std::regex re(R"(\[(.*?)\])");
    if (std::regex_search(src, m, re)) {
        return m[1].str();
    }
    return "";
}

// FSort: ファイルを[xxxx]ディレクトリに移動
std::string FSort(const std::string& p) {
    // ★UTF-8文字列をワイド文字列に変換してからpathを構築
    fs::path path(Utf8ToWString_fsort(p));
    if (!fs::exists(path) || !fs::is_regular_file(path)) {
        return "Error: File not found. " + p;
    }
    // ★ .string()の代わりに.wstring()を使用してUTF-8に変換
    std::string fn = WStringToUtf8_fsort(path.filename().wstring());
    std::string cnt = GetBracketContent(fn);
    std::string ext = WStringToUtf8_fsort(path.extension().wstring());
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    if (ext != ".zip" && ext != ".rar") {
        return "Skipped (not zip/rar). " + fn;
    }
    if (cnt.empty()) {
        return "No brackets found. " + fn;
    }
    fs::path newDir = path.parent_path() / Utf8ToWString_fsort(cnt);
    if (!fs::exists(newDir)) {
        fs::create_directory(newDir);
    }
    fs::path newPath = newDir / path.filename();
    if (fs::exists(newPath)) {
        return "File already exists in target directory. " + WStringToUtf8_fsort(newPath.wstring());
    }
    try {
        fs::rename(path, newPath);
        return "Moved: " + fn + " -> " + WStringToUtf8_fsort(newDir.wstring());
    }
    catch (const std::exception& ex) {
        return std::string("Error moving file: ") + ex.what();
    }
}

// サブディレクトリ内のファイルを一つ上の階層に移動し、空なら削除
std::string FsortDirSub(const fs::path& targetPath) {
    std::string ret;
    if (!fs::exists(targetPath) || !fs::is_directory(targetPath)) return ret;

    std::vector<fs::path> files;
    for (const auto& entry : fs::directory_iterator(targetPath)) {
        if (entry.is_regular_file()) {
            files.push_back(entry.path());
        }
    }
    if (files.empty()) return ret;

    for (const auto& file : files) {
        fs::path d = file.parent_path();
        fs::path d2 = d.parent_path();
        fs::path fn = file.filename();
        fs::path dest = d2 / fn;
        try {
            fs::rename(file, dest);
            ret += "moved file:" + WStringToUtf8_fsort(fn.wstring()) + "\n";
        }
        catch (const std::exception& ex) {
            ret += "Error moving file: " + std::string(ex.what()) + "\n" + WStringToUtf8_fsort(fn.wstring()) + "\n";
        }
    }
    // 残ファイルがなければディレクトリ削除
    if (fs::is_empty(targetPath)) {
        fs::remove(targetPath);
    }
    return ret;
}

// 指定ディレクトリ内のサブディレクトリを処理
std::string FsortDir(const std::string& targetPath) {
    std::string ret;
    // ★UTF-8文字列をワイド文字列に変換してからpathを構築
    fs::path path(Utf8ToWString_fsort(targetPath));
    if (fs::exists(path) && fs::is_regular_file(path)) {
        path = path.parent_path();
    }
    if (!fs::exists(path) || !fs::is_directory(path)) return ret;

    for (const auto& entry : fs::directory_iterator(path)) {
        if (entry.is_directory()) {
            std::string s = FsortDirSub(entry.path());
            if (!s.empty()) ret += s;
        }
    }
    return ret;
}

std::wstring NormalizePathUnicode(const std::wstring& path) {
    int len = NormalizeString(NormalizationC, path.c_str(), -1, NULL, 0);
    if (len <= 0) return path;
    
    std::vector<wchar_t> buffer(len);
    int result = NormalizeString(NormalizationC, path.c_str(), -1, buffer.data(), len);
    if (result <= 0) return path;
    
    return std::wstring(buffer.data(), result - 1);
}

// wstring → UTF-8 string 変換関数（Windows専用）
std::string WStringToUtf8(const std::wstring& wstr) {
    if (wstr.empty()) return "";
    int size_needed = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), (int)wstr.size(), NULL, 0, NULL, NULL);
    std::string str(size_needed, 0);
    WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), (int)wstr.size(), &str[0], size_needed, NULL, NULL);
    return str;
}

// UTF-8 string → wstring 変換関数（Windows専用）
std::wstring Utf8ToWString(const std::string& str) {
    if (str.empty()) return L"";
    int size_needed = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), (int)str.size(), NULL, 0);
    std::wstring wstr(size_needed, 0);
    MultiByteToWideChar(CP_UTF8, 0, str.c_str(), (int)str.size(), &wstr[0], size_needed);
    return wstr;
}