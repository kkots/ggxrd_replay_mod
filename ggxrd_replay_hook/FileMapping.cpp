#include "pch.h"
#include "FileMapping.h"
#include "logging.h"
#include "WinError.h"
#include "Detouring.h"
#include "EndScene.h"
#include "Graphics.h"
#include "Hitbox.h"
#include "Camera.h"
#include "GifMode.h"

FileMappingManager fileMappingManager;

const unsigned int MAX_FILEMAP_SIZE = 256 * 1024 + 2 * (1280 * 720 * 4 + 256);

char* outOfBoundsPtr = nullptr;
const unsigned int alignedSize = (1280 * 720 * 4 + 256 + 31) & (~0b11111);
const unsigned int logicSize = 128 * 1024 - 31 - sizeof(FileMapping);

bool FileMappingManager::onDllMain() {
	mutex = OpenMutexW(MUTEX_ALL_ACCESS, FALSE, L"ggxrd_replay_mutex");
	if (!mutex) {
		WinError winErr;
		logwrap(fprintf(logfile, "OpenMutexW failed: %s\n", winErr.getMessage()));
		return false;
	}
	mapping = OpenFileMappingW(FILE_MAP_ALL_ACCESS, FALSE, L"ggxrd_replay_file_mapping");
	if (!mapping) {
		WinError winErr;
		logwrap(fprintf(logfile, "OpenFileMappingW failed: %s\n", winErr.getMessage()));
		return false;
	}
	eventChangesMade = OpenEventW(EVENT_ALL_ACCESS, FALSE, L"ggxrd_replay_dll_wrote");
	if (!eventChangesMade) {
		WinError winErr;
		logwrap(fprintf(logfile, "OpenEventW failed: %s\n", winErr.getMessage()));
		return false;
	}
	eventChangesReceived = OpenEventW(EVENT_ALL_ACCESS, FALSE, L"ggxrd_replay_app_wrote");
	if (!eventChangesReceived) {
		WinError winErr;
		logwrap(fprintf(logfile, "OpenEventW (2) failed: %s\n", winErr.getMessage()));
		return false;
	}
	view = (FileMapping*)MapViewOfFile(mapping, FILE_MAP_ALL_ACCESS, 0, 0, MAX_FILEMAP_SIZE);
	if (!view) {
		WinError winErr;
		logwrap(fprintf(logfile, "MapViewOfFile failed: %s\n", winErr.getMessage()));
		return false;
	}
	eventExiting = CreateEventW(NULL, TRUE, FALSE, NULL);
	if (!eventExiting) {
		WinError winErr;
		logwrap(fprintf(logfile, "CreateEventW failed: %s\n", winErr.getMessage()));
		return false;
	}
	graphics.eventScreenshotSurfaceSet = CreateEventW(NULL, FALSE, FALSE, NULL);
	if (!graphics.eventScreenshotSurfaceSet) {
		WinError winErr;
		logwrap(fprintf(logfile, "CreateEventW (2) failed: %s\n", winErr.getMessage()));
		return false;
	}
	graphics.eventScreenshotSurfacesDeoccupied = CreateEventW(NULL, FALSE, FALSE, NULL);
	if (!graphics.eventScreenshotSurfacesDeoccupied) {
		WinError winErr;
		logwrap(fprintf(logfile, "CreateEventW (3) failed: %s\n", winErr.getMessage()));
		return false;
	}
	graphics.eventLogicBufferSet = CreateEventW(NULL, FALSE, FALSE, NULL);
	if (!graphics.eventLogicBufferSet) {
		WinError winErr;
		logwrap(fprintf(logfile, "CreateEventW (4) failed: %s\n", winErr.getMessage()));
		return false;
	}
	eventLogicBufferFreed = CreateEventW(NULL, FALSE, FALSE, NULL);
	if (!eventLogicBufferFreed) {
		WinError winErr;
		logwrap(fprintf(logfile, "CreateEventW (4) failed: %s\n", winErr.getMessage()));
		return false;
	}
	graphics.eventLogicBufferFreed = CreateEventW(NULL, FALSE, FALSE, NULL);
	if (!graphics.eventLogicBufferFreed) {
		WinError winErr;
		logwrap(fprintf(logfile, "CreateEventW (5) failed: %s\n", winErr.getMessage()));
		return false;
	}
	thread = std::thread([]() {
		fileMappingManager.threadLoop();
	});
	framesThread = std::thread([]() {
		fileMappingManager.framesThreadLoop();
	});
	logicThread = std::thread([]() {
		fileMappingManager.logicThreadLoop();
	});
	return true;
}

void FileMappingManager::onDllDetach() {
	if (view) {
		FlushViewOfFile((LPCVOID)view, sizeof(FileMapping));
		UnmapViewOfFile((LPCVOID)view);
	}
	if (mapping) CloseHandle(mapping);
	if (eventChangesMade) CloseHandle(eventChangesMade);
	if (eventChangesReceived) CloseHandle(eventChangesReceived);
	if (mutex) CloseHandle(mutex);
}

void FileMappingManager::threadLoop() {
	while (true) {
		HANDLE objects[2] = { eventExiting, eventChangesReceived };
		DWORD waitResult = WaitForMultipleObjects(2, objects, FALSE, INFINITE);
		if (waitResult == WAIT_OBJECT_0) {
			logwrap(fputs("FileMappingManager::threadLoop thread exiting.\n", logfile));
			return;
		} else if (waitResult == WAIT_OBJECT_0 + 1) {
			FileMappingGuard guard;
			readChanges(guard);
		} else {
			logwrap(fprintf(logfile, "FileMappingManager::threadLoop: Abnormal result of WaitForMultipleObjects: %.8x\n", waitResult));
		}
	}
}

void FileMappingManager::readChanges(FileMappingGuard& guard) {
	if (view->unloadDll) {
		logwrap(fputs("FileMappingManager found DLL unload request.\n", logfile));
		endScene.unloadDll = true;
		SetEvent(fileMappingManager.eventExiting);
		return;
	}
	if (view->needStartRecording) {
		logwrap(fputs("FileMappingManager found recording start request.\n", logfile));
		const unsigned int base = (sizeof(FileMapping) + 31) & (~0b11111);  // images must be 32-byte aligned in THIS process to get them out as fast as possible to not slow down the GAME
		for (unsigned int i = 0; i < FILE_MAPPING_SLOTS; ++i) {
			if (!view->imageInfos[i].offset) {
				view->imageInfos[i].offset = base + i * alignedSize;
			}
		}
		const unsigned int logicBase = base + FILE_MAPPING_SLOTS * alignedSize;
		for (unsigned int i = 0; i < FILE_MAPPING_SLOTS; ++i) {
			if (!view->logicInfos[i].boxesOffset) {
				view->logicInfos[i].boxesOffset = logicBase + i * logicSize;
			}
		}
		endScene.needStartRecording = true;
		view->needStartRecording = false;
		guard.markWroteSomething();
	} else if (!view->recording) {
		if (endScene.recording) {
			logwrap(fputs("FileMappingManager found recording stop request.\n", logfile));
			endScene.recording = false;
		}
	}
	if (needLogicBufferFreed) {
		for (unsigned int i = 0; i < FILE_MAPPING_SLOTS; ++i) {
			if (!view->logicInfos[i].occupied && view->logicInfos[i].consumed) {
				SetEvent(eventLogicBufferFreed);
				needLogicBufferFreed = false;
				break;
			}
		}
	}
	if (view->displayBoxes != (gifMode.hitboxDisplayDisabled == false)) {
		logwrap(fprintf(logfile, "Setting hitboxDisplayDisabled to %u\n", (unsigned int)(!view->displayBoxes)));
		gifMode.hitboxDisplayDisabled = !view->displayBoxes;
	}
	if (view->displayInputs != (gifMode.inputDisplayDisabled == false)) {
		logwrap(fprintf(logfile, "Setting inputDisplayDisabled to %u\n", (unsigned int)(!view->displayBoxes)));
		gifMode.inputDisplayDisabled = !view->displayInputs;
	}
}

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
	DWORD waitResult = WaitForSingleObject(fileMappingManager.mutex, INFINITE);
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
			SetEvent(fileMappingManager.eventChangesMade);
		}
		ReleaseMutex(fileMappingManager.mutex);
	}
	wroteSomething = false;
}
void FileMappingGuard::moveFrom(FileMappingGuard& src) noexcept {
	locked = src.locked;
	src.locked = false;
}

void FileMappingManager::joinAllThreads() {
	std::unique_lock<std::mutex> guard(allJoinMutex);
	if (threadsJoined) return;
	threadsJoined = true;
	SetEvent(fileMappingManager.eventExiting);
	thread.join();
	framesThread.join();
	logicThread.join();
	logwrap(fputs("All threads joined.\n", logfile));
}

void FileMappingManager::framesThreadLoop() {
	while (true) {
		HANDLE objects[2] = { eventExiting, graphics.eventScreenshotSurfaceSet };
		DWORD waitResult = WaitForMultipleObjects(2, objects, FALSE, INFINITE);
		if (waitResult == WAIT_OBJECT_0) {
			logwrap(fputs("framesThreadLoop: thread exiting\n", logfile));
			return;
		} else if (waitResult == WAIT_OBJECT_0 + 1) {
			std::unique_lock<std::mutex> guard(graphics.screenshotSurfacesMutex);
			unsigned int maxMatchCounter = 0;
			unsigned int maxSurface = SCREENSHOT_SURFACE_COUNT;
			bool maxIsEof = false;
			bool maxIsFirst = true;
			for (unsigned int i = 0; i < SCREENSHOT_SURFACE_COUNT; ++i) {
				Graphics::ScreenshotSurface& screenshotSurface = graphics.screenshotSurfaces[i];
				if (screenshotSurface.offscreenSurface && !screenshotSurface.consumed && !screenshotSurface.occupied) {
					if (maxIsFirst
							|| screenshotSurface.matchCounter > maxMatchCounter
							|| screenshotSurface.matchCounter == maxMatchCounter
							&& !(!maxIsEof && screenshotSurface.isEof)) {
						maxIsFirst = false;
						maxIsEof = screenshotSurface.isEof;
						maxMatchCounter = screenshotSurface.matchCounter;
						maxSurface = i;
					}
				}
			}
			if (maxSurface == SCREENSHOT_SURFACE_COUNT) continue;
			Graphics::ScreenshotSurface& screenshotSurface = graphics.screenshotSurfaces[maxSurface];
			screenshotSurface.occupied = true;
			guard.unlock();
			const unsigned int matchCounter = screenshotSurface.matchCounter;
			unsigned int width = screenshotSurface.offscreenSurfaceWidth;
			unsigned int height = screenshotSurface.offscreenSurfaceHeight;
			unsigned int widthStep = 1;
			unsigned int heightStep = 1;
			const unsigned int maxSize = 1280 * 720;
			unsigned int coeff = 1;
			while (width * height / (coeff*coeff) > maxSize) {
				++coeff;
			}
			const unsigned int origWidth = width;
			const unsigned int origHeight = height;
			width /= coeff;
			height /= coeff;
			widthStep = coeff;
			heightStep = coeff;

			unsigned int selectedSlot = FILE_MAPPING_SLOTS;
			{
				FileMappingGuard mappingGuard;
				for (unsigned int i = 0; i < FILE_MAPPING_SLOTS; ++i) {
					volatile ImageInfo& imageInfo = view->imageInfos[i];
					if (!imageInfo.occupied) {
						imageInfo.occupied = true;
						selectedSlot = i;
						break;
					}
				}
			}
			if (selectedSlot == FILE_MAPPING_SLOTS) {
				logwrap(fputs("Error: selectedSlot == FILE_MAPPING_SLOTS\n", logfile));
			}
			volatile ImageInfo& imageInfo = view->imageInfos[selectedSlot];
			
			if (selectedSlot < FILE_MAPPING_SLOTS) {
				if (!screenshotSurface.empty) {
					const unsigned int imageInfoOffset = imageInfo.offset;
					if (imageInfoOffset + imageInfo.pitch * height <= MAX_FILEMAP_SIZE) {
						if (widthStep != 1 || heightStep != 1) {
							imageInfo.pitch = 4 * width;
							const unsigned int widthStepConst = widthStep;
							unsigned int* rowPtrSrc = (unsigned int*)screenshotSurface.lockedRectPtr;
							unsigned int* pixelPtrDest = (unsigned int*)((char*)view + imageInfoOffset);
							for (unsigned int heightCounter = 0; heightCounter < screenshotSurface.offscreenSurfaceHeight; heightCounter += heightStep) {
								volatile unsigned int* pixelPtrSrc = rowPtrSrc;
								for (unsigned int widthCounter = 0; widthCounter < screenshotSurface.offscreenSurfaceWidth; widthCounter += widthStepConst) {
									*pixelPtrDest = *pixelPtrSrc;
									pixelPtrSrc += widthStepConst;
									++pixelPtrDest;
								}
								rowPtrSrc = (unsigned int*)((char*)rowPtrSrc + screenshotSurface.pitch * heightStep);
							}
						} else {
							imageInfo.pitch = screenshotSurface.pitch;
							memcpy((char*)view + imageInfoOffset, (const void*)screenshotSurface.lockedRectPtr, screenshotSurface.offscreenSurfaceHeight * screenshotSurface.pitch);
						}

					} else {
						logwrap(fputs("Error: imageInfoOffset + width * height * 4 <= MAX_FILEMAP_SIZE not passed\n", logfile));
					}
				}
				if (screenshotSurface.empty) { logwrap(fputs("framesThreadLoop sends empty frame\n", logfile)); }
				imageInfo.empty = screenshotSurface.empty;
				imageInfo.isEof = screenshotSurface.isEof;
			}
			screenshotSurface.empty = true;

			guard.lock();
			screenshotSurface.occupied = false;
			screenshotSurface.consumed = true;
			if (graphics.screenshotSurfacesNeedDeoccupy) {
				graphics.screenshotSurfacesNeedDeoccupy = false;
				SetEvent(graphics.eventScreenshotSurfacesDeoccupied);
			}
			guard.unlock();

			if (selectedSlot < FILE_MAPPING_SLOTS) {
				FileMappingGuard mappingGuard;
				mappingGuard.markWroteSomething();
				imageInfo.matchCounter = matchCounter;
				imageInfo.consumed = false;
				imageInfo.occupied = false;
				imageInfo.height = height;
				imageInfo.width = width;
				imageInfo.origWidth = origWidth;
				imageInfo.origHeight = origHeight;
			}
		}
	}
}

void FileMappingManager::logicThreadLoop() {
	while (true) {
		HANDLE objects[2] = { eventExiting, graphics.eventLogicBufferSet };
		DWORD waitResult = WaitForMultipleObjects(2, objects, FALSE, INFINITE);
		if (waitResult == WAIT_OBJECT_0) {
			logwrap(fputs("logicThreadLoop: thread exiting\n", logfile));
			return;
		}
		else if (waitResult == WAIT_OBJECT_0 + 1) {
			std::unique_lock<std::mutex> guard(graphics.logicBufferMutex);
			unsigned int maxMatchCounter = 0;
			unsigned int maxLogic = SCREENSHOT_SURFACE_COUNT;
			bool maxIsNotSetYet = true;
			bool maxIsEof = false;
			for (unsigned int i = 0; i < SCREENSHOT_SURFACE_COUNT; ++i) {
				volatile LogicSlot& logicSlot = graphics.logicBuffer[i];
				if (logicSlot.drawData.isEof) { logwrap(fputs("logicThreadLoop saw eof in queue\n", logfile)); }
				if (!logicSlot.consumed && !logicSlot.occupied) {
					if (maxIsNotSetYet
							|| logicSlot.drawData.matchCounter > maxMatchCounter
							|| logicSlot.drawData.matchCounter == maxMatchCounter
							&& !(!maxIsEof && logicSlot.drawData.isEof)) {
						maxMatchCounter = logicSlot.drawData.matchCounter;
						maxIsNotSetYet = false;
						maxIsEof = logicSlot.drawData.isEof;
						maxLogic = i;
					}
				}
			}
			if (maxLogic == SCREENSHOT_SURFACE_COUNT) continue;
			LogicSlot& logicSlot = (LogicSlot&)graphics.logicBuffer[maxLogic];
			logicSlot.occupied = true;
			guard.unlock();
			const unsigned int matchCounter = logicSlot.drawData.matchCounter;

			unsigned int selectedSlot = FILE_MAPPING_SLOTS;
			{
				FileMappingGuard mappingGuard;
				while (true) {
					for (unsigned int i = 0; i < FILE_MAPPING_SLOTS; ++i) {
						volatile LogicInfo& logicInfo = view->logicInfos[i];
						if (!logicInfo.occupied && logicInfo.consumed) {
							logicInfo.occupied = true;
							selectedSlot = i;
							break;
						}
					}
					if (selectedSlot != FILE_MAPPING_SLOTS) break;
					ResetEvent(eventLogicBufferFreed);
					needLogicBufferFreed = true;  // this tells the file-changes-reading thread to notify when a logic slot frees up
					HANDLE objects2[2] = { eventExiting, eventLogicBufferFreed };
					mappingGuard.unlock();
					logwrap(fputs("logicThreadLoop waiting for eventLogicBufferFreed\n", logfile));
					DWORD waitResult2 = WaitForMultipleObjects(2, objects2, FALSE, INFINITE);
					mappingGuard.lock();
					if (waitResult2 == WAIT_OBJECT_0) {
						logwrap(fputs("logicThreadLoop: thread exiting\n", logfile));
						return;
					}
				}
			}

			volatile LogicInfo& logicInfo = view->logicInfos[selectedSlot];
			memcpy((void*)logicInfo.facings, (const void*)logicSlot.drawData.facings, sizeof(logicSlot.drawData.facings));
			memcpy((void*)logicInfo.inputRingBuffers, (const void*)logicSlot.drawData.inputRingBuffers, sizeof(logicSlot.drawData.inputRingBuffers));
			memcpy((void*)logicInfo.replayStates, (const void*)logicSlot.drawData.replayStates, sizeof(logicSlot.drawData.replayStates));
			logicInfo.matchCounter = logicSlot.drawData.matchCounter;
			logicInfo.isEof = logicSlot.drawData.isEof;
			if (logicInfo.isEof) { logwrap(fputs("logicThreadLoop saw eof\n", logfile)); }
			logicInfo.wontHaveFrame = logicSlot.drawData.wontHaveFrame;

			if (!logicSlot.drawData.isEof) {
				view->gameMode = logicSlot.drawData.gameMode;
				view->playerSide = logicSlot.drawData.playerSide;
				memcpy((void*)view->characterTypes, logicSlot.drawData.characterTypes, sizeof(logicSlot.drawData.characterTypes));
				view->replayDataPtr = (unsigned int)logicSlot.drawData.replayDataInputsPtr;
			}

			char* ptr = (char*)view + logicInfo.boxesOffset;
			const char* const ptrStart = ptr;
			if (maxLogic != SCREENSHOT_SURFACE_COUNT - 1) {
				outOfBoundsPtr = ptr + logicSize;
			} else {
				outOfBoundsPtr = (char*)view + MAX_FILEMAP_SIZE;
			}
			if (logicInfo.boxesOffset < MAX_FILEMAP_SIZE - 10) {  // at least some sanity check
				// will have to let this write out of bounds. It should be no problem because we're the ones providing the data
				serializeCamera(ptr, logicSlot.camera);
				serializeHurtboxes(ptr, logicSlot.drawData.hurtboxes);
				serializeHitboxes(ptr, logicSlot.drawData.hitboxes);
				serializePushboxes(ptr, logicSlot.drawData.pushboxes);
				serializePoints(ptr, logicSlot.drawData.points);
				serializeThrowInfos(ptr, logicSlot.drawData.throwInfos);
				logicInfo.boxesSize = ptr - ptrStart;
			}

			guard.lock();
			logicSlot.occupied = false;
			logicSlot.consumed = true;
			logicSlot.drawData.clear();
			//logwrap(fprintf(logfile, "logicThreadLoop sending logic slot %u with matchCounter %u\n", selectedSlot, logicInfo.matchCounter));
			if (graphics.needToFreeLogicBuffer) {
				logwrap(fputs("logicThreadLoop freed a logic slot and notified graphics thread\n", logfile));
				graphics.needToFreeLogicBuffer = false;
				SetEvent(graphics.eventLogicBufferFreed);
			}
			guard.unlock();

			FileMappingGuard mappingGuard;
			mappingGuard.markWroteSomething();
			logicInfo.consumed = false;
			logicInfo.occupied = false;
		} else {
			logwrap(fprintf(logfile, "logicThreadLoop: abnormal result from main WaitForMultipleObjects: %.8x\n", (unsigned int)waitResult));
		}
	}
}

void FileMappingManager::serializeCamera(char*& ptr, const CameraValues& camera) {
	writeFloat(ptr, camera.pos.x);
	writeFloat(ptr, camera.pos.y);
	writeFloat(ptr, camera.pos.z);
	writeInt(ptr, camera.pitch);
	writeInt(ptr, camera.yaw);
	writeInt(ptr, camera.roll);
	writeFloat(ptr, camera.fov);
	writeFloat(ptr, camera.coordCoefficient);
}

void FileMappingManager::serializeArraybox(char*& ptr, const DrawHitboxArrayCallParams& arraybox) {
	if (arraybox.hitboxCount == 0) return;
	writeChar(ptr, arraybox.hitboxCount);
	writeUnsignedInt(ptr, (unsigned int)arraybox.hitboxData);
	const Hitbox* hitbox = arraybox.hitboxData;
	for (int i = 0; i < arraybox.hitboxCount; ++i) {
		writeFloat(ptr, hitbox->offX);
		writeFloat(ptr, hitbox->offY);
		writeFloat(ptr, hitbox->sizeX);
		writeFloat(ptr, hitbox->sizeY);
		++hitbox;
	}
	writeInt(ptr, arraybox.params.posX);
	writeInt(ptr, arraybox.params.posY);
	writeChar(ptr, arraybox.params.flip);
	writeInt(ptr, arraybox.params.scaleX);
	writeInt(ptr, arraybox.params.scaleY);
	writeInt(ptr, arraybox.params.angle);
	writeInt(ptr, arraybox.params.hitboxOffsetX);
	writeInt(ptr, arraybox.params.hitboxOffsetY);
	writeChar(ptr, arraybox.type);
}

void FileMappingManager::serializeHurtboxes(char*& ptr, const std::vector<ComplicatedHurtbox>& hurtboxes) {
	auto it = hurtboxes.cbegin();
	while (it != hurtboxes.cend()) {
		writeChar(ptr, it->hasTwo + 1);
		if (it->hasTwo) {
			serializeArraybox(ptr, it->param1);  // hurtbox
			serializeArraybox(ptr, it->param2);  // graybox
		} else {
			serializeArraybox(ptr, it->param1);  // hurtbox
		} 
		++it;
	}
	writeChar(ptr, '\0');
}

void FileMappingManager::serializeHitboxes(char*& ptr, const std::vector<DrawHitboxArrayCallParams>& hitboxes) {
	auto it = hitboxes.cbegin();
	while (it != hitboxes.cend()) {
		serializeArraybox(ptr, *it);
		++it;
	}
	writeChar(ptr, '\0');
	
}

void FileMappingManager::serializePushboxes(char*& ptr, const std::vector<DrawBoxCallParams>& pushboxes) {
	writeChar(ptr, (char)pushboxes.size());
	auto it = pushboxes.cbegin();
	while (it != pushboxes.cend()) {
		writeInt(ptr, it->left);
		writeInt(ptr, it->right);
		writeInt(ptr, it->top);
		writeInt(ptr, it->bottom);
		writeChar(ptr, it->type);
		++it;
	}
}

void FileMappingManager::serializePoints(char*& ptr, const std::vector<DrawPointCallParams>& points) {
	writeChar(ptr, (char)points.size());
	auto it = points.cbegin();
	while (it != points.cend()) {
		writeInt(ptr, it->posX);
		writeInt(ptr, it->posY);
		++it;
	}
}

void FileMappingManager::serializeThrowInfos(char*& ptr, const std::vector<ThrowInfo>& throwInfos) {
	writeChar(ptr, (char)throwInfos.size());
	auto it = throwInfos.cbegin();
	while (it != throwInfos.cend()) {
		unsigned char totalSize = 0;
		if (it->leftUnlimited) {
			totalSize += 4;
		}
		if (it->rightUnlimited) {
			totalSize += 4;
		}
		if (it->topUnlimited) {
			totalSize += 4;
		}
		if (it->bottomUnlimited) {
			totalSize += 4;
		}
		if (it->hasPushboxCheck) {
			totalSize += 8;
		}
		if (it->hasXCheck) {
			totalSize += 8;
		}
		if (it->hasYCheck) {
			totalSize += 8;
		}
		writeChar(ptr, totalSize);
		writeChar(ptr, (unsigned char)it->leftUnlimited
			| (unsigned char)((unsigned char)it->rightUnlimited << 1)
			| (unsigned char)((unsigned char)it->topUnlimited << 2)
			| (unsigned char)((unsigned char)it->bottomUnlimited << 3)
			| (unsigned char)((unsigned char)it->hasPushboxCheck << 4)
			| (unsigned char)((unsigned char)it->hasXCheck << 5)
			| (unsigned char)((unsigned char)it->hasYCheck << 6));
		if (!it->leftUnlimited) {
			writeInt(ptr, it->left);
		}
		if (!it->rightUnlimited) {
			writeInt(ptr, it->right);
		}
		if (!it->topUnlimited) {
			writeInt(ptr, it->top);
		}
		if (!it->bottomUnlimited) {
			writeInt(ptr, it->bottom);
		}
		if (it->hasPushboxCheck) {
			writeInt(ptr, it->pushboxCheckMinX);
			writeInt(ptr, it->pushboxCheckMaxX);
		}
		if (it->hasXCheck) {
			writeInt(ptr, it->minX);
			writeInt(ptr, it->maxX);
		}
		if (it->hasYCheck) {
			writeInt(ptr, it->minY);
			writeInt(ptr, it->maxY);
		}
		++it;
	}
}

void FileMappingManager::writeChar(char*& ptr, char value) {
	#ifdef _DEBUG
	if (ptr + 4 > outOfBoundsPtr) logwrap(fputs("writeChar writing past the end of file mapping\n", logfile));
	#endif
	*ptr = value;
	ptr += 4;
}

void FileMappingManager::writeUnsignedInt(char*& ptr, unsigned int value) {
	#ifdef _DEBUG
	if (ptr + 4 > outOfBoundsPtr) logwrap(fputs("writeUnsignedInt writing past the end of file mapping\n", logfile));
	#endif
	*(unsigned int*)ptr = value;
	ptr += 4;
}

void FileMappingManager::writeInt(char*& ptr, int value) {
	#ifdef _DEBUG
	if (ptr + 4 > outOfBoundsPtr) logwrap(fputs("writeInt writing past the end of file mapping\n", logfile));
	#endif
	*(int*)ptr = value;
	ptr += 4;
}

void FileMappingManager::writeFloat(char*& ptr, float value) {
	#ifdef _DEBUG
	if (ptr + 4 > outOfBoundsPtr) logwrap(fputs("writeFloat writing past the end of file mapping\n", logfile));
	#endif
	*(float*)ptr = value;
	ptr += 4;
}
