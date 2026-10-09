#pragma once

#include <Windows.h>
#include <initguid.h>
#include "third_party/7zip/CPP/7zip/Archive/IArchive.h"
#include <atomic>
#include <filesystem>
#include <initializer_list>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "oleaut32.lib")

namespace sevenzip {

inline void Check(HRESULT hr, const char* message)
{
    if (hr != S_OK)
        throw std::runtime_error(std::string(message) + " (HRESULT=" +
            std::to_string(static_cast<unsigned long>(hr)) + ")");
}

template<class T> struct ReleaseObject {
    void operator()(T* p) const { if (p) p->Release(); }
};
template<class T> using ComPtr = std::unique_ptr<T, ReleaseObject<T>>;

#define SEVENZIP_REFS(Type, Interface, InterfaceId) \
    std::atomic<ULONG> refs_{1}; \
    ULONG STDMETHODCALLTYPE AddRef() noexcept override { return ++refs_; } \
    ULONG STDMETHODCALLTYPE Release() noexcept override { \
        ULONG n = --refs_; if (!n) delete this; return n; } \
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void** out) noexcept override { \
        if (!out) return E_POINTER; *out = nullptr; \
        if (iid == IID_IUnknown || iid == InterfaceId) { \
            *out = static_cast<Interface*>(this); AddRef(); return S_OK; } \
        return QueryExtra(iid, out); }

class FileStream final : public IInStream {
    HANDLE file_;
public:
    explicit FileStream(const std::filesystem::path& path) : file_(CreateFileW(path.c_str(),
        GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr))
    {
        if (file_ == INVALID_HANDLE_VALUE)
            throw std::runtime_error("Cannot open archive file");
    }
    ~FileStream() { CloseHandle(file_); }
    SEVENZIP_REFS(FileStream, IInStream, IID_IInStream)
    HRESULT QueryExtra(REFIID iid, void** out) noexcept {
        if (iid != IID_ISequentialInStream) return E_NOINTERFACE;
        *out = static_cast<ISequentialInStream*>(this); AddRef(); return S_OK;
    }
    HRESULT STDMETHODCALLTYPE Read(void* data, UInt32 size, UInt32* processed) noexcept override {
        DWORD n = 0;
        BOOL ok = ReadFile(file_, data, size, &n, nullptr);
        if (processed) *processed = n;
        return ok ? S_OK : HRESULT_FROM_WIN32(GetLastError());
    }
    HRESULT STDMETHODCALLTYPE Seek(Int64 offset, UInt32 origin, UInt64* position) noexcept override {
        if (origin > FILE_END) return E_INVALIDARG;
        LARGE_INTEGER move{}, result{}; move.QuadPart = offset;
        if (!SetFilePointerEx(file_, move, &result, origin))
            return HRESULT_FROM_WIN32(GetLastError());
        if (position) *position = result.QuadPart;
        return S_OK;
    }
};

class Property {
public:
    PROPVARIANT value{};
    Property() = default;
    Property(const Property&) = delete;
    Property& operator=(const Property&) = delete;
    ~Property() { PropVariantClear(&value); }
};

class Library {
    struct Unload { void operator()(HINSTANCE p) const { if (p) FreeLibrary(p); } };
    std::unique_ptr<std::remove_pointer_t<HMODULE>, Unload> module_;
    using CreateObjectFn = HRESULT (WINAPI*)(const GUID*, const GUID*, void**);
    CreateObjectFn create_ = nullptr;
    Library() {
        std::vector<wchar_t> exe(32768);
        DWORD length = GetModuleFileNameW(nullptr, exe.data(), static_cast<DWORD>(exe.size()));
        if (!length || length >= exe.size()) throw std::runtime_error("Cannot locate executable");
        auto dll = std::filesystem::path(std::wstring(exe.data(), length)).parent_path() / L"7z.dll";
        module_.reset(LoadLibraryExW(dll.c_str(), nullptr,
            LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_SYSTEM32));
        // 同梱DLLを優先する。存在するDLLの不整合は隠さず報告する。
        if (!module_ && GetLastError() == ERROR_MOD_NOT_FOUND && !std::filesystem::exists(dll)) {
            wchar_t programFiles[32768]{};
            const DWORD count = GetEnvironmentVariableW(L"ProgramFiles", programFiles, 32768);
            if (count && count < 32768) {
                dll = std::filesystem::path(programFiles) / L"7-Zip" / L"7z.dll";
                module_.reset(LoadLibraryExW(dll.c_str(), nullptr,
                    LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_SYSTEM32));
            }
        }
        if (!module_) throw std::runtime_error("Cannot load 7z.dll beside the executable or in Program Files/7-Zip (check x86/x64 architecture)");
        create_ = reinterpret_cast<CreateObjectFn>(GetProcAddress(module_.get(), "CreateObject"));
        if (!create_) throw std::runtime_error("7z.dll has no CreateObject export");
    }
public:
    static std::shared_ptr<Library> Load() {
        // Keep the DLL loaded across archives, rather than reloading it for each file.
        static auto library = std::shared_ptr<Library>(new Library);
        return library;
    }
    ComPtr<IInArchive> Create(Byte format) const {
        GUID clsid{0x23170F69, 0x40C1, 0x278A, {0x10, 0, 0, 1, 0x10, format, 0, 0}};
        IInArchive* raw = nullptr;
        HRESULT hr = create_(&clsid, &IID_IInArchive, reinterpret_cast<void**>(&raw));
        ComPtr<IInArchive> result(raw);
        if (hr != S_OK) return {};
        return result;
    }
};

class Archive {
    // The DLL must outlive its interfaces; members are destroyed in reverse order.
    std::shared_ptr<Library> library_;
    ComPtr<FileStream> stream_;
protected:
    ComPtr<IInArchive> archive_;
public:
    Archive(const std::filesystem::path& path, std::initializer_list<Byte> formats)
        : library_(Library::Load()), stream_(new FileStream(path)) {
        // Format class IDs: ZIP = 0x01, RAR = 0x03, RAR5 = 0xCC.
        for (Byte format : formats) {
            archive_ = library_->Create(format);
            if (!archive_) continue;
            Check(stream_->Seek(0, FILE_BEGIN, nullptr), "Cannot seek archive");
            const UInt64 checkStart = 0;
            if (archive_->Open(stream_.get(), &checkStart, nullptr) == S_OK) return;
            archive_->Close();
            archive_.reset();
        }
        throw std::runtime_error("Cannot open archive (damaged, encrypted headers, or unsupported format)");
    }
    ~Archive() { if (archive_) archive_->Close(); }
    void Close() {
        Check(archive_->Close(), "Cannot close archive");
        archive_.reset();
        stream_.reset();
    }
    IInArchive* Get() const { return archive_.get(); }
};

} // namespace sevenzip
