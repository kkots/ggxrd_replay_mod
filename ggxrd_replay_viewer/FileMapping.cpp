#include "framework.h"
#include "FileMapping.h"
#include "debugMsg.h"
#include "WinError.h"
#include "MemoryProvisioner.h"

extern HANDLE fileMapMutex;
extern HANDLE eventChangesMade;
extern HANDLE eventChangesReceived;
extern HWND mainWindowHandle;
extern HANDLE eventApplicationIsClosing;
extern HANDLE eventGgProcessExited;
extern HDC hdcMem;
extern std::mutex hdcMemMutex;
extern int frameskip;

FileMappingGuard::FileMappingGuard() {
	lock();
}
FileMappingGuard::~FileMappingGuard() {
	unlock();
}
FileMappingGuard::FileMappingGuard(FileMappingGuard&& src) noexcept {
	moveFrom(src);
}
FileMappingGuard& FileMappingGuard::operator=(FileMappingGuard&& src) noexcept {
	unlock();
	moveFrom(src);
	return *this;
}
void FileMappingGuard::lock() {
	DWORD waitResult = WaitForSingleObject(fileMapMutex, INFINITE);
	if (waitResult == WAIT_OBJECT_0 || waitResult == WAIT_ABANDONED) {
		locked = true;
	}
}
void FileMappingGuard::markWroteSomething() {
	wroteSomething = true;
}
void FileMappingGuard::unlock() {
	if (locked) {
		if (wroteSomething) {
			SetEvent(eventChangesMade);
		}
		ReleaseMutex(fileMapMutex);
	}
	wroteSomething = false;
}
void FileMappingGuard::moveFrom(FileMappingGuard& src) noexcept {
	locked = src.locked;
	src.locked = false;
}

void FileMappingManager::sendStartRecordingCommand() {
	FileMappingGuard mapGuard;
	mapGuard.markWroteSomething();
	for (unsigned int i = 0; i < FILE_MAPPING_SLOTS; ++i) {
		
		view->imageInfos[i].offset = 0;
		view->imageInfos[i].consumed = true;

		view->logicInfos[i].boxesOffset = 0;
		view->logicInfos[i].consumed = true;

		view->needStartRecording = true;
		view->recording = true;  // recording field, when set to false, is treated as a command to stop recording
	}
}

void FileMappingManager::startThread() {
	if (threadStarted) return;
	threadStarted = true;
	thread = std::thread([](FileMappingManager* thisArg) {
		thisArg->threadLoop();
	}, this);
}

void FileMappingManager::joinAllThreads() {
	if (!threadStarted) return;
	thread.join();
}

void FileMappingManager::threadLoop() {
	debugMsg("FileMappingManager::threadLoop started\n");
	HANDLE objects[3] = { eventApplicationIsClosing, eventChangesReceived, eventGgProcessExited };
	while (true) {
		//debugMsg("threadLoop: Calling WaitForMultipleObjects\n");
		DWORD waitResult = WaitForMultipleObjects(3, objects, FALSE, INFINITE);
		//debugMsg("threadLoop: Called WaitForMultipleObjects\n");
		if (waitResult == WAIT_OBJECT_0) {
			return;
		} else if (waitResult == WAIT_OBJECT_0 + 2) {
			{
				FileMappingGuard mapGuard;
				for (unsigned int i = 0; i < FILE_MAPPING_SLOTS; ++i) {
					LogicInfo& logicInfo = (LogicInfo&)view->logicInfos[i];
					ImageInfo& imageInfo = (ImageInfo&)view->imageInfos[i];
					logicInfo.consumed = true;
					imageInfo.consumed = true;
				}
			}
			lastFrameMatchCounterIsEmpty = true;
			std::unique_lock<std::mutex> guard(encoderInput.mutex);
			encoderInput.processExited = true;
			SetEvent(encoderInput.event);
		} else if (waitResult != WAIT_OBJECT_0 + 1) { 
			debugMsg("FileMappingManager::threadLoop abnormal wait result (in decimal): %u\n", waitResult);
			return;
		}
		debugMsg("FileMappingManager::threadLoop received event\n");
		while (true) {
			unsigned int selectedSlot = FILE_MAPPING_SLOTS;
			unsigned int selectedLogicSlot = FILE_MAPPING_SLOTS;
			{
				FileMappingGuard mapGuard;
				unsigned int maxMatchCounter = 0;
				bool maxNotSetYet = true;
				bool maxIsEof = false;
				for (unsigned int i = 0; i < FILE_MAPPING_SLOTS; ++i) {
					LogicInfo& logicInfo = (LogicInfo&)view->logicInfos[i];
					/*if (!logicInfo.occupied) {
						if (logicInfo.isEof) {
							selectedLogicSlot = i;
						} else {
							logicInfo.consumed = true;
							mapGuard.markWroteSomething();
						}
					}
					continue;*/
					if (!logicInfo.occupied && !logicInfo.consumed
							&& (
									maxNotSetYet
									|| logicInfo.matchCounter > maxMatchCounter
									|| logicInfo.matchCounter == maxMatchCounter
									&& !(!maxIsEof && logicInfo.isEof)
								)
							) {
						maxNotSetYet = false;
						maxIsEof = logicInfo.isEof;
						selectedLogicSlot = i;
						maxMatchCounter = logicInfo.matchCounter;
					}
				}
				if (selectedLogicSlot != FILE_MAPPING_SLOTS) {
					view->logicInfos[selectedLogicSlot].occupied = true;
				}
				maxMatchCounter = 0;
				for (unsigned int i = 0; i < FILE_MAPPING_SLOTS; ++i) {
					ImageInfo& imageInfo = (ImageInfo&)view->imageInfos[i];
					/*if (!imageInfo.occupied) {
						if (imageInfo.isEof) {
							selectedSlot = i;
						} else {
							imageInfo.consumed = true;
							mapGuard.markWroteSomething();
						}
					}
					continue;*/
					if (!imageInfo.occupied && !imageInfo.consumed && imageInfo.matchCounter >= maxMatchCounter) {
						selectedSlot = i;
						maxMatchCounter = imageInfo.matchCounter;
					}
				}
				if (selectedSlot != FILE_MAPPING_SLOTS) {
					view->imageInfos[selectedSlot].occupied = true;
				}
			}
			if (selectedLogicSlot == FILE_MAPPING_SLOTS && selectedSlot == FILE_MAPPING_SLOTS) break;
			debugMsg("FileMappingManager::threadLoop picked logic slot %u, screenshot slot %u\n", selectedLogicSlot, selectedSlot);
			LogicInfo* logicInfo = nullptr;
			ImageInfo* imageInfo = nullptr;
			if (selectedLogicSlot != FILE_MAPPING_SLOTS) {
				logicInfo = (LogicInfo*)&view->logicInfos[selectedLogicSlot];
			}
			if (selectedSlot != FILE_MAPPING_SLOTS) {
				imageInfo = (ImageInfo*)&view->imageInfos[selectedSlot];
			}

			if (logicInfo) {
				if (logicInfo->isEof) { debugMsg("Received EOF logic\n"); }
				LogicInfoRef logicRef = memoryProvisioner.createLogicInfo();
				memcpy(logicRef.ptr, logicInfo, sizeof(LogicInfo));
				if (logicInfo->boxesSize <= 128 * 1024) {
					memcpy(logicRef.ptr->data, (char*)view + logicInfo->boxesOffset, logicInfo->boxesSize);
				}
				std::unique_lock<std::mutex> guard(encoderInput.mutex);
				debugMsg("FileMappingManager::threadLoop sending logic to encoderInput\n");
				encoderInput.addLogic(logicRef);
			}
			if (imageInfo) {
				if (imageInfo->isEof) { debugMsg("Received EOF image\n"); }
				EncoderInputFrameRef imageRef = memoryProvisioner.createEncoderInputFrame(imageInfo->pitch, imageInfo->width, imageInfo->height);
				if (imageInfo->empty) { debugMsg("Got empty frame in file mapping\n"); }
				imageRef->empty = imageInfo->empty;
				imageRef->isEof = imageInfo->isEof;
				imageRef->matchCounter = imageInfo->matchCounter;
				bool skipThisFrame = false;
				if (frameskip != 0
						&& !imageInfo->isEof
						&& !lastFrameMatchCounterIsEmpty
						&& imageInfo->matchCounter < lastFrameMatchCounter
						&& lastFrameMatchCounter - imageInfo->matchCounter < (unsigned int)(frameskip + 1)) {
					skipThisFrame = true;
				}
				if (!skipThisFrame) {
					if (imageInfo->isEof) {
						lastFrameMatchCounterIsEmpty = true;
					} else {
						lastFrameMatchCounter = imageInfo->matchCounter;
						lastFrameMatchCounterIsEmpty = false;
					}
					if (!imageInfo->empty && imageInfo->pitch * imageInfo->height <= imageRef.ptr->pitch * imageRef.ptr->height) {
						memcpy(imageRef.ptr->data, (char*)view + imageInfo->offset, imageInfo->pitch * imageInfo->height);
					}
					std::unique_lock<std::mutex> guard(encoderInput.mutex);
					debugMsg("FileMappingManager::threadLoop sending frame to encoderInput\n");
					encoderInput.addFrame(imageRef);
				}
			}
			SetEvent(encoderInput.event);

			{
				FileMappingGuard mapGuard;
				mapGuard.markWroteSomething();
				if (imageInfo) {
					imageInfo->consumed = true;
					imageInfo->occupied = false;
				}
				if (logicInfo) {
					logicInfo->consumed = true;
					logicInfo->occupied = false;
				}
			}
		}

		//std::unique_lock<std::mutex> guard(hdcMemMutex);
		/*HDC hdc = GetDC(mainWindowHandle);
		std::pair<HBITMAP, void*> getBitmapResult = getBitmap(imageInfo.width, imageInfo.height);
		HBITMAP hBit = getBitmapResult.first;
		void* destVoid = getBitmapResult.second;
		if (!hBitmap || !destVoid) {
			FileMappingGuard mapGuard;
			imageInfo.consumed = true;
			imageInfo.occupied = false;
			continue;
		}
		BITMAPINFO bInfo;
		memset(&bInfo, 0, sizeof(BITMAPINFO));
		bInfo.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
		bInfo.bmiHeader.biPlanes = 1;
		bInfo.bmiHeader.biWidth = imageInfo.width;
		bInfo.bmiHeader.biHeight = -(int)imageInfo.height;
		bInfo.bmiHeader.biBitCount = 24;
		bInfo.bmiHeader.biSizeImage = 0;
		bInfo.bmiHeader.biCompression = BI_RGB;
		struct Pixel {
			unsigned char red;
			unsigned char green;
			unsigned char blue;
		};
		struct PixelA {
			unsigned char red;
			unsigned char green;
			unsigned char blue;
			unsigned char alpha;
		};
		// L = Kr * R + Kb * B + (1 - Kr - Kb) * G
		// Y =               floor((219 * (L - Z) / S + 16) + 0.5)
		// U = clip3(0, 255, floor((112 * (B - L) / ((1 - Kb) * S) + 128) + 0.5))
		// V = clip3(0, 255, floor((112 * (R - L) / ((1 - Kr) * S) + 128) + 0.5))
		// clip3(min, max, value)
		// Z is the black-level variable. For computer RGB, Z equals 0. For studio video RGB, Z equals 16*2^(N-8), where N is the number of bits per RGB sample (N >= 8).
		// S is the scaling variable. For computer RGB, S equals 255. For studio video RGB, S equals 219*2^(N-8).

		// Computer RGB to YUV
		// Y = ( (  66 * R + 129 * G +  25 * B + 128) >> 8) +  16;
		// U = ( ( -38 * R -  74 * G + 112 * B + 128) >> 8) + 128;
		// V = ( ( 112 * R -  94 * G -  18 * B + 128) >> 8) + 128;

		// YUV to computer RGB
		// C = Y - 16
		// D = U - 128
		// E = V - 128
		// R = clip(( 298 * C           + 409 * E + 128) >> 8)
		// G = clip(( 298 * C - 100 * D - 208 * E + 128) >> 8)
		// B = clip(( 298 * C + 516 * D           + 128) >> 8)
		// clip() denotes clipping to a range of [0..255]

		Pixel* dest = (Pixel*)destVoid;
		PixelA* source = (PixelA*)((char*)view + imageInfo.offset);
		unsigned int stride = 3 * imageInfo.width;
		stride = (stride + 3) & (~0b11);
		Pixel* destRowPtr = dest;
		for (unsigned int heightCounter = 0; heightCounter < imageInfo.height; ++heightCounter) {
			Pixel* destPixelPtr = destRowPtr;
			for (unsigned int widthCounter = 0; widthCounter < imageInfo.width; ++widthCounter) {
				destPixelPtr->red = source->red;
				destPixelPtr->green = source->green;
				destPixelPtr->blue = source->blue;
				++destPixelPtr;
				++source;
			}
			destRowPtr = (Pixel*)((char*)destRowPtr + stride);
		}
		SetDIBits(hdc, hBitmap, 0, imageInfo.height, destVoid, &bInfo, DIB_RGB_COLORS);

		paintBitmap(hdc);
		ReleaseDC(mainWindowHandle, hdc);*/
	}
}

/*std::pair<HBITMAP, void*> FileMappingManager::getBitmap(unsigned int width, unsigned int height) {
	if (!(hBitmap && width == bitmapWidth && height == bitmapHeight && bitmapMemory)) {
		if (hBitmap) {
			DeleteObject(hBitmap);
		}
		if (bitmapMemory) {
			free(bitmapMemory);
		}
		debugMsg("Creating new bitmap\n");
		HDC hdc = GetDC(mainWindowHandle);
		hBitmap = CreateCompatibleBitmap(hdc, width, height);
		ReleaseDC(mainWindowHandle, hdc);
		if (!hBitmap || hBitmap == INVALID_HANDLE_VALUE) {
			hBitmap = NULL;
			WinError winErr;
			debugMsg("CreateCompatibleBitmap failed: " PRINTF_STRING_ARGA "\n", winErr.getMessageA().c_str());
			return { NULL, nullptr };
		}
		bitmapWidth = width;
		bitmapHeight = height;
		unsigned int stride = 3 * width;
		stride = (stride + 3) & (~0b11);
		bitmapMemory = malloc(stride * height);
	}
	return { hBitmap, bitmapMemory };
}

void FileMappingManager::onWmPaint(HDC hdc) {
	paintBitmap(hdc);
}

void FileMappingManager::paintBitmap(HDC hdc) {
	HGDIOBJ oldObject = SelectObject(hdcMem, hBitmap);
	BitBlt(hdc, 5, 100, bitmapWidth, bitmapHeight, hdcMem, 0, 0, SRCCOPY);
	SelectObject(hdcMem, oldObject);
}*/

void FileMappingManager::pushSettingsUpdate(bool displayBoxes, bool displayInputs) {
	FileMappingGuard guard;
	guard.markWroteSomething();
	view->displayBoxes = displayBoxes;
	view->displayInputs = displayInputs;
}
