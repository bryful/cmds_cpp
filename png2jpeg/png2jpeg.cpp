#define NOMINMAX
#include <Windows.h>
#include <wincodec.h>
#include <wrl/client.h>
#include <algorithm>
#include <filesystem>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace fs = std::filesystem;
using Microsoft::WRL::ComPtr;

std::string utf8(std::wstring_view value)
{
    if (value.empty()) return {};
    const int size = WideCharToMultiByte(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), nullptr, 0, nullptr, nullptr);
    std::string result(size, '\0');
    WideCharToMultiByte(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), result.data(), size, nullptr, nullptr);
    return result;
}

void check(HRESULT result)
{
    if (SUCCEEDED(result)) return;
    wchar_t message[1024]{};
    FormatMessageW(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, nullptr,
        static_cast<DWORD>(result), 0, message, 1024, nullptr);
    throw std::runtime_error("HRESULT " + std::to_string(static_cast<unsigned long>(result)) + ": " + utf8(message));
}

fs::path nativePath(const fs::path& path)
{
    auto value = fs::absolute(path).lexically_normal().wstring();
    if (value.starts_with(L"\\\\?\\")) return value;
    if (value.starts_with(L"\\\\")) return L"\\\\?\\UNC\\" + value.substr(2);
    return L"\\\\?\\" + value;
}

bool isPng(const fs::path& path)
{
    return _wcsicmp(path.extension().c_str(), L".png") == 0;
}

bool matches(std::wstring_view name, std::wstring_view pattern)
{
    size_t i = 0, j = 0, star = std::wstring_view::npos, retry = 0;
    while (i < name.size())
    {
        if (j < pattern.size() && (pattern[j] == L'?' ||
            CompareStringOrdinal(name.data() + i, 1, pattern.data() + j, 1, TRUE) == CSTR_EQUAL))
        {
            ++i;
            ++j;
        }
        else if (j < pattern.size() && pattern[j] == L'*')
        {
            star = j++;
            retry = i;
        }
        else if (star != std::wstring_view::npos)
        {
            j = star + 1;
            i = ++retry;
        }
        else return false;
    }
    while (j < pattern.size() && pattern[j] == L'*') ++j;
    return j == pattern.size();
}

struct ComScope
{
    ComScope() { check(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED)); }
    ~ComScope() { CoUninitialize(); }
};

struct TemporaryFile
{
    fs::path path;
    explicit TemporaryFile(const fs::path& directory)
    {
        GUID id{};
        check(CoCreateGuid(&id));
        wchar_t name[40]{};
        StringFromGUID2(id, name, 40);
        path = directory / (std::wstring(L".png2jpeg-") + name + L".tmp");
        const HANDLE handle = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (handle == INVALID_HANDLE_VALUE) check(HRESULT_FROM_WIN32(GetLastError()));
        CloseHandle(handle);
    }
    ~TemporaryFile()
    {
        if (!DeleteFileW(path.c_str()))
        {
            const DWORD error = GetLastError();
            if (error != ERROR_FILE_NOT_FOUND)
                std::cerr << "一時ファイル削除失敗: " << utf8(path.wstring()) << " (Win32 " << error << ")\n";
        }
    }
};

void encode(IWICImagingFactory* factory, const fs::path& input, const fs::path& output)
{
    ComPtr<IWICBitmapDecoder> decoder;
    check(factory->CreateDecoderFromFilename(input.c_str(), nullptr, GENERIC_READ, WICDecodeMetadataCacheOnDemand, &decoder));
    GUID container{};
    check(decoder->GetContainerFormat(&container));
    if (container != GUID_ContainerFormatPng) throw std::runtime_error("PNG形式ではありません。");
    ComPtr<IWICBitmapFrameDecode> source;
    check(decoder->GetFrame(0, &source));
    UINT width = 0, height = 0;
    check(source->GetSize(&width, &height));
    if (!width || !height || width > static_cast<UINT>(std::numeric_limits<INT>::max()) ||
        height > static_cast<UINT>(std::numeric_limits<INT>::max()) || width > std::numeric_limits<UINT>::max() / 4)
        throw std::runtime_error("画像サイズが未対応です。");
    ComPtr<IWICFormatConverter> converter;
    check(factory->CreateFormatConverter(&converter));
    check(converter->Initialize(source.Get(), GUID_WICPixelFormat32bppPBGRA,
        WICBitmapDitherTypeNone, nullptr, 0, WICBitmapPaletteTypeCustom));
    ComPtr<IWICStream> stream;
    check(factory->CreateStream(&stream));
    check(stream->InitializeFromFilename(output.c_str(), GENERIC_WRITE));
    ComPtr<IWICBitmapEncoder> encoder;
    check(factory->CreateEncoder(GUID_ContainerFormatJpeg, nullptr, &encoder));
    check(encoder->Initialize(stream.Get(), WICBitmapEncoderNoCache));
    ComPtr<IWICBitmapFrameEncode> frame;
    ComPtr<IPropertyBag2> properties;
    check(encoder->CreateNewFrame(&frame, &properties));
    PROPBAG2 option{};
    wchar_t qualityName[] = L"ImageQuality";
    option.pstrName = qualityName;
    VARIANT quality{};
    quality.vt = VT_R4;
    quality.fltVal = 0.90f;
    check(properties->Write(1, &option, &quality));
    check(frame->Initialize(properties.Get()));
    check(frame->SetSize(width, height));
    WICPixelFormatGUID format = GUID_WICPixelFormat24bppBGR;
    check(frame->SetPixelFormat(&format));
    if (format != GUID_WICPixelFormat24bppBGR) throw std::runtime_error("JPEGのピクセル形式が未対応です。");
    std::vector<BYTE> rgba(static_cast<size_t>(width) * 4);
    std::vector<BYTE> bgr(static_cast<size_t>(width) * 3);
    for (UINT y = 0; y < height; ++y)
    {
        WICRect rect{0, static_cast<INT>(y), static_cast<INT>(width), 1};
        check(converter->CopyPixels(&rect, width * 4, width * 4, rgba.data()));
        // 乗算済みのRGBを取り出すと、黒背景へのアルファ合成になる。
        for (size_t x = 0; x < width; ++x)
            std::copy_n(rgba.data() + x * 4, 3, bgr.data() + x * 3);
        check(frame->WritePixels(1, width * 3, width * 3, bgr.data()));
    }
    check(frame->Commit());
    check(encoder->Commit());
    check(stream->Commit(STGC_DEFAULT));
}

int wmain(int argc, wchar_t* argv[])
{
    SetConsoleOutputCP(CP_UTF8);
    fs::path target;
    bool hasPath = false, nodelete = false, overwrite = false;
    for (int i = 1; i < argc; ++i)
    {
        const std::wstring_view arg = argv[i];
        if (arg == L"-nodelete") nodelete = true;
        else if (arg == L"-overwrite") overwrite = true;
        else if (arg.empty() || arg.front() == L'-' || hasPath)
        {
            std::cerr << "引数が不正です: " << utf8(arg) << "\n使用法: png2jpeg.exe [path] [-nodelete] [-overwrite]\n";
            return 2;
        }
        else { target = argv[i]; hasPath = true; }
    }
    try
    {
        if (!hasPath) target = fs::current_path();
        std::vector<fs::path> files;
        const bool wildcard = target.filename().wstring().find_first_of(L"*?") != std::wstring::npos;
        if (target.parent_path().wstring().find_first_of(L"*?") != std::wstring::npos &&
            !target.wstring().starts_with(L"\\\\?\\"))
            throw std::runtime_error("ワイルドカードはファイル名部分のみ指定できます。");
        const auto resolved = nativePath(target);
        if (wildcard || fs::is_directory(resolved))
        {
            const auto directory = wildcard ? resolved.parent_path() : resolved;
            const auto pattern = wildcard ? target.filename().wstring() : L"*";
            for (const auto& entry : fs::directory_iterator(directory))
                if (isPng(entry.path()) && matches(entry.path().filename().wstring(), pattern) && entry.is_regular_file())
                    files.push_back(entry.path());
        }
        else
        {
            if (!fs::is_regular_file(resolved)) throw std::runtime_error("入力ファイルが存在しないか、通常ファイルではありません。");
            if (!isPng(resolved)) throw std::runtime_error("拡張子が.pngではありません。");
            files.push_back(resolved);
        }
        ComScope com;
        ComPtr<IWICImagingFactory> factory;
        check(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&factory)));
        size_t converted = 0, skipped = 0, failed = 0;
        for (const auto& input : files)
        {
            try
            {
                auto output = input;
                output.replace_extension(L".jpeg");
                if (!overwrite && fs::exists(output))
                {
                    ++skipped;
                    std::cout << "スキップ: " << utf8(input.wstring()) << " (出力が存在します)\n";
                    continue;
                }
                TemporaryFile temporary(input.parent_path());
                encode(factory.Get(), input, temporary.path);
                if (!MoveFileExW(temporary.path.c_str(), output.c_str(), MOVEFILE_WRITE_THROUGH | (overwrite ? MOVEFILE_REPLACE_EXISTING : 0)))
                {
                    const DWORD error = GetLastError();
                    if (!overwrite && (error == ERROR_ALREADY_EXISTS || error == ERROR_FILE_EXISTS))
                    {
                        ++skipped;
                        std::cout << "スキップ: " << utf8(input.wstring()) << " (出力が存在します)\n";
                        continue;
                    }
                    check(HRESULT_FROM_WIN32(error));
                }
                if (!nodelete && !DeleteFileW(input.c_str())) check(HRESULT_FROM_WIN32(GetLastError()));
                ++converted;
                std::cout << "変換完了: " << utf8(input.wstring()) << " -> " << utf8(output.wstring()) << '\n';
            }
            catch (const std::exception& error)
            {
                ++failed;
                std::cerr << "失敗: " << utf8(input.wstring()) << ": " << error.what() << '\n';
            }
        }
        std::cout << "変換: " << converted << " / スキップ: " << skipped << " / 失敗: " << failed << '\n';
        return failed ? 1 : 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "エラー: " << utf8(target.wstring()) << ": " << error.what() << '\n';
        return 1;
    }
}
