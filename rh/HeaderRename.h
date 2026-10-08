#pragma once
#include <Windows.h>
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

inline std::string Utf8(const std::wstring& value) {
    if (value.empty()) return {};
    const int length = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, value.data(), static_cast<int>(value.size()), nullptr, 0, nullptr, nullptr);
    if (!length) throw std::runtime_error("Invalid Unicode text");
    std::string result(length, 0);
    WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, value.data(), static_cast<int>(value.size()), result.data(), length, nullptr, nullptr);
    return result;
}

inline std::wstring NormalizePathUnicode(const std::wstring& value) {
    if (value.empty() || std::all_of(value.begin(), value.end(), [](wchar_t c) { return c < 128; })) return value;
    int size = NormalizeString(NormalizationC, value.data(), static_cast<int>(value.size()), nullptr, 0);
    if (size <= 0) throw std::runtime_error("Unicode normalization failed");
    for (int attempt = 0; attempt < 3; ++attempt) {
        std::wstring result(size, 0);
        const int written = NormalizeString(NormalizationC, value.data(), static_cast<int>(value.size()), result.data(), size);
        if (written > 0) { result.resize(written); return result; }
        if (written >= 0) break;
        size = -written;
    }
    throw std::runtime_error("Unicode normalization failed");
}

struct OrdinalLess {
    bool operator()(const std::wstring& a, const std::wstring& b) const {
        return CompareStringOrdinal(a.c_str(), -1, b.c_str(), -1, TRUE) == CSTR_LESS_THAN;
    }
};

class HeaderRename {
    std::vector<std::wstring> delWords = { L"(成年コミック)", L"成年コミック", L"(一般コミック)", L"一般コミック", L"(商業誌)", L"商業誌", L"(同人誌)", L"同人誌", L"[雑誌]", L"雑誌", L"[]", L"「」" };
public:
    static std::wstring Trim(const std::wstring& value) {
        const auto first = value.find_first_not_of(L" \t\r\n");
        return first == std::wstring::npos ? L"" : value.substr(first, value.find_last_not_of(L" \t\r\n") - first + 1);
    }
    bool LoadWords(const std::filesystem::path& path) {
        if (!std::filesystem::exists(path)) return false;
        std::ifstream input(path, std::ios::binary);
        if (!input) throw std::runtime_error("Cannot open rule file");
        std::vector<std::wstring> words;
        std::string line;
        bool first = true;
        while (std::getline(input, line)) {
            if (first && line.starts_with("\xef\xbb\xbf")) line.erase(0, 3);
            first = false;
            if (line.empty()) continue;
            const int length = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, line.data(), static_cast<int>(line.size()), nullptr, 0);
            if (!length || line.find('\0') != std::string::npos) throw std::runtime_error("Rule file must contain valid UTF-8 without NUL");
            std::wstring word(length, 0);
            MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, line.data(), static_cast<int>(line.size()), word.data(), length);
            word = Trim(word);
            if (!word.empty()) words.push_back(word);
        }
        if (!input.eof()) throw std::runtime_error("Cannot read rule file");
        delWords = std::move(words);
        return true;
    }
    void SaveWords(const std::filesystem::path& path) const {
        // Exclusive creation protects a rule file created by another process.
        HANDLE file = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (file == INVALID_HANDLE_VALUE) throw std::runtime_error("Cannot create default rule file");
        std::string text;
        for (const auto& word : delWords) text += Utf8(word) + '\n';
        DWORD written = 0;
        const bool ok = WriteFile(file, text.data(), static_cast<DWORD>(text.size()), &written, nullptr) && written == text.size();
        const bool closed = CloseHandle(file) != FALSE;
        if (!ok || !closed) throw std::runtime_error("Cannot write default rule file");
    }
    std::wstring Transform(const std::wstring& source) const {
        auto value = NormalizePathUnicode(source);
        for (auto& c : value) {
            if (c >= L'Ａ' && c <= L'Ｚ') c = L'A' + (c - L'Ａ');
            else if (c >= L'ａ' && c <= L'ｚ') c = L'a' + (c - L'ａ');
            else if (c >= L'０' && c <= L'９') c = L'0' + (c - L'０');
            else if (c == L'　') c = L' ';
            else if (c == L'（') c = L'(';
            else if (c == L'）') c = L')';
            else if (c == L'［' || c == L'【') c = L'[';
            else if (c == L'］' || c == L'】') c = L']';
            else if (c == L'〜') c = L'～';
        }
        for (const auto& word : delWords) {
            size_t pos = 0;
            while ((pos = value.find(word, pos)) != std::wstring::npos) value.erase(pos, word.size());
        }
        size_t output = 0;
        for (const auto c : value) {
            if (c == L' ' && output && value[output - 1] == L' ') continue;
            value[output++] = c;
        }
        value.resize(output);
        return Trim(value);
    }
};

// One instance per directory; names include files outside the rename target types.
class RenameSession {
    std::set<std::wstring, OrdinalLess> occupied;
    std::map<std::pair<std::wstring, std::wstring>, size_t> nextNumbers;
public:
    void AddName(const std::wstring& name) { occupied.insert(name); }
    bool Rename(const std::filesystem::path& source, bool isDirectory, const HeaderRename& rules) {
        const auto original = source.filename().wstring();
        const auto stem = rules.Transform(isDirectory ? original : source.stem().wstring());
        const auto extension = isDirectory ? L"" : source.extension().wstring();
        if (stem.empty() || stem == L"." || stem == L".." || stem.back() == L'.')
            throw std::runtime_error("Transformed name is empty or invalid");
        const auto base = stem + extension;
        if (base == original) return false;
        auto& next = nextNumbers[{stem, extension}];
        if (!next) next = 1;
        std::wstring candidate = base;
        for (;;) {
            const bool ownName = CompareStringOrdinal(original.c_str(), -1, candidate.c_str(), -1, TRUE) == CSTR_EQUAL;
            if (!occupied.contains(candidate) || ownName) {
                const auto destination = source.parent_path() / candidate;
                if (MoveFileExW(source.c_str(), destination.c_str(), 0)) {
                    occupied.erase(original);
                    occupied.insert(candidate);
                    std::cout << "Renamed: " << Utf8(original) << " -> " << Utf8(candidate) << '\n';
                    return true;
                }
                const auto error = GetLastError();
                // A newly created destination is a collision too, including directories.
                if (error != ERROR_ALREADY_EXISTS && error != ERROR_FILE_EXISTS &&
                    !(error == ERROR_ACCESS_DENIED && GetFileAttributesW(destination.c_str()) != INVALID_FILE_ATTRIBUTES))
                    throw std::runtime_error("Rename failed; Windows error " + std::to_string(error));
                occupied.insert(candidate);
            }
            candidate = stem + L"_" + std::to_wstring(next++) + extension;
        }
    }
};
