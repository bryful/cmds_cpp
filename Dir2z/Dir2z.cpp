#include <iostream>
#include <vector>
#include <string>
#include <filesystem>
#include <algorithm>
#include <windows.h>
#include <fcntl.h>
#include <io.h>

#include <bit7z/bit7zlibrary.hpp>
#include <bit7z/bitfilecompressor.hpp>

namespace fs = std::filesystem;

// UTF-8変換（bit7zへのパス受け渡し用）
std::string ToUtf8(const std::wstring& wstr) {
    if (wstr.empty()) return std::string();
    int size_needed = WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), NULL, 0, NULL, NULL);
    std::string strTo(size_needed, 0);
    WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), &strTo[0], size_needed, NULL, NULL);
    return strTo;
}

// 個別圧縮処理
// 個別圧縮処理
bool CompressIndividual(bit7z::Bit7zLibrary& lib, const fs::path& targetPath) {
    try {
        if (!fs::is_directory(targetPath)) return false;

        // 絶対パスに変換しておく（カレントディレクトリ移動に備えて）
        fs::path absTargetPath = fs::absolute(targetPath);
        fs::path zipPath = absTargetPath;
        zipPath.replace_extension(L".zip");

        if (fs::exists(zipPath)) {
            std::wcout << L"Skipped: " << zipPath.filename().wstring() << L" already exists." << std::endl;
            return false;
        }

        bit7z::BitFileCompressor compressor{ lib, bit7z::BitFormat::Zip };

        // 進捗表示
        uint64_t totalSize = 0;
        compressor.setTotalCallback([&totalSize](uint64_t total) { totalSize = total; });
        compressor.setProgressCallback([&totalSize](uint64_t completed) {
            if (totalSize > 0) {
                int percent = static_cast<int>((completed * 100) / totalSize);
                std::wcout << L"\rCompressing: " << percent << L"% " << std::flush;
            }
            return true;
            });

        std::wcout << L"Target: " << absTargetPath.filename().wstring() << std::endl;

        // --- 【改善ポイント】ワイルドカードを使わない方式 ---
        // 1. 現在のディレクトリを保存
        fs::path oldPath = fs::current_path();

        // 2. 圧縮したいフォルダの中へ移動
        fs::current_path(absTargetPath);

        try {
            // 3. 移動後の場所で "." （現在のディレクトリすべて）を指定して圧縮
            // これにより、ルートディレクトリ名を含まずに中身だけが圧縮されます
            compressor.compress({ "." }, ToUtf8(zipPath.wstring()));
        }
        catch (...) {
            fs::current_path(oldPath); // エラー時もディレクトリを戻す
            throw;
        }

        // 4. 元のディレクトリに戻す
        fs::current_path(oldPath);
        // --------------------------------------------------

        std::wcout << L"\nSuccess: " << zipPath.filename().wstring() << std::endl;
        return true;
    }
    catch (const bit7z::BitException& ex) {
        fwprintf(stderr, L"\nbit7z Error: %S\n", ex.what());
        return false;
    }
}
int main() {
    _setmode(_fileno(stdout), _O_U16TEXT);
    _setmode(_fileno(stderr), _O_U16TEXT);

    try {
        bit7z::Bit7zLibrary lib{ "7z.dll" };
        int wargc;
        wchar_t** wargv = CommandLineToArgvW(GetCommandLineW(), &wargc);
        if (!wargv) return 1;

        for (int i = 1; i < wargc; ++i) {
            fs::path p = wargv[i];
            if (fs::exists(p)) CompressIndividual(lib, p);
        }
        LocalFree(wargv);
    }
    catch (const bit7z::BitException& ex) {
        fwprintf(stderr, L"Fatal Error: %S\n", ex.what());
        return 1;
    }
    return 0;
}