#pragma once
#include <string>
#include <vector>
#include <algorithm>
#include <cwctype>
#include <Windows.h>
#include <filesystem>
#include <iostream>
#include <fstream>
#include <regex> // 追加
#include <io.h>
#include <fcntl.h>

// ヌル文字を絶対に混入させない正規化関数
inline std::wstring NormalizePathUnicode(const std::wstring& path) {
    if (path.empty()) return path;
    int len = NormalizeString(NormalizationC, path.c_str(), (int)path.length(), NULL, 0);
    if (len <= 0) return path;
    std::wstring normalized(len, 0);
    NormalizeString(NormalizationC, path.c_str(), (int)path.length(), &normalized[0], len);
    return normalized;
}

inline std::wstring toLowwer(std::wstring s) {
    std::transform(s.begin(), s.end(), s.begin(), [](wchar_t c) { return std::towlower(c); });
    return s;
}

class HeaderRename {
private:
    std::vector<std::wstring> delWords;

public:
    HeaderRename() { Init(); }

    static std::wstring Trim(const std::wstring& s) {
        size_t start = s.find_first_not_of(L" \t\r\n");
        if (start == std::wstring::npos) return L"";
        size_t end = s.find_last_not_of(L" \t\r\n");
        return s.substr(start, end - start + 1);
    }

    static std::wstring ReplaceAll(const std::wstring& src, const std::wstring& target, const std::wstring& replacement) {
        if (target.empty()) return src;
        std::wstring result = src;
        size_t pos = 0;
        while ((pos = result.find(target, pos)) != std::wstring::npos) {
            result.replace(pos, target.length(), replacement);
            pos += replacement.length();
        }
        return result;
    }

    std::wstring ZenHan(const std::wstring& src) {
        std::wstring res;
        for (wchar_t c : src) {
            if (c >= L'Ａ' && c <= L'Ｚ') res += L'A' + (c - L'Ａ');
            else if (c >= L'ａ' && c <= L'ｚ') res += L'a' + (c - L'ａ');
            else if (c >= L'０' && c <= L'９') res += L'0' + (c - L'０');
            else if (c == L'　') res += L' ';
            else if (c == L'（') res += L'(';
            else if (c == L'）') res += L')';
            else if (c == L'［' || c == L'【') res += L'[';
            else if (c == L'］' || c == L'】') res += L']';
            else if (c == L'〜' || c == L'～') res += L'～'; //～に統一！半角はNG
            else if (c != L'\0') res += c;
            else res += c;
        }
        return res;
    }

    void Init() {
        delWords = { L"(成年コミック)", L"成年コミック", L"(一般コミック)", L"一般コミック", L"(商業誌)", L"商業誌", L"(同人誌)", L"同人誌", L"[雑誌]", L"雑誌" };
    }

    bool LoadWords(const std::wstring& path) {
        std::ifstream ifs(path);
        if (!ifs) return false;
        std::string line;
        delWords.clear();
        while (std::getline(ifs, line)) {
            int size_needed = MultiByteToWideChar(CP_UTF8, 0, line.c_str(), (int)line.size(), NULL, 0);
            std::wstring wline(size_needed, 0);
            MultiByteToWideChar(CP_UTF8, 0, line.c_str(), (int)line.size(), &wline[0], size_needed);
            wline = Trim(wline);
            if (!wline.empty()) delWords.push_back(wline);
        }
        return true;
    }

    bool SaveWords(const std::wstring& path) {
        std::ofstream ofs(path);
        if (!ofs) return false;
        for (const auto& word : delWords) {
            int size_needed = WideCharToMultiByte(CP_UTF8, 0, word.c_str(), (int)word.size(), NULL, 0, NULL, NULL);
            std::string utf8(size_needed, 0);
            WideCharToMultiByte(CP_UTF8, 0, word.c_str(), (int)word.size(), &utf8[0], size_needed, NULL, NULL);
            ofs << utf8 << std::endl;
        }
        return true;
    }

    bool Rename(const std::filesystem::path& srcP) {
        if (!std::filesystem::exists(srcP)) return false;

        std::filesystem::path dir = srcP.parent_path();
        std::wstring stem;
        std::wstring ext;

        if (std::filesystem::is_directory(srcP)) {
            stem = srcP.filename().wstring();
            ext = L"";
        }
        else {
            stem = srcP.stem().wstring();
            ext = srcP.extension().wstring();
        }

        // 1. 正規化（ヌル文字排除）
        stem = NormalizePathUnicode(stem);

        // 2. 加工
        std::wstring newStem = ZenHan(stem);
        for (const auto& word : delWords) {
            newStem = ReplaceAll(newStem, word, L"");
        }

        // ★ 複数の半角スペースを1つに集約
        static const std::wregex multi_space(L" +");
        newStem = std::regex_replace(newStem, multi_space, L" ");

        newStem = Trim(newStem);

        // 結合前にヌル文字を完全除去（デバッグ結果に基づく対策）
        newStem.erase(std::remove(newStem.begin(), newStem.end(), L'\0'), newStem.end());
        ext.erase(std::remove(ext.begin(), ext.end(), L'\0'), ext.end());

        if (newStem.empty()) return false;

        // 3. 安全なパス合成
        std::filesystem::path newP = dir;
        newP /= newStem;
        newP.concat(ext);

        try {
            if (std::filesystem::exists(newP) && std::filesystem::equivalent(srcP, newP)) {
                if (srcP.filename().wstring() == newP.filename().wstring()) return true;
            }
        }
        catch (...) {}

        int count = 1;
        std::wstring baseStem = newStem;
        while (std::filesystem::exists(newP) && !std::filesystem::equivalent(srcP, newP)) {
            newStem = baseStem + L"_" + std::to_wstring(count++);
            newP = dir;
            newP /= newStem;
            newP.concat(ext);
        }

        try {
            std::filesystem::rename(srcP, newP);
            std::wcout << L"Renamed: " << srcP.filename().wstring() << L" -> " << newP.filename().wstring() << std::endl;
        }
        catch (...) {
            return false;
        }
        return true;
    }
};