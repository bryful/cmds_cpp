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
            const auto candidate = parent / (L".d2z-" + std::to_wstring(GetCurrentProcessId()) +
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

void Compress(const fs::path& executable, const fs::path& target, const fs::path& archive, const std::wstring& mode) {
    std::wstring command = Quote(executable.wstring()) + L" a -tzip -y -sse -bd -bb0 -sccUTF-8 " +
        mode + L" -- " + Quote(archive.wstring()) + L" \"*\"";
    STARTUPINFOW startup = {sizeof(startup)};
    PROCESS_INFORMATION process = {};
    if (!CreateProcessW(executable.c_str(), command.data(), nullptr, nullptr, FALSE, 0,
        nullptr, target.c_str(), &startup, &process)) throw WindowsError("Start 7-Zip");
    Handle thread{process.hThread};
    Handle child{process.hProcess};
    if (WaitForSingleObject(child.value, INFINITE) != WAIT_OBJECT_0) throw WindowsError("Wait for 7-Zip");
    DWORD code = 0;
    if (!GetExitCodeProcess(child.value, &code)) throw WindowsError("Get 7-Zip exit code");
    if (code != 0) throw std::runtime_error("7-Zip failed with exit code " + std::to_string(code));
}

void DirToZip(const fs::path& input, const fs::path& executable, const std::wstring& mode) {
    auto directory = fs::absolute(input).lexically_normal();
    while (directory.filename().empty() && directory != directory.root_path()) directory = directory.parent_path();
    if (directory == directory.root_path()) throw std::runtime_error("A drive root cannot be archived");
    if (!fs::is_directory(directory)) throw std::runtime_error("Directory not found");
    auto destination = directory;
    destination += L".zip";
    if (fs::exists(fs::symlink_status(destination))) throw std::runtime_error("Output already exists");

    // One pass, at most two entries: unwrap exactly one child directory.
    auto target = directory;
    {
        auto entries = fs::directory_iterator(directory);
        if (entries != fs::directory_iterator()) {
            const auto first = entries->path();
            if (entries->is_directory() && ++entries == fs::directory_iterator()) target = first;
        }
    }
    StagingDirectory staging(directory.parent_path());
    const auto temporaryZip = staging.path / L"result.zip";
    Compress(executable, target, temporaryZip, mode);
    if (!fs::is_regular_file(temporaryZip)) throw std::runtime_error("7-Zip did not create an archive");
    {
        Handle file{CreateFileW(temporaryZip.c_str(), GENERIC_WRITE, FILE_SHARE_READ, nullptr,
            OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr)};
        if (file.value == INVALID_HANDLE_VALUE) throw WindowsError("Open completed ZIP");
        if (!FlushFileBuffers(file.value)) throw WindowsError("Flush completed ZIP");
        const auto completed = file.value;
        file.value = INVALID_HANDLE_VALUE;
        if (!CloseHandle(completed)) throw WindowsError("Close completed ZIP");
    }
    // No replacement flag: a destination created while compressing is protected too.
    if (!MoveFileExW(temporaryZip.c_str(), destination.c_str(), MOVEFILE_WRITE_THROUGH))
        throw WindowsError("Publish ZIP (destination may already exist)");
    std::cout << "Created: " << Text(destination) << '\n';
}

int wmain(int argc, wchar_t* argv[]) {
    SetConsoleOutputCP(CP_UTF8);
    try {
        std::wstring mode;
        std::vector<fs::path> inputs;
        bool options = true;
        for (int i = 1; i < argc; ++i) {
            const std::wstring argument(argv[i]);
            if (options && argument == L"--") { options = false; continue; }
            if (options && (argument == L"--fast" || argument == L"--store")) {
                if (!mode.empty()) throw std::runtime_error("Choose only one compression mode");
                mode = argument == L"--fast" ? L"-mx1" : L"-mx0";
            } else {
                if (options && argument.starts_with(L"-")) throw std::runtime_error("Unknown option; use -- before a directory starting with '-'");
                inputs.emplace_back(argument);
            }
        }
        if (inputs.empty()) {
            std::cerr << "Usage: d2z [--fast|--store] [--] <directory> [...]\n";
            return 1;
        }
        const auto executable = LoadSevenZipPath();
        if (!fs::is_regular_file(executable)) throw std::runtime_error("7-Zip not found");
        size_t failed = 0;
        for (const auto& input : inputs) {
            try { DirToZip(input, executable, mode); }
            catch (const std::exception& error) {
                std::cerr << Text(input) << ": " << error.what() << '\n';
                ++failed;
            }
        }
        std::cout << "Processed: " << inputs.size() << ", failed: " << failed << '\n';
        return failed ? 1 : 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
