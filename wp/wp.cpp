#include <Windows.h>
#include <webp/decode.h>
#include <turbojpeg.h>
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <map>
#include <memory>
#include <string>
#include <stdexcept>
#include <vector>

namespace fs = std::filesystem;

std::string PathText(const fs::path& path) {
    const auto value = path.u8string();
    return std::string(value.begin(), value.end());
}

bool IsWebp(const fs::path& path) {
    const auto ext = path.extension().wstring();
    return CompareStringOrdinal(ext.c_str(), -1, L".webp", -1, TRUE) == CSTR_EQUAL;
}

std::wstring Digits(std::wstring value) {
    const auto first = value.find_first_not_of(L'0');
    return first == std::wstring::npos ? L"0" : value.substr(first);
}

fs::path JpegPath(const fs::path& input) {
    const auto stem = input.stem().wstring();
    const auto separator = stem.find(L'_', 5);
    if (stem.size() > 5 &&
        CompareStringOrdinal(stem.c_str(), 5, L"imgi_", 5, TRUE) == CSTR_EQUAL &&
        separator != std::wstring::npos) {
        const auto a = stem.substr(5, separator - 5);
        const auto b = stem.substr(separator + 1);
        if (!a.empty() && a == b && a.find_first_not_of(L"0123456789") == std::wstring::npos) {
            auto number = Digits(a);
            if (number.size() < 3) number.insert(0, 3 - number.size(), L'0');
            return input.parent_path() / (L"img" + number + L".jpeg");
        }
    }
    auto result = input;
    return result.replace_extension(L".jpeg");
}

struct Job {
    fs::path input, output;
    std::wstring number;
};

struct CaseInsensitiveLess {
    bool operator()(const std::wstring& a, const std::wstring& b) const {
        return CompareStringOrdinal(a.c_str(), -1, b.c_str(), -1, TRUE) == CSTR_LESS_THAN;
    }
};

// Own the temporary file until it has been published successfully.
struct TemporaryFile {
    fs::path path;
    HANDLE handle = INVALID_HANDLE_VALUE;
    ~TemporaryFile() {
        if (handle != INVALID_HANDLE_VALUE) CloseHandle(handle);
        if (!path.empty()) DeleteFileW(path.c_str());
    }
};

bool Convert(const Job& job, tjhandle encoder) {
    if (fs::exists(job.output)) throw std::runtime_error("Output already exists");
    std::ifstream file(job.input, std::ios::binary | std::ios::ate);
    if (!file) throw std::runtime_error("Cannot open WebP");
    const auto size = file.tellg();
    if (size <= 0 || static_cast<unsigned long long>(size) >
        (std::numeric_limits<size_t>::max)()) throw std::runtime_error("Invalid input size");
    std::vector<unsigned char> buffer(static_cast<size_t>(size));
    file.seekg(0);
    if (!file.read(reinterpret_cast<char*>(buffer.data()), static_cast<std::streamsize>(size)))
        throw std::runtime_error("Cannot read WebP");
    file.close();
    int width = 0, height = 0;
    std::unique_ptr<unsigned char, decltype(&WebPFree)> rgb(
        WebPDecodeRGB(buffer.data(), buffer.size(), &width, &height), WebPFree);
    if (!rgb) throw std::runtime_error("WebP decoding failed");
    unsigned char* rawJpeg = nullptr;
    unsigned long jpegSize = 0;
    const int status = tjCompress2(encoder, rgb.get(), width, 0, height, TJPF_RGB,
        &rawJpeg, &jpegSize, TJSAMP_420, 75, 0);
    std::unique_ptr<unsigned char, decltype(&tjFree)> jpeg(rawJpeg, tjFree);
    if (status != 0) throw std::runtime_error(tjGetErrorStr());

    TemporaryFile temp;
    for (unsigned int attempt = 0; attempt < 100; ++attempt) {
        temp.path = job.output;
        temp.path += L".wp-" + std::to_wstring(GetCurrentProcessId()) + L"-" +
            std::to_wstring(attempt) + L".tmp";
        temp.handle = CreateFileW(temp.path.c_str(), GENERIC_WRITE, 0, nullptr,
            CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (temp.handle != INVALID_HANDLE_VALUE) break;
        const auto error = GetLastError();
        temp.path.clear(); // Never delete a temporary file owned by another process.
        if (error != ERROR_FILE_EXISTS && error != ERROR_ALREADY_EXISTS)
            throw std::runtime_error("Cannot create temporary JPEG");
    }
    if (temp.handle == INVALID_HANDLE_VALUE) throw std::runtime_error("Temporary name unavailable");
    size_t offset = 0;
    while (offset < jpegSize) {
        const DWORD count = static_cast<DWORD>((std::min)(size_t(jpegSize) - offset, size_t(1 << 20)));
        DWORD written = 0;
        if (!WriteFile(temp.handle, jpeg.get() + offset, count, &written, nullptr) || written == 0)
            throw std::runtime_error("JPEG writing failed");
        offset += written;
    }
    if (!FlushFileBuffers(temp.handle)) throw std::runtime_error("JPEG flushing failed");
    const auto handle = temp.handle;
    temp.handle = INVALID_HANDLE_VALUE;
    if (!CloseHandle(handle)) throw std::runtime_error("JPEG closing failed");
    // Without REPLACE_EXISTING, a destination created after preflight is protected too.
    if (!MoveFileExW(temp.path.c_str(), job.output.c_str(), MOVEFILE_WRITE_THROUGH))
        throw std::runtime_error("Cannot publish JPEG; Windows error " + std::to_string(GetLastError()));
    temp.path.clear();
    if (!fs::remove(job.input)) throw std::runtime_error("Cannot remove source WebP; JPEG retained");
    std::cout << PathText(job.input.filename()) << " -> " << PathText(job.output.filename()) << '\n';
    return true;
}

int wmain(int argc, wchar_t* argv[]) {
    SetConsoleOutputCP(CP_UTF8);
    if (argc != 2) {
        std::cerr << "Usage: wp <input.webp|directory>\n";
        return 1;
    }
    try {
        const fs::path input(argv[1]);
        std::vector<Job> jobs;
        auto add = [&](const fs::path& path) {
            const auto stem = path.stem().wstring();
            const auto end = stem.find_last_not_of(L"0123456789");
            const auto number = Digits(stem.substr(end == std::wstring::npos ? 0 : end + 1));
            jobs.push_back({path, JpegPath(path), number});
        };
        if (fs::is_directory(input)) {
            for (const auto& entry : fs::directory_iterator(input))
                if (entry.is_regular_file() && IsWebp(entry.path())) add(entry.path());
        } else if (fs::is_regular_file(input) && IsWebp(input)) {
            add(input);
        } else throw std::runtime_error("Input must be a WebP file or directory");

        std::map<std::wstring, fs::path, CaseInsensitiveLess> outputs;
        bool conflict = false;
        for (const auto& job : jobs) {
            if (!outputs.emplace(job.output.wstring(), job.input).second || fs::exists(job.output)) {
                std::cerr << "Output conflict: " << PathText(job.output) << '\n';
                conflict = true;
            }
        }
        if (conflict) return 1;
        std::sort(jobs.begin(), jobs.end(), [](const Job& a, const Job& b) {
            if (a.number.size() != b.number.size()) return a.number.size() < b.number.size();
            if (a.number != b.number) return a.number < b.number;
            return a.input.native() < b.input.native();
        });
        struct EncoderDeleter { void operator()(void* p) const { tjDestroy(p); } };
        std::unique_ptr<void, EncoderDeleter> encoder(tjInitCompress());
        if (!encoder) throw std::runtime_error("Cannot initialize JPEG encoder");
        size_t failures = 0;
        for (const auto& job : jobs) {
            try { Convert(job, encoder.get()); }
            catch (const std::exception& e) {
                std::cerr << PathText(job.input) << ": ";
                std::cerr << e.what() << '\n';
                ++failures;
            }
        }
        std::cout << "Processed: " << jobs.size() << ", failed: " << failures << '\n';
        return failures == 0 ? 0 : 1;
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
