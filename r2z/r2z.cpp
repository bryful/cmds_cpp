#include <Windows.h>
#include <filesystem>
#include <algorithm>
#include <cstdint>
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
            const auto candidate = parent / (L".r2z-" + std::to_wstring(GetCurrentProcessId()) +
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

#include <bit7z/bit7zlibrary.hpp>
#include <bit7z/bitarchivereader.hpp>
#include <bit7z/bitfilecompressor.hpp>
#include <set>

struct OrdinalLess {
    bool operator()(const std::wstring& a, const std::wstring& b) const {
        return CompareStringOrdinal(a.c_str(), -1, b.c_str(), -1, TRUE) == CSTR_LESS_THAN;
    }
};

bool Equal(const std::wstring& a, const wchar_t* b) {
    return CompareStringOrdinal(a.c_str(), -1, b, -1, TRUE) == CSTR_EQUAL;
}

void ValidatePath(const std::wstring& name) {
    if (name.empty() || name.find_first_of(L":<>\"|?*\0", 0, 8) != std::wstring::npos)
        throw std::runtime_error("Unsafe archive entry name");
    const fs::path path(name);
    if (path.has_root_path()) throw std::runtime_error("Absolute archive entry path is not supported");
    for (const auto& component : path) {
        const auto part = component.wstring();
        if (part == L"." || part == L".." || (!part.empty() && (part.back() == L'.' || part.back() == L' ')))
            throw std::runtime_error("Unsafe archive entry path");
    }
}

const bit7z::BitInFormat& RarFormat(const fs::path& path) {
    std::ifstream input(path, std::ios::binary);
    char signature[8] = {};
    input.read(signature, sizeof(signature));
    const auto count = input.gcount();
    if (count >= 7 && std::string(signature, 6) == std::string("Rar!\x1a\x07", 6)) {
        if (signature[6] == 0) return bit7z::BitFormat::Rar;
        if (count == 8 && signature[6] == 1 && signature[7] == 0) return bit7z::BitFormat::Rar5;
    }
    throw std::runtime_error("Input is not a RAR4/RAR5 archive");
}

struct Progress {
    const char* stage;
    uint64_t total = 0;
    unsigned int lastStep = 0;

    template<class Handler> void Attach(Handler& handler) {
        std::cout << "  " << stage << ": starting" << std::endl;
        handler.setTotalCallback([this](uint64_t size) { total = size; });
        handler.setProgressCallback([this](uint64_t completed) {
            if (total) {
                const auto percent = static_cast<unsigned int>(std::min(100.0L, 100.0L * completed / total));
                const auto step = percent / 10;
                if (step > lastStep) {
                    lastStep = step;
                    std::cout << "  " << stage << ": " << step * 10 << "%" << std::endl;
                }
            }
            return true;
        });
    }
};

void Convert(const fs::path& input, const bit7z::Bit7zLibrary& library, bit7z::BitCompressionLevel level) {
    const auto archive = fs::absolute(input);
    if (!Equal(archive.extension().wstring(), L".rar") || !fs::is_regular_file(archive))
        throw std::runtime_error("Input must be an existing .rar file");
    auto output = archive;
    output.replace_extension(L".zip");
    if (fs::exists(fs::symlink_status(output))) throw std::runtime_error("Output already exists");
    StagingDirectory staging(archive.parent_path());
    const auto contents = staging.path / L"contents";
    fs::create_directory(contents);
    size_t excluded = 0;
    {
        Progress extracting{"Extracting"};
        bit7z::BitArchiveReader reader(library, archive.wstring(), RarFormat(archive));
        bit7z::IndicesVector selected;
        std::set<std::wstring, OrdinalLess> names;
        for (const auto& item : reader) {
            const auto name = item.path();
            ValidatePath(name);
            const bool directory = item.isDir();
            if (!directory && Equal(fs::path(name).extension().wstring(), L".scr")) { ++excluded; continue; }
            if (item.isEncrypted()) throw std::runtime_error("Encrypted archives are not supported");
            if ((item.attributes() & FILE_ATTRIBUTE_REPARSE_POINT) ||
                !reader.itemProperty(item.index(), bit7z::BitProperty::SymLink).isEmpty() ||
                !reader.itemProperty(item.index(), bit7z::BitProperty::HardLink).isEmpty())
                throw std::runtime_error("Archive links are not supported");
            auto key = fs::path(name).lexically_normal().wstring();
            while (!key.empty() && (key.back() == L'\\' || key.back() == L'/')) key.pop_back();
            if (!names.insert(key).second) throw std::runtime_error("Duplicate archive entry path");
            selected.push_back(item.index());
        }
        // An empty index vector means 'extract everything' in bit7z: never pass it here.
        if (selected.empty()) throw std::runtime_error("Archive has no entries after filtering");
        extracting.Attach(reader);
        reader.extractTo(contents.wstring(), selected);
    }
    std::cout << "  Checking extracted files" << std::endl;
    for (const auto& entry : fs::recursive_directory_iterator(contents)) {
        const auto attributes = GetFileAttributesW(entry.path().c_str());
        if (attributes == INVALID_FILE_ATTRIBUTES) throw WindowsError("Inspect extracted entry");
        if (attributes & FILE_ATTRIBUTE_REPARSE_POINT) throw std::runtime_error("Extracted links are not supported");
        if (!entry.is_directory() && Equal(entry.path().extension().wstring(), L".scr"))
            throw std::runtime_error("Unexpected .scr file after extraction");
    }
    auto target = contents;
    {
        auto entries = fs::directory_iterator(contents);
        if (entries != fs::directory_iterator()) {
            const auto first = entries->path();
            if (entries->is_directory() && ++entries == fs::directory_iterator()) target = first;
        }
    }
    const auto temporaryZip = staging.path / L"result.zip";
    Progress compressing{"Compressing"};
    bit7z::BitFileCompressor compressor(library, bit7z::BitFormat::Zip);
    compressor.setCompressionLevel(level);
    compressing.Attach(compressor);
    // The API preserves relative paths without changing the process working directory.
    compressor.compressDirectoryContents(target.wstring(), temporaryZip.wstring());
    std::cout << "  Finalizing ZIP" << std::endl;
    {
        Handle file{CreateFileW(temporaryZip.c_str(), GENERIC_WRITE, FILE_SHARE_READ, nullptr,
            OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr)};
        if (file.value == INVALID_HANDLE_VALUE) throw WindowsError("Open completed ZIP");
        if (!FlushFileBuffers(file.value)) throw WindowsError("Flush completed ZIP");
        const auto completed = file.value;
        file.value = INVALID_HANDLE_VALUE;
        if (!CloseHandle(completed)) throw WindowsError("Close completed ZIP");
    }
    if (!MoveFileExW(temporaryZip.c_str(), output.c_str(), MOVEFILE_WRITE_THROUGH)) throw WindowsError("Publish ZIP");
    std::cout << "Created: " << Text(output) << ", excluded .scr: " << excluded << '\n';
}

int wmain(int argc, wchar_t* argv[]) {
    SetConsoleOutputCP(CP_UTF8);
    try {
        std::vector<fs::path> inputs;
        fs::path dll;
        auto level = bit7z::BitCompressionLevel::Normal;
        bool modeSet = false, options = true;
        for (int i = 1; i < argc; ++i) {
            const std::wstring arg(argv[i]);
            if (options && arg == L"--") { options = false; continue; }
            if (options && arg == L"--dll") {
                if (!dll.empty() || i + 1 == argc) throw std::runtime_error("--dll requires one DLL path");
                dll = fs::absolute(argv[++i]);
            } else if (options && (arg == L"--fast" || arg == L"--store")) {
                if (modeSet) throw std::runtime_error("Choose only one compression mode");
                modeSet = true;
                level = arg == L"--fast" ? bit7z::BitCompressionLevel::Fastest : bit7z::BitCompressionLevel::None;
            } else {
                if (options && arg.starts_with(L"-")) throw std::runtime_error("Unknown option");
                inputs.emplace_back(arg);
            }
        }
        if (inputs.empty()) { std::cerr << "Usage: r2z [--dll path] [--fast|--store] [--] <archive.rar> [...]\n"; return 1; }
        if (dll.empty()) dll = LoadSevenZipDll();
        if (!fs::is_regular_file(dll)) throw std::runtime_error("7z.dll not found");
        // Load once for the entire batch. No 7z.exe process is started.
        bit7z::Bit7zLibrary library(fs::absolute(dll).wstring());
        size_t failed = 0;
        size_t current = 0;
        for (const auto& input : inputs) {
            std::cout << '[' << ++current << '/' << inputs.size() << "] Converting: " << Text(input) << std::endl;
            try { Convert(input, library, level); }
            catch (const std::exception& error) { std::cerr << Text(input) << ": " << error.what() << '\n'; ++failed; }
        }
        std::cout << "Processed: " << inputs.size() << ", failed: " << failed << '\n';
        return failed ? 1 : 0;
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
