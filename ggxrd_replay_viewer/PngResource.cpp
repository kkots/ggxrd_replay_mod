#include "PngResource.h"
#include "WinError.h"
#include "png.h"
#include <vector>
#include "PixelA.h"
#include "debugMsg.h"

extern HDC hdcMem;

void loadPngResource(HINSTANCE hInstance, WORD resourceSymbolId, PngResource& pngResource, HDC hdc, const char* name, bool makeBitmap) {

    HRSRC resourceInfoHandle = FindResource(hInstance, MAKEINTRESOURCE(resourceSymbolId), TEXT("PNG"));
    if (!resourceInfoHandle) {
        WinError winErr;
        debugMsg("FindResource failed: %ls\n", winErr.getMessage());
        return;
    }
    HGLOBAL resourceHandle = LoadResource(hInstance, resourceInfoHandle);
    if (!resourceHandle) {
        WinError winErr;
        debugMsg("LoadResource failed: %ls\n", winErr.getMessage());
        return;
    }
    LPVOID pngBtnData = LockResource(resourceHandle);
    if (!pngBtnData) {
        WinError winErr;
        debugMsg("LockResource failed: %ls\n", winErr.getMessage());
        return;
    }
    DWORD size = SizeofResource(hInstance, resourceInfoHandle);
    if (!size) {
        WinError winErr;
        debugMsg("SizeofResource failed: %ls\n", winErr.getMessage());
        return;
    }
    png_image image;
    memset(&image, 0, (sizeof image));
    image.version = PNG_IMAGE_VERSION;
    if (!png_image_begin_read_from_memory(&image, pngBtnData, size)) {
        debugMsg("png_image_begin_read_from_memory failed.\n");
        return;
    }
    image.format = PNG_FORMAT_BGRA;
    pngResource.name = name;
    pngResource.width = image.width;
    pngResource.height = image.height;
    png_int_32 stride = PNG_IMAGE_ROW_STRIDE(image);
    size_t debugSize = PNG_IMAGE_BUFFER_SIZE(image, stride);
    pngResource.data.resize(image.width * image.height);
    if (!png_image_finish_read(&image, nullptr, &pngResource.data.front(), stride, nullptr)) {
        debugMsg("png_image_finish_read failed.\n");
        return;
    }

    if (makeBitmap) {
        pngResource.createBitmap(hdc);
    }

}

void PngResource::createBitmap(HDC hdc) {
    if (hBitmap) {
        DeleteObject(hBitmap);
        hBitmap = NULL;
    }
    if (width > INT_MAX || height > INT_MAX) return;
    hBitmap = CreateCompatibleBitmap(hdc, (int)width, (int)height);
    if (!hBitmap) {
        WinError winErr;
        debugMsg("CreateCompatibleBitmap failed\n");
        return;
    }
    BITMAPINFO bInfo;
    memset(&bInfo, 0, sizeof(BITMAPINFO));
    bInfo.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bInfo.bmiHeader.biPlanes = 1;
    bInfo.bmiHeader.biWidth = (int)width;
    bInfo.bmiHeader.biHeight = -(int)height;
    bInfo.bmiHeader.biBitCount = 24;
    bInfo.bmiHeader.biSizeImage = 0;
    bInfo.bmiHeader.biCompression = BI_RGB;

    size_t stride = (width * 3 + 3) & (~0b11);
    PixelRGB* bitmapData = (PixelRGB*)malloc(stride * height);
    if (!bitmapData) {
        debugMsg("Failed to allocate memory in loadPngResource\n");
        return;
    }
    PixelRGB* destRowPtr = bitmapData;
    auto srcPtr = data.begin();
    for (unsigned int heightCounter = 0; heightCounter < height; ++heightCounter) {
        PixelRGB* destPixelPtr = destRowPtr;
        for (unsigned int widthCounter = 0; widthCounter < width; ++widthCounter) {
            destPixelPtr->red = (0xff * (0xff - srcPtr->alpha) + srcPtr->red * srcPtr->alpha) / 0xff;
            destPixelPtr->green = (0xff * (0xff - srcPtr->alpha) + srcPtr->green * srcPtr->alpha) / 0xff;
            destPixelPtr->blue = (0xff * (0xff - srcPtr->alpha) + srcPtr->blue * srcPtr->alpha) / 0xff;
            ++srcPtr;
            ++destPixelPtr;
        }
        destRowPtr = (PixelRGB*)((char*)destRowPtr + stride);
    }

    SetDIBits(hdc, hBitmap, 0, (UINT)height, bitmapData, &bInfo, DIB_RGB_COLORS);
    free(bitmapData);
}

void drawPngResource(HDC hdc, const PngResource& img, int x, int y) {
    if (img.width > INT_MAX || img.height > INT_MAX) return;
    HGDIOBJ oldObject = SelectObject(hdcMem, img.hBitmap);
    BitBlt(hdc, x, y, (int)img.width, (int)img.height, hdcMem, 0, 0, SRCCOPY);
    SelectObject(hdcMem, oldObject);
}

void PngResource::bitBlt(PngResource& destination, unsigned int destinationX, unsigned int destinationY, unsigned int sourceX, unsigned int sourceY, unsigned int sourceWidth, unsigned int sourceHeight) const {
    PixelA* destinationRowPtr = &destination.data.front() + destinationY * destination.width;
    const PixelA* sourceRowPtr = &data.front() + sourceY * width;
    for (unsigned int heightCounter = sourceHeight; heightCounter != 0; --heightCounter) {
        memcpy(destinationRowPtr + destinationX, sourceRowPtr + sourceX, sourceWidth * sizeof(PixelA));
        sourceRowPtr += width;
        destinationRowPtr += destination.width;
    }
}

void PngResource::bitBlt(void* destination, int destinationStride, unsigned int destinationX, unsigned int destinationY, unsigned int sourceX, unsigned int sourceY, unsigned int sourceWidth, unsigned int sourceHeight) const {
    PixelA* destinationRowPtr = (PixelA*)((char*)destination + destinationY * destinationStride);
    const PixelA* sourceRowPtr = &data.front() + sourceY * width;
    for (unsigned int heightCounter = sourceHeight; heightCounter != 0; --heightCounter) {
        memcpy(destinationRowPtr + destinationX, sourceRowPtr + sourceX, sourceWidth * sizeof(PixelA));
        sourceRowPtr += width;
        destinationRowPtr = (PixelA*)((char*)destinationRowPtr + destinationStride);
    }
}
