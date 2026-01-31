#include <iostream>
#include <vector>
#include <string>
#include <filesystem>
#include <algorithm>
#include <windows.h>
#include <fcntl.h>
#include <io.h>

#include <bit7z/bit7zlibrary.hpp>
#include <bit7z/bitfileextractor.hpp>
#include <bit7z/bitfilecompressor.hpp>

namespace fs = std::filesystem;

// bit7z (UTF-8) 用の変換
std::string ToUtf8(const std::wstring& wstr) {
    if (wstr.empty()) return std::string();
    int size_needed = WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), NULL, 0, NULL, NULL);
    std::string strTo(size_needed, 0);
    WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), &strTo[0], size_needed, NULL, NULL);
    return strTo;
}

bool ConvertRarToZip(bit7z::Bit7zLibrary& lib, const fs::path& rarPath) {
    fs::path absRarPath = fs::absolute(rarPath);
    fs::path workRar = absRarPath.parent_path() / L"processing_target.tmp";
    fs::path tempDir = absRarPath.parent_path() / L"T_EXTRACT_WORK";

    try {
        std::wcout << L"\nTarget: " << absRarPath.filename().wstring() << std::endl;

        // 1. 特殊文字回避のために一時リネーム
        if (fs::exists(workRar)) fs::remove(workRar);
        fs::rename(absRarPath, workRar);

        if (fs::exists(tempDir)) fs::remove_all(tempDir);
        fs::create_directories(tempDir);

        // 2. 展開（RAR）
        bit7z::BitFileExtractor extractor{ lib, bit7z::BitFormat::Rar };

        // 【重要】Callbackの引数を (uint64_t) 1つにし、戻り値を bool にします
        extractor.setProgressCallback([](uint64_t completed) -> bool {
            std::wcout << L"\rExtracting... " << (completed / 1024) << L" KB" << std::flush;
            return true; // 処理を続行
            });

        extractor.extractMatching(ToUtf8(workRar.wstring()), "*.scr", ToUtf8(tempDir.wstring()), bit7z::FilterPolicy::Exclude);
        std::wcout << L" [Done]" << std::endl;

        // 3. 圧縮（ZIP）
        fs::path zipPath = absRarPath;
        zipPath.replace_extension(L".zip");
        bit7z::BitFileCompressor compressor{ lib, bit7z::BitFormat::Zip };

        compressor.setProgressCallback([](uint64_t completed) -> bool {
            std::wcout << L"\rCompressing... " << (completed / 1024) << L" KB" << std::flush;
            return true;
            });

        fs::path oldPath = fs::current_path();
        fs::current_path(tempDir);
        try {
            compressor.compress({ "." }, ToUtf8(zipPath.wstring()));
        }
        catch (...) {
            fs::current_path(oldPath);
            throw;
        }
        fs::current_path(oldPath);

        // 4. 後始末（元のファイルはリネームで戻す）
        fs::remove_all(tempDir);
        if (fs::exists(workRar)) fs::rename(workRar, absRarPath);

        std::wcout << L"Success: " << zipPath.filename().wstring() << std::endl;
        return true;
    }
    catch (const bit7z::BitException& ex) {
        if (fs::exists(workRar) && !fs::exists(absRarPath)) fs::rename(workRar, absRarPath);
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
            if (fs::exists(p)) ConvertRarToZip(lib, p);
        }
        LocalFree(wargv);
    }
    catch (const bit7z::BitException& ex) {
        fwprintf(stderr, L"Fatal Error: %S\n", ex.what());
        return 1;
    }
    return 0;
}