#pragma once
#include <string>
#include <vector>
#include <algorithm>
#include <cwctype>
#include <Windows.h>
#include <filesystem>
#include <iostream>
#include <fstream>
#include <regex>
#include <chrono>
#include <iomanip>
#include <sstream>

inline std::wstring NormalizePathUnicode(const std::wstring& path) {
    if (path.empty()) return path;
    int len = NormalizeString(NormalizationC, path.c_str(), (int)path.length(), NULL, 0);
    if (len <= 0) return path;
    std::wstring normalized(len, 0);
    NormalizeString(NormalizationC, path.c_str(), (int)path.length(), &normalized[0], len);
    return normalized;
}

inline std::wstring toLower(std::wstring s) {
    std::transform(s.begin(), s.end(), s.begin(), [](wchar_t c) { return std::towlower(c); });
    return s;
}

class BracketRename {
private:
    std::wofstream logFile;

    std::wstring GetCurrentTimeString() {
        auto now = std::chrono::system_clock::now();
        auto time_t_now = std::chrono::system_clock::to_time_t(now);
        std::tm tm_buf;
        localtime_s(&tm_buf, &time_t_now);
        std::wstringstream wss;
        wss << std::put_time(&tm_buf, L"%Y%m%d_%H%M");
        return wss.str();
    }

public:
    BracketRename() {}

    void InitLog(const std::filesystem::path& targetDir) {
        std::wstring logFileName = L"log" + GetCurrentTimeString() + L".txt";
        std::filesystem::path logPath = targetDir / logFileName;
        
        logFile.open(logPath, std::ios::out | std::ios::app);
        if (logFile.is_open()) {
            logFile.imbue(std::locale(""));
        }
    }

    bool Rename(const std::filesystem::path& srcP) {
        if (!std::filesystem::exists(srcP)) return false;

        std::filesystem::path dir = srcP.parent_path();
        std::wstring filename = srcP.filename().wstring();

        filename = NormalizePathUnicode(filename);

        // [aaa(bbb)] を [bbb] に置換する正規表現
        // \[  : '['
        // [^\]\(]+ : ']' や '(' 以外の文字 (aaa)
        // \(  : '('
        // ([^\)]+) : ')' 以外の文字 (bbb) -> キャプチャグループ1
        // \)  : ')'
        // \]  : ']'
        static const std::wregex pattern(LR"(\[[^\]\(]+\(([^\)]+)\)\])");

        std::wstring newFilename = std::regex_replace(filename, pattern, L"[$1]");

        if (filename == newFilename) return false;

        std::filesystem::path newP = dir / newFilename;

        int count = 1;
        std::wstring stem = newP.stem().wstring();
        std::wstring ext = newP.extension().wstring();
        while (std::filesystem::exists(newP) && !std::filesystem::equivalent(srcP, newP)) {
            std::wstring tempStem = stem + L"_" + std::to_wstring(count++);
            newP = dir / (tempStem + ext);
        }

        try {
            std::filesystem::rename(srcP, newP);
            
            std::wstring oldName = srcP.filename().wstring();
            std::wstring renamedName = newP.filename().wstring();

            std::wcout << L"Renamed: " << oldName << L" -> " << renamedName << std::endl;

            if (logFile.is_open()) {
                logFile << L"\"" << oldName << L"\" >> \"" << renamedName << L"\"" << std::endl;
            }
        }
        catch (...) {
            return false;
        }
        return true;
    }
};