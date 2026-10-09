#pragma once
#include <Windows.h>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace fs = std::filesystem;

inline std::string Text(const fs::path& path) {
    const auto value = path.u8string();
    return std::string(value.begin(), value.end());
}

inline std::runtime_error WindowsError(const char* operation) {
    return std::runtime_error(std::string(operation) + "; Windows error " + std::to_string(GetLastError()));
}

inline DWORD Attributes(const fs::path& path) {
    const auto attributes = GetFileAttributesW(path.c_str());
    if (attributes == INVALID_FILE_ATTRIBUTES) throw WindowsError("Inspect path");
    return attributes;
}

inline void RequireDirectory(const fs::path& path) {
    const auto attributes = Attributes(path);
    if (attributes & FILE_ATTRIBUTE_REPARSE_POINT) throw std::runtime_error("Directory links are not supported");
    if (!(attributes & FILE_ATTRIBUTE_DIRECTORY)) throw std::runtime_error("Path is not a directory");
}

inline void ValidateRoot(const fs::path& root) {
    // Also reject a link in an explicitly supplied ancestor path.
    for (auto path = root;; path = path.parent_path()) {
        RequireDirectory(path);
        if (path == path.root_path()) break;
    }
}

inline bool Equal(const std::wstring& text, const wchar_t* other) {
    return CompareStringOrdinal(text.c_str(), -1, other, -1, TRUE) == CSTR_EQUAL;
}

inline void ValidateName(const std::wstring& name) {
    if (name.empty() || name == L"." || name == L".." || name.back() == L'.' || name.back() == L' ' ||
        name.find_first_of(L"\\/:<>\"|?*", 0) != std::wstring::npos)
        throw std::runtime_error("Unsafe destination directory name");
    for (const auto c : name) if (c < 32) throw std::runtime_error("Unsafe destination directory name");
    const auto stem = name.substr(0, name.find(L'.'));
    if (Equal(stem, L"CON") || Equal(stem, L"PRN") || Equal(stem, L"AUX") || Equal(stem, L"NUL") ||
        Equal(stem, L"CONIN$") || Equal(stem, L"CONOUT$"))
        throw std::runtime_error("Reserved destination directory name");
    if (stem.size() == 4 && (Equal(stem.substr(0, 3), L"COM") || Equal(stem.substr(0, 3), L"LPT")) &&
        ((stem[3] >= L'0' && stem[3] <= L'9') || stem[3] == L'\u00b9' || stem[3] == L'\u00b2' || stem[3] == L'\u00b3'))
        throw std::runtime_error("Reserved destination directory name");
}

struct SortResult {
    size_t moved = 0, skipped = 0, failed = 0;
    void Error(const fs::path& path, const std::exception& error) {
        ++failed;
        std::cerr << Text(path) << ": " << error.what() << '\n';
    }
};

inline std::vector<fs::path> Entries(const fs::path& directory) {
    RequireDirectory(directory);
    std::vector<fs::path> paths;
    for (const auto& entry : fs::directory_iterator(directory)) paths.push_back(entry.path());
    return paths;
}

inline void MoveFile(const fs::path& source, const fs::path& destination, SortResult& result) {
    RequireDirectory(source.parent_path());
    RequireDirectory(destination.parent_path());
    const auto attributes = Attributes(source);
    if (attributes & (FILE_ATTRIBUTE_REPARSE_POINT | FILE_ATTRIBUTE_DIRECTORY))
        throw std::runtime_error("Only regular files can be moved");
    // No REPLACE_EXISTING or COPY_ALLOWED: collisions cannot overwrite data.
    if (!MoveFileExW(source.c_str(), destination.c_str(), 0)) throw WindowsError("Move file");
    ++result.moved;
    std::cout << "Moved: " << Text(source) << " -> " << Text(destination) << '\n';
}

inline void SortFile(const fs::path& file, SortResult& result) {
    const auto attributes = Attributes(file);
    if (attributes & (FILE_ATTRIBUTE_REPARSE_POINT | FILE_ATTRIBUTE_DIRECTORY)) { ++result.skipped; return; }
    const auto extension = file.extension().wstring();
    if (!Equal(extension, L".zip") && !Equal(extension, L".rar")) { ++result.skipped; return; }
    const auto name = file.filename().wstring();
    const auto opening = name.find(L'[');
    const auto closing = opening == std::wstring::npos ? opening : name.find(L']', opening + 1);
    if (closing == std::wstring::npos || closing == opening + 1) { ++result.skipped; return; }
    const auto group = name.substr(opening + 1, closing - opening - 1);
    ValidateName(group);
    const auto directory = file.parent_path() / group;
    if (!CreateDirectoryW(directory.c_str(), nullptr) && GetLastError() != ERROR_ALREADY_EXISTS)
        throw WindowsError("Create destination directory");
    RequireDirectory(directory);
    MoveFile(file, directory / file.filename(), result);
}

inline void ReverseDirectory(const fs::path& directory, SortResult& result) {
    const auto files = Entries(directory);
    for (const auto& file : files) {
        try {
            const auto attributes = Attributes(file);
            if (attributes & (FILE_ATTRIBUTE_REPARSE_POINT | FILE_ATTRIBUTE_DIRECTORY)) { ++result.skipped; continue; }
            MoveFile(file, directory.parent_path() / file.filename(), result);
        } catch (const std::exception& error) { result.Error(file, error); }
    }
    // Preserve pre-existing empty directories; remove only a directory we emptied.
    if (!files.empty()) {
        RequireDirectory(directory);
        if (fs::is_empty(directory) && !RemoveDirectoryW(directory.c_str())) throw WindowsError("Remove empty directory");
    }
}

inline SortResult Sort(const fs::path& root, bool reverse) {
    SortResult result;
    for (const auto& path : Entries(root)) {
        try {
            const auto attributes = Attributes(path);
            if (attributes & FILE_ATTRIBUTE_REPARSE_POINT) {
                ++result.skipped;
                std::cout << "Skipped link: " << Text(path) << '\n';
            } else if (reverse) {
                if (attributes & FILE_ATTRIBUTE_DIRECTORY) ReverseDirectory(path, result);
                else ++result.skipped;
            } else SortFile(path, result);
        } catch (const std::exception& error) { result.Error(path, error); }
    }
    return result;
}
