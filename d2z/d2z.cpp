#include <Windows.h>
#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <vector>
#include <bit7z/bit7zlibrary.hpp>
#include <bit7z/bitfilecompressor.hpp>

namespace fs = std::filesystem;

std::string Text(const fs::path& path) {
    const auto value = path.u8string();
    return std::string(value.begin(), value.end());
}

std::runtime_error WindowsError(const char* operation) {
    return std::runtime_error(std::string(operation) + "; Windows error " + std::to_string(GetLastError()));
}

fs::path LoadSevenZipDll() {
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
                fs::path configured(path);
                if (CompareStringOrdinal(configured.extension().c_str(), -1, L".exe", -1, TRUE) == CSTR_EQUAL)
                    configured.replace_extension(L".dll");
                if (!fs::is_regular_file(configured)) throw std::runtime_error("Configured 7z.dll not found");
                return fs::absolute(configured);
            }
        }
    }
    const auto local = pref.parent_path() / L"7z.dll";
    if (fs::is_regular_file(local)) return local;
    return L"C:\\Program Files\\7-Zip\\7z.dll";
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

void Compress(const bit7z::Bit7zLibrary& library, const fs::path& target, const fs::path& archive,
    bit7z::BitCompressionLevel level) {
    bit7z::BitFileCompressor compressor(library, bit7z::BitFormat::Zip);
    compressor.setCompressionLevel(level);
    uint64_t total = 0;
    unsigned int lastStep = 0;
    compressor.setTotalCallback([&](uint64_t size) { total = size; });
    compressor.setProgressCallback([&](uint64_t completed) {
        if (total) {
            const auto percent = static_cast<unsigned int>(std::min(100.0L, 100.0L * completed / total));
            const auto step = percent / 10;
            if (step > lastStep) { lastStep = step; std::cout << "  Compressing: " << step * 10 << "%" << std::endl; }
        }
        return true;
    });
    compressor.compressDirectoryContents(target.wstring(), archive.wstring());
}

void DirToZip(const fs::path& input, const bit7z::Bit7zLibrary& library, bit7z::BitCompressionLevel level) {
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
    Compress(library, target, temporaryZip, level);
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
        auto level = bit7z::BitCompressionLevel::Normal;
        bool modeSet = false;
        fs::path dll;
        std::vector<fs::path> inputs;
        bool options = true;
        for (int i = 1; i < argc; ++i) {
            const std::wstring argument(argv[i]);
            if (options && argument == L"--") { options = false; continue; }
            if (options && (argument == L"--fast" || argument == L"--store")) {
                if (modeSet) throw std::runtime_error("Choose only one compression mode");
                modeSet = true;
                level = argument == L"--fast" ? bit7z::BitCompressionLevel::Fastest : bit7z::BitCompressionLevel::None;
            } else if (options && argument == L"--dll") {
                if (!dll.empty() || i + 1 == argc) throw std::runtime_error("--dll requires one DLL path");
                dll = fs::absolute(argv[++i]);
            } else {
                if (options && argument.starts_with(L"-")) throw std::runtime_error("Unknown option; use -- before a directory starting with '-'");
                inputs.emplace_back(argument);
            }
        }
        if (inputs.empty()) {
            std::cerr << "Usage: d2z [--dll path] [--fast|--store] [--] <directory> [...]\n";
            return 1;
        }
        if (dll.empty()) dll = LoadSevenZipDll();
        if (!fs::is_regular_file(dll)) throw std::runtime_error("7z.dll not found");
        bit7z::Bit7zLibrary library(fs::absolute(dll).wstring());
        size_t failed = 0;
        size_t current = 0;
        for (const auto& input : inputs) {
            std::cout << '[' << ++current << '/' << inputs.size() << "] Compressing: " << Text(input) << std::endl;
            try { DirToZip(input, library, level); }
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
