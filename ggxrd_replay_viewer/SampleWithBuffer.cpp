#include "SampleWithBuffer.h"
#include <mfapi.h>
#include "debugMsg.h"

void SampleWithBuffer::resize(DWORD cbAlignment, DWORD cbSize) {
    if (alignment == cbAlignment && maxSize >= cbSize) {
        maxSize = cbSize;
        return;
    }
    alignment = cbAlignment;
    maxSize = cbSize;
    HRESULT errorCode;
    if (sample) {
        sample->RemoveAllBuffers();
        sample = nullptr;
        buffer = nullptr;
    } else {
        errorCode = MFCreateSample(&sample);
        if (FAILED(errorCode)) {
            debugMsg("Failed to create a sample: %.8x\n", errorCode);
            return;
        }
    }

    if (alignment) {
        errorCode = MFCreateAlignedMemoryBuffer(maxSize, alignment, &buffer);
    }
    else {
        errorCode = MFCreateMemoryBuffer(maxSize, &buffer);
    }
    if (FAILED(errorCode)) {
        debugMsg("Failed to create a buffer: %.8x\n", errorCode);
        return;
    }
    errorCode = sample->AddBuffer(buffer);
    if (FAILED(errorCode)) {
        debugMsg("Failed to add buffer to sample: %.8x\n", errorCode);
        return;
    }
}
