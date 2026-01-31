#include <iostream>
#include <vector>
#include <string>
#include <filesystem>
#include <algorithm>
#include <windows.h>
#include <fcntl.h>
#include <io.h>
#include <iomanip>
#include <cwchar> // fwprintf 用

#include <bit7z/bit7zlibrary.hpp>
#include <bit7z/bitfileextractor.hpp>
#include <bit7z/bitinputarchive.hpp>

namespace fs = std::filesystem;

// 安全なワイド文字出力ヘルパー
void SafeWriteError(const std::string& msg) {
    // string (UTF-8) を一度 wstring に戻す（表示用）
    int wsize = MultiByteToWideChar(CP_UTF8, 0, msg.c_str(), -1, NULL, 0);
    std::wstring wmsg(wsize, 0);
    MultiByteToWideChar(CP_UTF8, 0, msg.c_str(), -1, &wmsg[0], wsize);

    // std::wcerr が有効か確認
    if (std::wcerr.rdbuf() != nullptr) {
        std::wcerr << L"bit7z Error: " << wmsg << std::endl;
    }
    else {
        // ストリームが死んでいる場合は Windows のデバッガまたは標準エラー出力へ直接
        fwprintf(stderr, L"Fatal Error (Stream dead): %s\n", wmsg.c_str());
        OutputDebugStringW(wmsg.c_str());
    }
}

// UTF-8変換ヘルパー
std::string ToUtf8(const std::wstring& wstr) {
    if (wstr.empty()) return std::string();
    int size_needed = WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), NULL, 0, NULL, NULL);
    std::string strTo(size_needed, 0);
    WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), &strTo[0], size_needed, NULL, NULL);
    return strTo;
}

// 単一ルートフォルダ判定
bool HasSingleRootFolder(bit7z::Bit7zLibrary& lib, const fs::path& archivePath, const bit7z::BitInFormat& format) {
    try {
        bit7z::BitFileExtractor handler{ lib, format };
        bit7z::BitInputArchive inputArchive{ handler, ToUtf8(archivePath.wstring()) };

        std::string commonRoot = "";
        bool firstItem = true;

        for (const auto& item : inputArchive) {
            if (item.isDir()) continue;

            std::string path = item.path();
            std::replace(path.begin(), path.end(), '\\', '/');

            size_t pos = path.find_first_of('/');
            if (pos == std::string::npos) return false; // ルート直下にファイルがある

            std::string currentRoot = path.substr(0, pos);
            if (firstItem) {
                commonRoot = currentRoot;
                firstItem = false;
            }
            else if (commonRoot != currentRoot) {
                return false; // 複数のルートフォルダがある
            }
        }
        return !commonRoot.empty();
    }
    catch (...) {
        return false;
    }
}

// 展開処理
bool ExtractIndividual(bit7z::Bit7zLibrary& lib, const fs::path& archivePath) {
    try {
        std::wstring ext = archivePath.extension().wstring();
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
        if (ext != L".zip" && ext != L".rar") return false;

        const auto& format = (ext == L".rar") ? bit7z::BitFormat::Rar : bit7z::BitFormat::Zip;

        fs::path outputDir = HasSingleRootFolder(lib, archivePath, format)
            ? archivePath.parent_path()
            : archivePath.parent_path() / archivePath.stem();

        if (!fs::exists(outputDir)) fs::create_directories(outputDir);

        bit7z::BitFileExtractor extractor{ lib, format };

        // --- プログレス表示の設定 ---
        uint64_t totalSize = 0;
        extractor.setTotalCallback([&totalSize](uint64_t total) {
            totalSize = total;
            });

        extractor.setProgressCallback([&totalSize](uint64_t completed) {
            if (totalSize > 0) {
                int percent = static_cast<int>((completed * 100) / totalSize);
                std::wcout << L"\rProgress: " << percent << L"% ("
                    << (completed / 1024) << L" / " << (totalSize / 1024) << L" KB)" << std::flush;
            }
            return true; // false を返すとキャンセル
            });
        // ---------------------------

        std::wcout << L"\nProcessing: " << archivePath.filename().wstring() << std::endl;

        extractor.extractMatching(
            ToUtf8(archivePath.wstring()),
            "*.scr",
            ToUtf8(outputDir.wstring()),
            bit7z::FilterPolicy::Exclude
        );

        std::wcout << L"\nSuccess!" << std::endl;
        return true;
    }
    catch (const bit7z::BitException& ex) {
        SafeWriteError(ex.what());
        return false;
    }
}

int main() {
    _setmode(_fileno(stdout), _O_U16TEXT);
    _setmode(_fileno(stderr), _O_U16TEXT);

    try {
        // 1. ここで失敗すると BitException が飛ぶ
        bit7z::Bit7zLibrary lib{ "7z.dll" };

        int wargc;
        wchar_t** wargv = CommandLineToArgvW(GetCommandLineW(), &wargc);
        if (!wargv) return 1;

        for (int i = 1; i < wargc; ++i) {
            fs::path p = wargv[i];
            if (fs::exists(p)) ExtractIndividual(lib, p);
        }
        LocalFree(wargv);
    }
    catch (const bit7z::BitException& ex) {
        // std::wcerr ではなく C 関数の fwprintf を使う（最も安全）
        // DLL が見つからない時はここに来ます
        fwprintf(stderr, L"bit7z Fatal Error: %S\n", ex.what());
        return 1;
    }
    return 0;
}