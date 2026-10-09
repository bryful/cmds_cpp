#include <iostream>
#include <filesystem>
#include <vector>
#include "HeaderRename.h"

namespace fs = std::filesystem;

fs::path RulePath() {
    std::vector<wchar_t> buffer(512);
    for (;;) {
        const auto length = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
        if (!length) throw std::runtime_error("Cannot locate executable");
        if (length < buffer.size()) {
            fs::path path(std::wstring(buffer.data(), length));
            return path.replace_extension(L".lst");
        }
        buffer.resize(buffer.size() * 2);
    }
}

bool Supported(const fs::path& path) {
    const auto extension = path.extension().wstring();
    for (const auto* allowed : {L".zip", L".rar", L".7z", L".mp4", L".mov", L".mpg", L".mpeg"})
        if (CompareStringOrdinal(extension.c_str(), -1, allowed, -1, TRUE) == CSTR_EQUAL) return true;
    return false;
}

int wmain(int argc, wchar_t* argv[]) {
    SetConsoleOutputCP(CP_UTF8);
    if (argc > 2) { std::cerr << "Usage: rh [directory]\n"; return 1; }
    try {
        const auto target = argc == 1 ? fs::current_path() : fs::absolute(argv[1]);
        if (!fs::is_directory(target)) throw std::runtime_error("Input must be an existing directory");
        HeaderRename rules;
        const auto path = RulePath();
        if (!rules.LoadWords(path)) rules.SaveWords(path);
        struct Item { fs::path path; bool directory; bool applyRules; };
        std::vector<Item> items;
        RenameSession session;
        for (const auto& entry : fs::directory_iterator(target)) {
            session.AddName(entry.path().filename().wstring());
            const auto attributes = GetFileAttributesW(entry.path().c_str());
            if (attributes == INVALID_FILE_ATTRIBUTES) throw std::runtime_error("Cannot inspect directory entry");
            // Links are left untouched; do not follow them to determine their type.
            if (attributes & FILE_ATTRIBUTE_REPARSE_POINT) continue;
            const bool directory = (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
            items.push_back({entry.path(), directory, directory || Supported(entry.path())});
        }
        std::sort(items.begin(), items.end(), [](const Item& a, const Item& b) { return a.path.native() < b.path.native(); });
        size_t changed = 0, unchanged = 0, failed = 0;
        for (const auto& item : items) {
            try {
                if (session.Rename(item.path, item.directory, rules, item.applyRules)) ++changed;
                else ++unchanged;
            } catch (const std::exception& error) {
                std::cerr << Utf8(item.path.wstring()) << ": " << error.what() << '\n';
                ++failed;
            }
        }
        std::cout << "Renamed: " << changed << ", unchanged: " << unchanged << ", failed: " << failed << '\n';
        return failed ? 1 : 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
