#pragma once
#include <string>
#include <vector>
#include <regex>
#include <fstream>
#include <algorithm>
#include <cwctype>
#include <codecvt>
#include <Windows.h>
#include <filesystem>
#include <iostream>
#include <io.h>
#include <fcntl.h>

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
// UTF-8→wstring変換関数（Windows専用）
static std::wstring Utf8ToWString(const std::string& str) {
    if (str.empty()) return L"";
    int size_needed = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), (int)str.size(), NULL, 0);
    std::wstring wstr(size_needed, 0);
    MultiByteToWideChar(CP_UTF8, 0, str.c_str(), (int)str.size(), &wstr[0], size_needed);
    return wstr;
}
static std::wstring ReplaceAll(const std::wstring& src, const std::wstring& target, const std::wstring& replacement) {
    if (target.empty()) return src;
    std::wstring result = src;
    size_t pos = 0;
    while ((pos = result.find(target, pos)) != std::wstring::npos) {
        result.replace(pos, target.length(), replacement);
        pos += replacement.length(); // 置換後の位置から再検索
    }
    return result;
}
class HeaderRename {
private:
    std::vector<std::wstring> delWords;

public:
    HeaderRename() {
        Init();
    }

    void Init() {
        delWords.clear();
        delWords.push_back(L"(成年コミック)");
        delWords.push_back(L"成年コミック");
        delWords.push_back(L"(一般コミック)");
        delWords.push_back(L"一般コミック");
        delWords.push_back(L"(一般小説)");
        delWords.push_back(L"(一般小説･SF)");
        delWords.push_back(L"(一般小説･近代SF)");
        delWords.push_back(L"(一般小説･古典SF)");
        delWords.push_back(L"(商業誌)");
        delWords.push_back(L"商業誌");
        delWords.push_back(L"(同人誌)");
        delWords.push_back(L"同人誌");
        delWords.push_back(L"[雑誌]");
        delWords.push_back(L"雑誌");
        delWords.push_back(L"(C94)");
        delWords.push_back(L"(C95)");
        delWords.push_back(L"(C96)");
        delWords.push_back(L"(C97)");
        delWords.push_back(L"(C98)");
        delWords.push_back(L"(C99)");
        delWords.push_back(L"(C100)");
        delWords.push_back(L"(C101)");
        delWords.push_back(L"(C102)");
        delWords.push_back(L"(C103)");
        delWords.push_back(L"(C104)");
        delWords.push_back(L"(C105)");
        delWords.push_back(L"(C106)");
        delWords.push_back(L"()");
        delWords.push_back(L"( )");
        delWords.push_back(L"[]");
        delWords.push_back(L"[ ]");
        delWords.push_back(L"【 】");
        delWords.push_back(L"【】");
    }

    void AddDelWord(const std::wstring& word) {
        std::wstring w = Trim(word);
        if (w.empty()) return;
        if (std::find(delWords.begin(), delWords.end(), w) == delWords.end()) {
            delWords.push_back(w);
        }
    }

    bool LoadWords(const std::wstring& path) {
        delWords.clear();
        std::ifstream ifs(path, std::ios::binary);
        if (!ifs) {
            Init();
            return false;
        }
        std::string line;
        bool hasLine = false;
        while (std::getline(ifs, line)) {
            // 置き換え: std::wstring_convert<std::codecvt_utf8<wchar_t>> conv;
            std::wstring wline = Utf8ToWString(line);
            AddDelWord(wline);
            hasLine = true;
        }
        if (!hasLine) {
            Init();
            return false;
        }
        return true;
    }

    // 修正: std::wstringをstd::stringに変換してから出力する
    bool SaveWords(const std::wstring& path) {
        std::ofstream ofs(path);
        if (!ofs) {
            return false;
        }
        // std::wstringをUTF-8に変換して出力
        for (const auto& word : delWords) {
            // Windows専用: WideCharToMultiByteでUTF-8変換
            int size_needed = WideCharToMultiByte(CP_UTF8, 0, word.c_str(), (int)word.size(), NULL, 0, NULL, NULL);
            std::string utf8word(size_needed, 0);
            WideCharToMultiByte(CP_UTF8, 0, word.c_str(), (int)word.size(), &utf8word[0], size_needed, NULL, NULL);
            ofs << utf8word << std::endl;
        }
        return true;
    }

    std::wstring DelSP(const std::wstring& src) {
        std::wstring s = Trim(src);
        if (s.length() <= 1) return s;
        std::wstring res(1, s[0]);
        for (size_t i = 1; i < s.length(); ++i) {
            if (s[i] != ' ') {
                res += s[i];
            } else if (s[i - 1] != ' ') {
                res += s[i];
            }
        }
        return res;
    }

    
    
    std::wstring ZenHan(const std::wstring& src) {
        std::wstring res;
        for (wchar_t c : src) {
            // 英字全角→半角
            if (c >= L'Ａ' && c <= L'Ｚ') res += L'A' + (c - L'Ａ');
            else if (c >= L'ａ' && c <= L'ｚ') res += L'a' + (c - L'ａ');
            // 数字全角→半角
            else if (c >= L'０' && c <= L'９') res += L'0' + (c - L'０');
            // 括弧類
            else if (c == L'　') res += L' ';
            else if (c == L'（') res += L'(';
            else if (c == L'）') res += L')';
            else if (c == L'｛') res += L'{';
            else if (c == L'｝') res += L'}';
            else if (c == L'［' || c == L'【') res += L'[';
            else if (c == L'］' || c == L'】') res += L']';
            else if (c == L'〜' || c == L'～') res += L'～';  // 波ダッシュと全角チルダを半角に   
            else res += c;
        }
        return res;
    }
    std::wstring TrimU(const std::wstring& src) {
        if (src.empty()) return src;
        size_t start = 0;
        size_t end = src.size();

        // 先頭側
        while (start < end && (src[start] == L'_' || src[start] == L'-')) {
            ++start;
        }
        // 末尾側
        while (end > start && (src[end - 1] == L'_' || src[end - 1] == L'-')) {
            --end;
        }
        return src.substr(start, end - start);

    }
    std::wstring RemoveDateStrings(const std::wstring& src) {
        // 年は2桁または4桁、月・日は1桁または2桁、区切りは-, /, . に対応
        std::wregex re(LR"((\b\d{4}|\b\d{2})[-/.]\d{1,2}[-/.]\d{1,2}\b)");
        return std::regex_replace(src, re, L"");
    }
    // wstring → UTF-8 string 変換ヘルパー
    static std::string WStringToUtf8(const std::wstring& wstr) {
        if (wstr.empty()) return "";
        int size_needed = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), (int)wstr.size(), NULL, 0, NULL, NULL);
        std::string str(size_needed, 0);
        WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), (int)wstr.size(), &str[0], size_needed, NULL, NULL);
        return str;
    }

    bool Rename(const std::filesystem::path& srcP) {
        // src: フルパス
        std::wstring dir = srcP.parent_path().wstring();
        std::wstring filename = srcP.stem().wstring();
        std::wstring ext = srcP.extension().wstring();

        filename = Trim(filename);
        if (filename.empty()) return false;

        std::wstring fn2 = ZenHan(filename);
        fn2 = RemoveDateStrings(fn2);
        fn2 = DelSP(fn2);
        for (const auto& word : delWords) {
            fn2 = ReplaceAll(fn2, word, L"");
        }
        fn2 = DelSP(fn2);
        fn2 = Trim(fn2);
        fn2 = TrimU(fn2);

        std::wstring newFilename = dir.empty() ? fn2 + ext : dir + L"\\" + fn2 + ext;
        std::filesystem::path newP = std::filesystem::path(newFilename);
        
        // 文字列比較ではなく、filesystem::equivalentを使用
        try {
            if (std::filesystem::exists(newP) && std::filesystem::equivalent(srcP, newP)) {
                std::wcout << L"リネーム不必要 " << srcP.filename().wstring() << std::endl;
                return true;
            }
        }
        catch (const std::filesystem::filesystem_error&) {
            // equivalent失敗時は続行
        }

        while (std::filesystem::exists(newFilename)) {
            fn2 += L"_";
            newFilename = dir.empty() ? fn2 + ext : dir + L"\\" + fn2 + ext;
        }
        try {
            std::filesystem::path newP = std::filesystem::path(newFilename);
            std::filesystem::rename(srcP, newP);
            std::wcout << L"リネーム成功: " << srcP.filename().wstring() << L" → " << newP.filename().wstring() << std::endl;
        }
        catch (const std::filesystem::filesystem_error& e) {
            std::cerr << "リネーム失敗: " << e.what() << std::endl;
        }

        return true;
    }

    // ユーティリティ: 文字列の前後空白除去
    static std::string Trim(const std::string& s) {
        size_t start = s.find_first_not_of(" \t\r\n");
        if (start == std::string::npos) return "";
        size_t end = s.find_last_not_of(" \t\r\n");
        return s.substr(start, end - start + 1);
    }
    static std::wstring Trim(const std::wstring& s) {
        size_t start = s.find_first_not_of(L" \t\r\n");
        if (start == std::wstring::npos) return L"";
        size_t end = s.find_last_not_of(L" \t\r\n");
        return s.substr(start, end - start + 1);
    }
};