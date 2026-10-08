#include <Windows.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <vector>

namespace fs = std::filesystem;

std::string Text(const fs::path& path) {
    const auto value = path.u8string();
    return std::string(value.begin(), value.end());
}

std::runtime_error WindowsError(const char* operation) {
    return std::runtime_error(std::string(operation) + "; Windows error " + std::to_string(GetLastError()));
}

// Apply the Windows command-line quoting rules, including trailing backslashes.
std::wstring Quote(const std::wstring& value) {
    std::wstring result = L"\"";
    size_t slashes = 0;
    for (const auto c : value) {
        if (c == L'\\') { ++slashes; continue; }
        result.append(c == L'\"' ? slashes * 2 + 1 : slashes, L'\\');
        result += c;
        slashes = 0;
    }
    result.append(slashes * 2, L'\\');
    return result + L'\"';
}

fs::path LoadSevenZipPath() {
    std::vector<wchar_t> module(512);
    for (;;) {
        const auto length = GetModuleFileNameW(nullptr, module.data(), static_cast<DWORD>(module.size()));
        if (!length) throw WindowsError("GetModuleFileName");
        if (length < module.size()) { module.resize(length); break; }
        module.resize(module.size() * 2);
    }
    fs::path pref(std::wstring(module.begin(), module.end()));
    pref.replace_extension(L".pref");
    std::ifstream input(pref, std::ios::binary);
    if (input) {
        std::string line;
        std::getline(input, line);
        if (line.starts_with("\xef\xbb\xbf")) line.erase(0, 3);
        if (!line.empty()) {
            UINT codePage = CP_UTF8;
            int length = MultiByteToWideChar(codePage, MB_ERR_INVALID_CHARS, line.data(), static_cast<int>(line.size()), nullptr, 0);
            if (!length) {
                codePage = CP_ACP;
                length = MultiByteToWideChar(codePage, 0, line.data(), static_cast<int>(line.size()), nullptr, 0);
            }
            std::wstring path(length, 0);
            if (length) MultiByteToWideChar(codePage, 0, line.data(), static_cast<int>(line.size()), path.data(), length);
            const auto first = path.find_first_not_of(L" \t\r\n");
            if (first != std::wstring::npos) {
                path = path.substr(first, path.find_last_not_of(L" \t\r\n") - first + 1);
                if (fs::is_regular_file(path)) return fs::absolute(path);
            }
        }
    }
    return L"C:\\Program Files\\7-Zip\\7z.exe";
}

struct Handle {
    HANDLE value;
    ~Handle() { if (value && value != INVALID_HANDLE_VALUE) CloseHandle(value); }
};

struct StagingDirectory {
    fs::path path;
    explicit StagingDirectory(const fs::path& parent) {
        static unsigned long sequence = 0;
        for (unsigned int attempt = 0; attempt < 128; ++attempt) {
            const auto candidate = parent / (L".a2d-" + std::to_wstring(GetCurrentProcessId()) +
                L"-" + std::to_wstring(sequence++) + L".tmp");
            if (CreateDirectoryW(candidate.c_str(), nullptr)) { path = candidate; return; }
            if (GetLastError() != ERROR_ALREADY_EXISTS) throw WindowsError("Create staging directory");
        }
        throw std::runtime_error("No staging directory name available");
    }
    ~StagingDirectory() {
        if (!path.empty()) {
            std::error_code error;
            fs::remove_all(path, error);
            if (error) std::cerr << "Cannot clean staging directory: " << Text(path) << '\n';
        }
    }
};

void Extract(const fs::path& executable, const fs::path& archive, const fs::path& staging) {
    std::wstring command = Quote(executable.wstring()) + L" x -y -bd -bb0 -sccUTF-8 -ssc- -xr!*.scr " +
        Quote(L"-o" + staging.wstring()) + L" -- " + Quote(archive.wstring());
    STARTUPINFOW startup = {sizeof(startup)};
    PROCESS_INFORMATION process = {};
    if (!CreateProcessW(executable.c_str(), command.data(), nullptr, nullptr, FALSE, 0,
        nullptr, nullptr, &startup, &process)) throw WindowsError("Start 7-Zip");
    Handle thread{process.hThread};
    Handle child{process.hProcess};
    if (WaitForSingleObject(child.value, INFINITE) != WAIT_OBJECT_0) throw WindowsError("Wait for 7-Zip");
    DWORD code = 0;
    if (!GetExitCodeProcess(child.value, &code)) throw WindowsError("Get 7-Zip exit code");
    if (code != 0) throw std::runtime_error("7-Zip failed with exit code " + std::to_string(code));
}

bool IsScr(const fs::path& path) {
    const auto extension = path.extension().wstring();
    return CompareStringOrdinal(extension.c_str(), -1, L".scr", -1, TRUE) == CSTR_EQUAL;
}

void ArcToDir(const fs::path& input, const fs::path& executable) {
    const auto archive = fs::absolute(input);
    if (!fs::is_regular_file(archive)) throw std::runtime_error("Archive file not found");
    const auto extension = archive.extension().wstring();
    if (CompareStringOrdinal(extension.c_str(), -1, L".zip", -1, TRUE) != CSTR_EQUAL &&
        CompareStringOrdinal(extension.c_str(), -1, L".rar", -1, TRUE) != CSTR_EQUAL)
        throw std::runtime_error("Only ZIP and RAR are supported");
    const auto destination = archive.parent_path() / archive.stem();
    if (destination.filename().empty() || destination.filename() == L"." || destination.filename() == L"..")
        throw std::runtime_error("Invalid output directory name");
    // symlink_status also detects dangling links, which must not be overwritten.
    if (fs::exists(fs::symlink_status(destination))) throw std::runtime_error("Output already exists");
    StagingDirectory staging(archive.parent_path());
    Extract(executable, archive, staging.path);

    // Do not publish link/reparse entries or a screen saver missed by the extractor.
    for (const auto& entry : fs::recursive_directory_iterator(staging.path)) {
        const auto attributes = GetFileAttributesW(entry.path().c_str());
        if (attributes == INVALID_FILE_ATTRIBUTES) throw WindowsError("Inspect extracted entry");
        if (attributes & FILE_ATTRIBUTE_REPARSE_POINT) throw std::runtime_error("Extracted links are not supported");
        if (!entry.is_directory() && IsScr(entry.path())) throw std::runtime_error("Unexpected .scr file after extraction");
    }
    auto contents = fs::directory_iterator(staging.path);
    if (contents == fs::directory_iterator()) throw std::runtime_error("Archive has no publishable entries");
    const auto first = contents->path();
    const bool singleDirectory = contents->is_directory() && ++contents == fs::directory_iterator();
    const auto source = singleDirectory ? first : staging.path;
    // Same-volume directory rename: publish all entries together, never replace a destination.
    if (!MoveFileExW(source.c_str(), destination.c_str(), MOVEFILE_WRITE_THROUGH))
        throw WindowsError("Publish output (destination may already exist)");
    if (!singleDirectory) staging.path.clear();
    std::cout << "Extracted to: " << Text(destination) << '\n';
}

int wmain(int argc, wchar_t* argv[]) {
    SetConsoleOutputCP(CP_UTF8);
    if (argc < 2) { std::cerr << "Usage: a2d <archive.zip|archive.rar> [...]\n"; return 1; }
    try {
        const auto executable = LoadSevenZipPath();
        if (!fs::is_regular_file(executable)) throw std::runtime_error("7-Zip not found");
        size_t failed = 0;
        for (int i = 1; i < argc; ++i) {
            try { ArcToDir(argv[i], executable); }
            catch (const std::exception& error) {
                std::cerr << Text(argv[i]) << ": " << error.what() << '\n';
                ++failed;
            }
        }
        std::cout << "Processed: " << argc - 1 << ", failed: " << failed << '\n';
        return failed ? 1 : 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
