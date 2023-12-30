#pragma once
#include "EncodedImageSize.h"
class ImageYUVScaler
{
public:
	void resize(unsigned int pitch, unsigned int width, unsigned int height);
	void process(void* destinationVoidPtr, void* sourceVoidPtr);
private:
	unsigned int currentWidth = 0;
	unsigned int currentHeight = 0;
	unsigned int currentPitch = 0;
	unsigned int cells[encodedImageHeight * encodedImageWidth]{0};
};
