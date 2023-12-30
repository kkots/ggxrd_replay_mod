#pragma once
#include "framework.h"
#include <vector>
#include "PixelA.h"
#include "PixelRGB.h"

struct PngResource {
	const char* name = nullptr;
	size_t width = 0;
	size_t height = 0;
	float uStart = 0.F;
	float uEnd = 0.F;
	float vStart = 0.F;
	float vEnd = 0.F;
	unsigned int startX = 0;
	unsigned int startY = 0;
	std::vector<PixelA> data;
	HBITMAP hBitmap = NULL;
	void createBitmap(HDC hdc);
	void bitBlt(PngResource& destination, unsigned int destinationX, unsigned int destinationY, unsigned int sourceX, unsigned int sourceY, unsigned int sourceWidth, unsigned int sourceHeight) const;
	void bitBlt(void* destination, int destinationStride, unsigned int destinationX, unsigned int destinationY, unsigned int sourceX, unsigned int sourceY, unsigned int sourceWidth, unsigned int sourceHeight) const;
};

void loadPngResource(HINSTANCE hInstance, WORD resourceSymbolId, PngResource& windowsImage, HDC hdc, const char* name, bool makeBitmap = true);

void drawPngResource(HDC hdc, const PngResource& img, int x, int y);
