#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <algorithm>
#include <filesystem>
#include <sstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

// Console output is UTF-16; redirected files and pipes receive UTF-8 bytes.
// Avoid CRT Unicode stream modes, whose behavior depends on the output handle.
class Output {
public:
    explicit Output(DWORD kind) : handle_(GetStdHandle(kind)) {}

    template<class T> Output& operator<<(const T& value) {
        std::wostringstream stream;
        stream << value;
        write(stream.str());
        return *this;
    }
    explicit operator bool() const { return good_; }

private:
    void write(const std::wstring& text) {
        if (!good_ || text.empty()) return;
        DWORD mode = 0;
        if (GetConsoleMode(handle_, &mode)) {
            size_t offset = 0;
            while (offset < text.size()) {
                DWORD written = 0;
                const DWORD length = static_cast<DWORD>((std::min)(text.size() - offset, size_t{16384}));
                if (!WriteConsoleW(handle_, text.data() + offset, length, &written, nullptr) || !written) {
                    good_ = false;
                    return;
                }
                offset += written;
            }
        } else {
            const int length = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS,
                text.data(), static_cast<int>(text.size()), nullptr, 0, nullptr, nullptr);
            if (!length) { good_ = false; return; }
            std::string bytes(length, '\0');
            if (!WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, text.data(),
                static_cast<int>(text.size()), bytes.data(), length, nullptr, nullptr)) {
                good_ = false;
                return;
            }
            size_t offset = 0;
            while (offset < bytes.size()) {
                DWORD written = 0;
                if (!WriteFile(handle_, bytes.data() + offset,
                    static_cast<DWORD>(bytes.size() - offset), &written, nullptr) || !written) {
                    good_ = false;
                    return;
                }
                offset += written;
            }
        }
    }
    HANDLE handle_;
    bool good_ = true;
};

static Output output(STD_OUTPUT_HANDLE);
static Output errors(STD_ERROR_HANDLE);

static void usage()
{
    output << L"Usage: findName <path> <text> [text...] [-e <extension>]\n"
                  L"Recursively find files whose names contain all search texts (case-insensitive).\n"
                  L"Default extension: .zip. Both txt and .txt are accepted.\n"
                  L"Use -- before search texts beginning with '-'.\n"
                  L"Example: findName \"D:\\Archive\" sample backup -e zip\n";
}

static bool contains(const std::wstring& name, const std::wstring& text)
{
    return FindStringOrdinal(FIND_FROMSTART, name.c_str(),
        static_cast<int>(name.size()), text.c_str(),
        static_cast<int>(text.size()), TRUE) >= 0;
}

int wmain(int argc, wchar_t* argv[])
{
    if (argc == 2 && std::wstring(argv[1]) == L"--help") {
        usage();
        return output ? 0 : 1;
    }
    if (argc < 3) {
        usage();
        return 2;
    }

    std::wstring extension = L".zip";
    std::vector<std::wstring> texts;
    bool options = true;
    bool extensionSpecified = false;
    for (int i = 2; i < argc; ++i) {
        const std::wstring arg = argv[i];
        if (options && arg == L"--") {
            options = false;
        } else if (options && arg == L"-e") {
            if (extensionSpecified || i + 1 == argc) {
                errors << L"Error: -e requires one extension and may only be specified once.\n";
                return 2;
            }
            extension = argv[++i];
            extensionSpecified = true;
            if (extension.empty() || extension.find_first_of(L"\\/:*?\"<>| ") != std::wstring::npos
                || extension.front() == L'-') {
                errors << L"Error: invalid extension.\n";
                return 2;
            }
            if (extension.front() != L'.') extension.insert(extension.begin(), L'.');
            if (extension.size() == 1 || extension.find(L'.', 1) != std::wstring::npos) {
                errors << L"Error: specify a single extension, such as zip or .txt.\n";
                return 2;
            }
        } else {
            if (arg.empty() || (options && arg.front() == L'-')) {
                errors << L"Error: empty search text or unknown option: " << arg << L'\n';
                return 2;
            }
            texts.push_back(arg);
        }
    }
    if (texts.empty()) {
        errors << L"Error: at least one search text is required.\n";
        return 2;
    }

    try {
        std::error_code ec;
        const fs::path root = fs::absolute(argv[1], ec).lexically_normal();
        if (ec || !fs::is_directory(root, ec) || ec) {
            errors << L"Error: path is not an accessible directory: " << argv[1] << L'\n';
            return 1;
        }
        std::vector<fs::path> pending{ root };
        bool failed = false;
        const auto report = [&](const fs::path& path, const std::error_code& error) {
            errors << L"Error: cannot scan " << path.wstring()
                << L" (code " << error.value() << L")\n";
            failed = true;
        };
        while (!pending.empty()) {
            const fs::path directory = std::move(pending.back());
            pending.pop_back();
            fs::directory_iterator it(directory, ec), end;
            if (ec) { report(directory, ec); continue; }
            while (it != end) {
                const fs::path path = it->path();
                const DWORD attributes = GetFileAttributesW(path.c_str());
                if (attributes == INVALID_FILE_ATTRIBUTES) {
                    report(path, std::error_code(GetLastError(), std::system_category()));
                } else if (attributes & FILE_ATTRIBUTE_DIRECTORY) {
                    // Do not follow junctions/symlinks: cycles could otherwise repeat forever.
                    if (!(attributes & FILE_ATTRIBUTE_REPARSE_POINT)) pending.push_back(path);
                } else if (it->is_regular_file(ec)) {
                    const std::wstring suffix = path.extension().wstring();
                    if (CompareStringOrdinal(suffix.c_str(), static_cast<int>(suffix.size()),
                        extension.c_str(), static_cast<int>(extension.size()), TRUE) == CSTR_EQUAL) {
                        const std::wstring name = path.filename().wstring();
                        if (std::all_of(texts.begin(), texts.end(), [&](const auto& text) {
                            return contains(name, text);
                        })) output << path.wstring() << L'\n';
                    }
                }
                if (ec) { report(path, ec); ec.clear(); }
                it.increment(ec);
                if (ec) { report(directory, ec); break; }
            }
        }
        return failed || !output ? 1 : 0;
    } catch (const std::exception&) {
        errors << L"Error: file search failed.\n";
        return 1;
    }
}
