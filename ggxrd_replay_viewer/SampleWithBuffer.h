#pragma once
#include <atlbase.h>
#include <mfobjects.h>
#include <atomic>
struct SampleWithBuffer {
    CComPtr<IMFSample> sample;
    CComPtr<IMFMediaBuffer> buffer;
    DWORD alignment = 0;
    DWORD maxSize = 0;
    DWORD currentSize = 0;
    unsigned int matchCounter = 0;
    std::atomic_int refCount = 0;
    bool empty = true;
    bool isEof = false;
    void resize(DWORD cbAlignment, DWORD cbSize);
};

