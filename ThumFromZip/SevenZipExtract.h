#pragma once
#include "SevenZipArchive.h"

namespace sevenzip {

class OutputStream final : public ISequentialOutStream {
    HANDLE file_;
public:
    explicit OutputStream(HANDLE file) : file_(file) {}
    SEVENZIP_REFS(OutputStream, ISequentialOutStream, IID_ISequentialOutStream)
    HRESULT QueryExtra(REFIID, void**) noexcept { return E_NOINTERFACE; }
    HRESULT STDMETHODCALLTYPE Write(const void* data, UInt32 size, UInt32* processed) noexcept override {
        DWORD written = 0;
        BOOL ok = WriteFile(file_, data, size, &written, nullptr);
        if (processed) *processed = written;
        if (!ok) return HRESULT_FROM_WIN32(GetLastError());
        return written == size ? S_OK : E_FAIL;
    }
};

class ExtractCallback final : public IArchiveExtractCallback {
    ComPtr<OutputStream> stream_;
    UInt32 index_;
public:
    Int32 result = -1;
    ExtractCallback(HANDLE file, UInt32 index) : stream_(new OutputStream(file)), index_(index) {}
    SEVENZIP_REFS(ExtractCallback, IArchiveExtractCallback, IID_IArchiveExtractCallback)
    HRESULT QueryExtra(REFIID iid, void** out) noexcept {
        if (iid != IID_IProgress) return E_NOINTERFACE;
        *out = static_cast<IProgress*>(this); AddRef(); return S_OK;
    }
    HRESULT STDMETHODCALLTYPE SetTotal(UInt64) noexcept override { return S_OK; }
    HRESULT STDMETHODCALLTYPE SetCompleted(const UInt64*) noexcept override { return S_OK; }
    HRESULT STDMETHODCALLTYPE GetStream(UInt32 index, ISequentialOutStream** out, Int32 mode) noexcept override {
        if (!out) return E_POINTER;
        *out = nullptr;
        if (index != index_ || mode != NArchive::NExtract::NAskMode::kExtract) return E_FAIL;
        *out = stream_.get(); stream_->AddRef(); return S_OK;
    }
    HRESULT STDMETHODCALLTYPE PrepareOperation(Int32) noexcept override { return S_OK; }
    HRESULT STDMETHODCALLTYPE SetOperationResult(Int32 value) noexcept override {
        result = value; return S_OK;
    }
};

} // namespace sevenzip
