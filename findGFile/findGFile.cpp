#define NOMINMAX
#include <Windows.h>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

namespace fs = std::filesystem;
constexpr std::uintmax_t BYTES_PER_MB = 1024 * 1024;

std::string toUtf8(std::wstring_view value)
{
    if (value.empty())
    {
        return {};
    }
    const int length = static_cast<int>(value.size());
    const int size = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, value.data(), length, nullptr, 0, nullptr, nullptr);
    if (size == 0)
    {
        throw std::runtime_error("UTF-8 conversion failed.");
    }
    std::string result(size, '\0');
    if (WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, value.data(), length, result.data(), size, nullptr, nullptr) == 0)
    {
        throw std::runtime_error("UTF-8 conversion failed.");
    }
    return result;
}

void reportError(const fs::path& path, const std::error_code& error)
{
    wchar_t message[2048]{};
    const DWORD length = FormatMessageW(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        nullptr, static_cast<DWORD>(error.value()), 0, message, 2048, nullptr);
    std::cerr << "Error: " << toUtf8(path.native()) << ": ";
    if (length != 0)
    {
        std::cerr << toUtf8(std::wstring_view(message, length));
    }
    else
    {
        std::cerr << "error code " << error.value();
    }
    std::cerr << '\n';
}

bool parseSize(std::wstring_view text, std::uintmax_t& bytes)
{
    if (text.empty())
    {
        return false;
    }
    constexpr auto MAX_MB = (std::numeric_limits<std::uintmax_t>::max)() / BYTES_PER_MB;
    std::uintmax_t value = 0;
    for (const wchar_t character : text)
    {
        if (character < L'0' || character > L'9')
        {
            return false;
        }
        const auto digit = static_cast<std::uintmax_t>(character - L'0');
        if (value > (MAX_MB - digit) / 10)
        {
            return false;
        }
        value = value * 10 + digit;
    }
    bytes = value * BYTES_PER_MB;
    return true;
}

int scan(const fs::path& root, std::uintmax_t minimumSize)
{
    std::vector<fs::path> directories{ root };
    bool hadError = false;
    while (!directories.empty())
    {
        const fs::path directory = std::move(directories.back());
        directories.pop_back();
        std::cerr << "Scanning: " << toUtf8(directory.native()) << '\n';
        std::error_code error;
        fs::directory_iterator iterator(directory, error);
        if (error)
        {
            reportError(directory, error);
            hadError = true;
            continue;
        }
        const fs::directory_iterator end;
        while (iterator != end)
        {
            const auto& entry = *iterator;
            const auto status = entry.status(error);
            if (error)
            {
                reportError(entry.path(), error);
                hadError = true;
            }
            else if (fs::is_directory(status))
            {
                const DWORD attributes = GetFileAttributesW(entry.path().c_str());
                if (attributes == INVALID_FILE_ATTRIBUTES)
                {
                    reportError(entry.path(), std::error_code(static_cast<int>(GetLastError()), std::system_category()));
                    hadError = true;
                }
                else if ((attributes & FILE_ATTRIBUTE_REPARSE_POINT) == 0)
                {
                    // ジャンクションなどのリンク先には再帰しない。
                    directories.push_back(entry.path());
                }
            }
            else if (fs::is_regular_file(status))
            {
                const auto size = entry.file_size(error);
                if (error)
                {
                    reportError(entry.path(), error);
                    hadError = true;
                }
                else if (size >= minimumSize)
                {
                    std::cout << toUtf8(entry.path().native()) << '\n';
                    if (!std::cout)
                    {
                        std::cerr << "Error: Failed to write file path to standard output.\n";
                        return 1;
                    }
                }
            }
            iterator.increment(error);
            if (error)
            {
                reportError(directory, error);
                hadError = true;
                break;
            }
        }
    }
    std::cout.flush();
    std::cerr.flush();
    return hadError || !std::cout || !std::cerr ? 1 : 0;
}

int wmain(int argc, wchar_t* argv[])
{
    SetConsoleOutputCP(CP_UTF8);
    try
    {
        fs::path path;
        bool hasPath = false;
        bool hasSize = false;
        std::uintmax_t minimumSize = 1024 * BYTES_PER_MB;
        for (int index = 1; index < argc; ++index)
        {
            const std::wstring_view argument(argv[index]);
            if (argument == L"-size")
            {
                if (hasSize || index + 1 >= argc || !parseSize(argv[++index], minimumSize))
                {
                    std::cerr << "Error: -size requires a nonnegative integer in MByte within range.\n";
                    return 1;
                }
                hasSize = true;
            }
            else if (argument.empty() || argument.front() == L'-' || hasPath)
            {
                std::cerr << "Error: Invalid argument: " << toUtf8(argument)
                    << "\nUsage: findGFile.exe [path] [-size MByte]\n";
                return 1;
            }
            else
            {
                path = argv[index];
                hasPath = true;
            }
        }
        std::error_code error;
        const fs::path root = hasPath ? fs::absolute(path, error) : fs::current_path(error);
        if (error)
        {
            reportError(hasPath ? path : fs::path(L"."), error);
            return 1;
        }
        if (!fs::is_directory(root, error))
        {
            reportError(root, error ? error : std::make_error_code(std::errc::not_a_directory));
            return 1;
        }
        return scan(root, minimumSize);
    }
    catch (const fs::filesystem_error& error)
    {
        reportError(error.path1(), error.code());
    }
    catch (const std::exception& error)
    {
        std::cerr << "Error: " << error.what() << '\n';
    }
    return 1;
}
