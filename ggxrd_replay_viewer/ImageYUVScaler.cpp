#include "framework.h"
#include "ImageYUVScaler.h"
#include "PixelA.h"
#include <math.h>

void ImageYUVScaler::resize(unsigned int pitch, unsigned int width, unsigned int height) {
	if (currentPitch == pitch && currentWidth == width && currentHeight == height) {
		return;
	}
    currentPitch = pitch;
	currentWidth = width;
	currentHeight = height;
    for (unsigned int heightCounter = 0; heightCounter < encodedImageHeight; ++heightCounter) {
        unsigned int source = pitch * (heightCounter * height / encodedImageHeight);
        for (unsigned int widthCounter = 0; widthCounter < encodedImageWidth; ++widthCounter) {
            cells[heightCounter * encodedImageWidth + widthCounter] = source + (widthCounter * width / encodedImageWidth) * 4;
        }
    }
}

void ImageYUVScaler::process(void* destinationVoidPtr, void* sourceVoidPtr) {
    const PixelA* sourceRowPtr = (const PixelA*)sourceVoidPtr;
    unsigned char* yPtr = (unsigned char*)destinationVoidPtr;
    unsigned char* vPtr = (unsigned char*)destinationVoidPtr + (encodedImageWidth * encodedImageHeight);
    unsigned char* uPtr = vPtr + (encodedImageWidth * encodedImageHeight) / 4;
    unsigned int* cellPtr = cells;
    for (unsigned int heightCounter = 0; heightCounter < encodedImageHeight; heightCounter += 2) {
        for (unsigned int widthCounter = 0; widthCounter < encodedImageWidth; widthCounter += 2) {
            {
                const PixelA* sourcePixel = (const PixelA*)((const char*)sourceRowPtr + *cellPtr);
                const BYTE red = sourcePixel->red;
                const BYTE green = sourcePixel->green;
                const BYTE blue = sourcePixel->blue;
                // these formulas are taken from https://learn.microsoft.com/en-us/windows/win32/medfound/recommended-8-bit-yuv-formats-for-video-rendering
                *yPtr = ((66 * red + 129 * green + 25 * blue + 128) >> 8) + 16;
                ++yPtr;
                *uPtr = ((-38 * red - 74 * green + 112 * blue + 128) >> 8) + 128;
                ++uPtr;
                *vPtr = ((112 * red - 94 * green - 18 * blue + 128) >> 8) + 128;
                ++vPtr;
                ++cellPtr;
            }
            {
                const PixelA* sourcePixel = (const PixelA*)((const char*)sourceRowPtr + *cellPtr);
                *yPtr = ((66 * sourcePixel->red + 129 * sourcePixel->green + 25 * sourcePixel->blue + 128) >> 8) + 16;
                ++yPtr;
                ++cellPtr;
            }
        }
        for (unsigned int widthCounter = 0; widthCounter < encodedImageWidth; ++widthCounter) {
            const PixelA* sourcePixel = (const PixelA*)((const char*)sourceRowPtr + *cellPtr);
            *yPtr = ((66 * sourcePixel->red + 129 * sourcePixel->green + 25 * sourcePixel->blue + 128) >> 8) + 16;
            ++yPtr;
            ++cellPtr;
        }
    }
}
