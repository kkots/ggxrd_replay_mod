#include "pch.h"
#include "EndScene.h"
#include "Direct3DVTable.h"
#include "Detouring.h"
#include "DrawOutlineCallParams.h"
#include "AltModes.h"
#include "HitDetector.h"
#include "logging.h"
#include "Game.h"
#include "EntityList.h"
#include "InvisChipp.h"
#include "Graphics.h"
#include "Camera.h"
#include <algorithm>
#include "collectHitboxes.h"
#include "Throws.h"
#include "colors.h"
#include "GifMode.h"
#include "memoryFunctions.h"
#include "FileMapping.h"

EndScene endScene;

bool EndScene::onDllMain() {
	bool error = false;

	char** d3dvtbl = direct3DVTable.getDirect3DVTable();
	orig_EndScene = (EndScene_t)d3dvtbl[42];
	orig_Present = (Present_t)d3dvtbl[17];

	// there will actually be a deadlock during DLL unloading if we don't put Present first and EndScene second

	if (!detouring.attach(&(PVOID&)(orig_Present),
		hook_Present,
		&orig_PresentMutex,
		"Present")) return false;

	if (!detouring.attach(&(PVOID&)(orig_EndScene),
		hook_EndScene,
		&orig_EndSceneMutex,
		"EndScene")) return false;

	orig_SendUnrealPawnData = (SendUnrealPawnData_t)sigscanOffset(
		"GuiltyGearXrd.exe",
		"\x8b\x0d\x00\x00\x00\x00\x33\xdb\x53\xe8\x00\x00\x00\x00\xf3\x0f\x10\x80\x24\x04\x00\x00\xf3\x0f\x5c\x05\x00\x00\x00\x00\xf3\x0f\x10\x8e\xd0\x01\x00\x00\x0f\x2f\xc8\x76\x05\x8d\x43\x01\xeb\x02",
		"xx????xxxx????xxxxxxxxxxxx????xxxxxxxxxxxxxxxxxx",
		{ -0x11 },
		&error, "SendUnrealPawnData");

	if (orig_SendUnrealPawnData) {
		void (HookHelp::*sendUnrealPawnDataHookPtr)(void) = &HookHelp::sendUnrealPawnDataHook;
		if (!detouring.attach(&(PVOID&)orig_SendUnrealPawnData,
			(PVOID&)sendUnrealPawnDataHookPtr,
			&orig_SendUnrealPawnDataMutex,
			"SendUnrealPawnData")) return false;
	}

	orig_ReadUnrealPawnData = (ReadUnrealPawnData_t)sigscanOffset(
		"GuiltyGearXrd.exe",
		"\x8b\xf1\x8b\x0e\xe8\x00\x00\x00\x00\x8b\x06\x8b\x48\x04\x89\x44\x24\x04\x85\xc9\x74\x1e\x83\x78\x08\x00\x74\x18\xf6\x81\x84\x00\x00\x00\x20",
		"xxxxx????xxxxxxxxxxxxxxxxxxxxxxxxxx",
		{ -2 },
		&error, "ReadUnrealPawnData");

	if (orig_ReadUnrealPawnData) {
		void (HookHelp::*readUnrealPawnDataHookPtr)(void) = &HookHelp::readUnrealPawnDataHook;
		if (!detouring.attach(&(PVOID&)orig_ReadUnrealPawnData,
			(PVOID&)readUnrealPawnDataHookPtr,
			&orig_ReadUnrealPawnDataMutex,
			"ReadUnrealPawnData")) return false;
	}

	return !error;
}

void EndScene::HookHelp::sendUnrealPawnDataHook() {
	// this gets called many times every frame, presumably once per entity, but there're way more entities and they're not in the entityList.list
	++detouring.hooksCounter;
	detouring.markHookRunning("SendUnrealPawnData", true);
	{
		endScene.sendUnrealPawnDataHook((char*)this);
		{
			bool needToUnlock = false;
			if (!endScene.orig_SendUnrealPawnDataMutexLocked || endScene.orig_SendUnrealPawnDataMutexThreadId != GetCurrentThreadId()) {
				needToUnlock = true;
				endScene.orig_SendUnrealPawnDataMutex.lock();
				endScene.orig_SendUnrealPawnDataMutexLocked = true;
				endScene.orig_SendUnrealPawnDataMutexThreadId = GetCurrentThreadId();
			}
			endScene.orig_SendUnrealPawnData((char*)this);
			if (needToUnlock) {
				endScene.orig_SendUnrealPawnDataMutexLocked = false;
				endScene.orig_SendUnrealPawnDataMutex.unlock();
			}
		}
	}
	detouring.markHookRunning("SendUnrealPawnData", false);
	--detouring.hooksCounter;
}

void EndScene::HookHelp::readUnrealPawnDataHook() {
	// this read happens many times every frame and it appears to be synchronized with sendUnrealPawnDataHook via a simple SetEvent.
	// the model we built might not be super precise and probably is not how the game sends data over from the logic thread to the graphics thread,
	// but it's precise enough to never fail so we'll keep using it
	++detouring.hooksCounter;
	detouring.markHookRunning("ReadUnrealPawnData", true);
	{
		endScene.readUnrealPawnDataHook((char*)this);
	}
	detouring.markHookRunning("ReadUnrealPawnData", false);
	--detouring.hooksCounter;
}

void EndScene::sendUnrealPawnDataHook(char* thisArg) {
	if (*aswEngine == nullptr) return;
	entityList.populate();
	if (entityList.count < 1) return;
	if (*(char**)(*(char**)(entityList.slots[0] + 0x27a8) + 0x384) != thisArg) return;
	endScene.logic();
}

void EndScene::logic() {
	std::unique_lock<std::mutex> guard(graphics.drawDataPreparedMutex);
	if (!graphics.drawDataPrepared.isPrepared) {
		checkUnloadDll();

		bool recordingLocal = recording;
		bool timeToEof = false;
		bool canPrepareDrawData = false;
		bool needToClearHitDetection = false;
		bool matchIsRunning = false;
		bool menuOpen = false;
		graphics.drawDataPrepared.clear();
		graphics.drawDataPrepared.isPrepared = true;
		graphics.drawDataPrepared.matchCounter = previouslyPreparedMatchCounter;
		if (*aswEngine == nullptr) {
			needToClearHitDetection = true;
		}
		else {
			isEof = false;
			if (altModes.isGameInNormalMode(&needToClearHitDetection, &timeToEof, &menuOpen)) {
				matchIsRunning = true;
				//matchIsRunning = game.isMatchRunning();
				if (!(matchIsRunning ? true : altModes.roundendCameraFlybyType() != 8)) {
					needToClearHitDetection = true;
				}
				else {
					canPrepareDrawData = true;
				}
			}
		}

		entityList.populate();
		logOnce(fputs("entityList.populate() called\n", logfile));
		if (canPrepareDrawData) {
			if (!entityList.areAnimationsNormal()) {
				timeToEof = true;
				needToClearHitDetection = true;
				canPrepareDrawData = false;
			}
		}

		GameMode gameMode = game.getGameMode();
		bool dontRecordInputs = false;
		if (menuOpen && gameMode != GAME_MODE_NETWORK) {
			dontRecordInputs = true;
		}
		if (canPrepareDrawData) {
			invisChipp.onEndSceneStart();
			drawnEntities.clear();
		}

		if (needStartRecording) {
			needStartRecording = false;
			if (!recordingLocal) {
				graphics.drawDataPrepared.isFirst = true;
				recording = true;
				recordingLocal = true;
				p1PreviousTimeOfTakingScreen = ~0;
				p2PreviousTimeOfTakingScreen = ~0;
				logwrap(fputs("EndScene started recording\n", logfile));
				//if (gameMode == GAME_MODE_TRAINING || gameMode == GAME_MODE_TUTORIAL) {
				// we will always use the inifinite match counter, because inputs can be made while the game is frozen
					matchCounterForInfiniteModes = 0xFFFFFFFF;
				//}
			}
		}
		if (timeToEof && recordingLocal) {
			logwrap(fputs("Time to eof\n", logfile));
			recordingLocal = false;
			recording = false;
			isEof = true;
			graphics.drawDataPrepared.isEof = true;
		}
		graphics.drawDataPrepared.playerSide = game.getPlayerSide();
		if (recordingLocal) {
			graphics.drawDataPrepared.facings[0] = 0;
			graphics.drawDataPrepared.facings[1] = 0;
			if (entityList.count > 0) {
				Entity ent{ entityList.slots[0] };
				graphics.drawDataPrepared.facings[0] = (ent.isFacingLeft() ? -1 : 1);
				graphics.drawDataPrepared.characterTypes[0] = ent.characterType();
			}
			if (entityList.count > 1) {
				Entity ent{ entityList.slots[1] };
				graphics.drawDataPrepared.facings[1] = (Entity{ entityList.slots[1] }.isFacingLeft() ? -1 : 1);
				graphics.drawDataPrepared.characterTypes[1] = ent.characterType();
			}
			graphics.drawDataPrepared.gameMode = gameMode;
			if (gameMode == GAME_MODE_REPLAY) {
				for (int i = 0; i < 2; ++i) {
					// Thanks to WorseThanYou for studying the structure and location of the replay data
					graphics.drawDataPrepared.replayStates[i].index = *(int*)(game.replayDataAddr + 0x25380 + 0x24 * i + 0x4);
					graphics.drawDataPrepared.replayStates[i].repeatIndex = *(int*)(game.replayDataAddr + 0x25380 + 0x24 * i + 0x8);
				}
				graphics.drawDataPrepared.replayDataInputsPtr = (void*)(game.replayDataAddr + 0x2100);
			}
		}
		bool needSendInputs = false;
		graphics.drawDataPrepared.inputDisplayDisabled = gifMode.inputDisplayDisabled
			|| (gameMode == GAME_MODE_NETWORK && graphics.drawDataPrepared.playerSide != 2);
		if (((recordingLocal || !graphics.drawDataPrepared.inputDisplayDisabled) && !dontRecordInputs) && *aswEngine) {
			memcpy(graphics.drawDataPrepared.inputRingBuffers, *aswEngine + 4 + game.inputRingBuffersOffset, 0x7e * 2);  // two ring buffers, 0x7e in size each
			graphics.drawDataPrepared.hasInputs = true;
			needSendInputs = recordingLocal;
		}
		if (canPrepareDrawData || needSendInputs) {
			bool frameHasChanged = false;
			unsigned int p1CurrentTimer = ~0;
			unsigned int p2CurrentTimer = ~0;
			if (entityList.count >= 1) {
				p1CurrentTimer = Entity{ entityList.slots[0] }.currentAnimDuration();
			}
			if (entityList.count >= 2) {
				p2CurrentTimer = Entity{ entityList.slots[1] }.currentAnimDuration();
			}
			if (p1CurrentTimer != p1PreviousTimeOfTakingScreen
				|| p2CurrentTimer != p2PreviousTimeOfTakingScreen) {
				frameHasChanged = true;
			}
			if (recordingLocal) {

				p1PreviousTimeOfTakingScreen = p1CurrentTimer;
				p2PreviousTimeOfTakingScreen = p2CurrentTimer;

				//if (gameMode == GAME_MODE_TRAINING || gameMode == GAME_MODE_TUTORIAL) {
					graphics.drawDataPrepared.matchCounter = matchCounterForInfiniteModes;
					previouslyPreparedMatchCounter = matchCounterForInfiniteModes;
				//}
				//else {
				//	graphics.drawDataPrepared.matchCounter = game.getMatchCounter();
				//}

				if (matchCounterForInfiniteModes == 0) {
					recordingLocal = false;
					recording = false;
					logwrap(fputs("EndScene stopped recordingLocal due to matchCounterForInfiniteModes overflow\n", logfile));
				}
				else {
					--matchCounterForInfiniteModes;
				}
			}
			else if (frameHasChanged) {
				p1PreviousTimeOfTakingScreen = ~0;
				p2PreviousTimeOfTakingScreen = ~0;
			}
			if (recordingLocal) {
				graphics.drawDataPrepared.recordThisFrame = canPrepareDrawData;
			}
		}
		graphics.drawDataPrepared.recording = recordingLocal;

		graphics.drawDataPrepared.hitboxDisplayDisabled = gifMode.hitboxDisplayDisabled;
		if (canPrepareDrawData && (!graphics.drawDataPrepared.hitboxDisplayDisabled || recordingLocal)) {

			logOnce3(fprintf(logfile, "entity count: %d\n", entityList.count));

			for (auto i = 0; i < entityList.count; i++)
			{
				Entity ent(entityList.list[i]);
				if (isEntityAlreadyDrawn(ent)) continue;

				bool active = ent.isActive();
				logOnce3(fprintf(logfile, "drawing entity # %d. active: %d\n", i, (int)active));

				bool invisChippNeedToHide = invisChipp.needToHide(ent);
				DrawHitboxArrayCallParams hurtbox;
				collectHitboxes(ent, active, &hurtbox, &graphics.drawDataPrepared.hitboxes, &graphics.drawDataPrepared.points, &graphics.drawDataPrepared.pushboxes);
				if (hurtbox.hitboxCount) {
					HitDetector::WasHitInfo wasHitResult = hitDetector.wasThisHitPreviously(ent, hurtbox);
					if (!wasHitResult.wasHit) {
						graphics.drawDataPrepared.hurtboxes.push_back({ invisChippNeedToHide, false, hurtbox });
					}
					else {
						graphics.drawDataPrepared.hurtboxes.push_back({ invisChippNeedToHide, true, hurtbox, wasHitResult.hurtbox });
					}
				}
				logOnce3(fputs("collectHitboxes(...) call successful\n", logfile));
				drawnEntities.push_back(ent);
				logOnce3(fputs("drawnEntities.push_back(...) call successful\n", logfile));

				// Attached entities like dusts
				const auto attached = *(char**)(ent + 0x204);
				if (attached != nullptr) {
					logOnce3(fprintf(logfile, "Attached entity: %p\n", attached));
					size_t hitboxesIt = graphics.drawDataPrepared.hitboxes.size();
					size_t pointsIt = graphics.drawDataPrepared.points.size();
					size_t pushboxesIt = graphics.drawDataPrepared.pushboxes.size();
					collectHitboxes(attached, active, &hurtbox, &graphics.drawDataPrepared.hitboxes, &graphics.drawDataPrepared.points, &graphics.drawDataPrepared.pushboxes);
					for (auto it = graphics.drawDataPrepared.hitboxes.begin() + hitboxesIt; it != graphics.drawDataPrepared.hitboxes.end(); ++it) {
						it->invisChippNeedToHide = invisChippNeedToHide;
					}
					for (auto it = graphics.drawDataPrepared.points.begin() + pointsIt; it != graphics.drawDataPrepared.points.end(); ++it) {
						it->invisChippNeedToHide = invisChippNeedToHide;
					}
					for (auto it = graphics.drawDataPrepared.pushboxes.begin() + pushboxesIt; it != graphics.drawDataPrepared.pushboxes.end(); ++it) {
						it->invisChippNeedToHide = invisChippNeedToHide;
					}
					graphics.drawDataPrepared.hurtboxes.push_back({ invisChippNeedToHide, false, hurtbox });
					drawnEntities.push_back(attached);
				}
			}

			logOnce3(fputs("got past the entity loop\n", logfile));
			hitDetector.drawHits();
			logOnce3(fputs("hitDetector.drawDetected() call successful\n", logfile));
			throws.drawThrows();
			logOnce3(fputs("throws.drawThrows() call successful\n", logfile));

			// Camera values are updated later, after this, in a updateCameraHook call
			graphics.drawDataPrepared.empty = false;
			#ifdef LOG_PATH
			didWriteOnce3 = true;
			#endif
		}
		if (prevRecording && !recordingLocal) {
			logwrap(fputs("EndScene notices the recording stop request\n", logfile));
			graphics.drawDataPrepared.isEof = true;
			graphics.drawDataPrepared.recording = true;
		}
		prevRecording = recordingLocal;

		if (needToClearHitDetection) {
			hitDetector.clearAllBoxes();
		}
		#ifdef LOG_PATH
		didWriteOnce = true;
		#endif
	}
}

void EndScene::readUnrealPawnDataHook(char* thisArg) {
	{
		std::unique_lock<std::mutex> guard(graphics.drawDataPreparedMutex);
		if (graphics.drawDataPrepared.isPrepared && graphics.needNewDrawData || graphics.drawDataPrepared.isEof) {
			graphics.drawDataUse.clear();
			graphics.drawDataPrepared.copyTo(&graphics.drawDataUse);
			if (graphics.drawDataUse.isEof) {
				logwrap(fputs("readUnrealPawnDataHook got eof\n", logfile));
			}
			graphics.drawDataPrepared.empty = true;
			graphics.drawDataPrepared.hasInputs = false;
			graphics.drawDataPrepared.isEof = false;
			graphics.drawDataPrepared.isPrepared = false;
			graphics.needNewDrawData = false;
		}
	}
	{
		std::unique_lock<std::mutex> guard(camera.valuesPrepareMutex);
		if (!camera.valuesPrepare.sent && graphics.needNewCameraData) {
			camera.valuesPrepare.copyTo(camera.valuesUse);
			camera.valuesPrepare.sent = true;
			graphics.needNewCameraData = false;
		}
	}
	{
		std::unique_lock<std::mutex> guard(orig_ReadUnrealPawnDataMutex);
		orig_ReadUnrealPawnData(thisArg);
	}
}

HRESULT __stdcall hook_EndScene(IDirect3DDevice9* device) {
	++detouring.hooksCounter;
	detouring.markHookRunning("EndScene", true);
	HRESULT result;
	{
		bool hasPresentFlag = endScene.consumePresentFlag();
		if (hasPresentFlag) {
			endScene.endSceneHook(device);
		}
		{
			std::unique_lock<std::mutex> guard(endScene.orig_EndSceneMutex);
			result = endScene.orig_EndScene(device);
		}
		if (hasPresentFlag) {
			endScene.endSceneHookAfter(device);
			graphics.needNewDrawData = true;
			graphics.needNewCameraData = true;
			#ifdef LOG_PATH
			didWriteOnce2 = true;
			#endif
		}
	}
	detouring.markHookRunning("EndScene", false);
	--detouring.hooksCounter;
	return result;
}

HRESULT __stdcall hook_Present(IDirect3DDevice9* device, const RECT* pSourceRect, const RECT* pDestRect, HWND hDestWindowOverride, const RGNDATA* pDirtyRegion) {
	++detouring.hooksCounter;
	detouring.markHookRunning("Present", true);
	HRESULT result;
	{
		result = endScene.presentHook(device, pSourceRect, pDestRect, hDestWindowOverride, pDirtyRegion);
	}
	detouring.markHookRunning("Present", false);
	--detouring.hooksCounter;
	return result;
}

HRESULT EndScene::presentHook(IDirect3DDevice9* device, const RECT* pSourceRect, const RECT* pDestRect, HWND hDestWindowOverride, const RGNDATA* pDirtyRegion) {
	setPresentFlag();
	std::unique_lock<std::mutex> guard(endScene.orig_PresentMutex);
	HRESULT result = orig_Present(device, pSourceRect, pDestRect, hDestWindowOverride, pDirtyRegion);  // may call d3d9.dll::EndScene() (and, consecutively, the hook)
	return result;
}

bool EndScene::consumePresentFlag() {
	if (!presentCalled) return false;
	presentCalled = false;
	return true;
}

void EndScene::setPresentFlag() {
	presentCalled = true;
}

bool EndScene::isEntityAlreadyDrawn(const Entity& ent) const {
	return std::find(drawnEntities.cbegin(), drawnEntities.cend(), ent) != drawnEntities.cend();
}

void EndScene::endSceneHook(IDirect3DDevice9* device) {
	if (graphics.drawDataUse.empty && !graphics.drawDataUse.hasInputs && !graphics.drawDataUse.isEof || !*aswEngine) {
		outOfGameLogic();
		return;
	}
	needDrawInEndSceneHook = !graphics.drawDataUse.hitboxDisplayDisabled || !graphics.drawDataUse.inputDisplayDisabled && graphics.drawDataUse.hasInputs;
	graphics.onEndSceneStart(device);
	if (!graphics.drawDataUse.hitboxDisplayDisabled) {
		drawOutlineCallParamsManager.onEndSceneStart();
		camera.onEndSceneStart();
	}

	if (needDrawInEndSceneHook) {
		if ((graphics.drawDataUse.recordThisFrame || (graphics.drawDataUse.hasInputs || graphics.drawDataUse.isEof) && graphics.drawDataUse.recording)
				&& (graphics.lastSentMatchCounter != graphics.drawDataUse.matchCounter || graphics.drawDataUse.isFirst || graphics.drawDataUse.isEof)) {
			graphics.takeScreenshotSimple(device, graphics.drawDataUse.recordThisFrame, graphics.drawDataUse.hasInputs, graphics.drawDataUse.isEof);
			graphics.lastSentMatchCounter = graphics.drawDataUse.matchCounter;
			graphics.drawDataUse.isFirst = false;
			graphics.drawDataUse.isEof = false;
			graphics.drawDataUse.recordThisFrame = false;
		}
		if (!graphics.drawDataUse.recording || graphics.drawDataUse.isEof && graphics.drawDataUse.recording) {
			graphics.stopRecording();
		}
		removeInvisChippStuff();
		graphics.drawAll();
		graphics.drawDataUse.recordThisFrame = false;
	} else {
		graphics.rememberRenderTarget();
	}
}

void EndScene::endSceneHookAfter(IDirect3DDevice9* device) {
	if (graphics.drawDataUse.empty && !graphics.drawDataUse.hasInputs && !graphics.drawDataUse.isEof || !*aswEngine) {
		return;
	}
	if (!needDrawInEndSceneHook) {
		if ((graphics.drawDataUse.recordThisFrame || (graphics.drawDataUse.hasInputs || graphics.drawDataUse.isEof) && graphics.drawDataUse.recording)
				&& (graphics.lastSentMatchCounter != graphics.drawDataUse.matchCounter || graphics.drawDataUse.isFirst || graphics.drawDataUse.isEof)) {
			graphics.takeScreenshotSimple(device, graphics.drawDataUse.recordThisFrame, graphics.drawDataUse.hasInputs, graphics.drawDataUse.isEof);
			graphics.lastSentMatchCounter = graphics.drawDataUse.matchCounter;
			graphics.drawDataUse.isFirst = false;
			graphics.drawDataUse.isEof = false;
			graphics.drawDataUse.recordThisFrame = false;
		}
		if (!graphics.drawDataUse.recording || graphics.drawDataUse.isEof && graphics.drawDataUse.recording) {
			graphics.stopRecording();
		}
		graphics.drawDataUse.recordThisFrame = false;
	}

	graphics.clearRememberedRenderTarget();

}

void EndScene::removeInvisChippStuff() {
	{
		auto it = graphics.drawDataUse.hurtboxes.begin();
		while (it != graphics.drawDataUse.hurtboxes.end()) {
			if (it->invisChippNeedToHide) {
				it = graphics.drawDataUse.hurtboxes.erase(it);
				continue;
			}
			++it;
		}
	}
	{
		auto it = graphics.drawDataUse.hitboxes.begin();
		while (it != graphics.drawDataUse.hitboxes.end()) {
			if (it->invisChippNeedToHide) {
				it = graphics.drawDataUse.hitboxes.erase(it);
				continue;
			}
			++it;
		}
	}
	{
		auto it = graphics.drawDataUse.pushboxes.begin();
		while (it != graphics.drawDataUse.pushboxes.end()) {
			if (it->invisChippNeedToHide) {
				it = graphics.drawDataUse.pushboxes.erase(it);
				continue;
			}
			++it;
		}
	}
	{
		auto it = graphics.drawDataUse.points.begin();
		while (it != graphics.drawDataUse.points.end()) {
			if (it->invisChippNeedToHide) {
				it = graphics.drawDataUse.points.erase(it);
				continue;
			}
			++it;
		}
	}
	{
		auto it = graphics.drawDataUse.throwBoxes.begin();
		while (it != graphics.drawDataUse.throwBoxes.end()) {
			if (it->invisChippNeedToHide) {
				it = graphics.drawDataUse.throwBoxes.erase(it);
				continue;
			}
			++it;
		}
	}
}

void EndScene::outOfGameLogic() {
	checkUnloadDll();
	if (!*aswEngine) {
		if (recording) {
			if (!isEof) {
				isEof = true;
				graphics.drawDataUse.isEof = true;
				graphics.takeScreenshotSimple(nullptr, false, false, true);
				graphics.drawDataUse.isEof = false;
			}
			p1PreviousTimeOfTakingScreen = ~0;
			p2PreviousTimeOfTakingScreen = ~0;
		}
		if (graphics.drawDataUse.hasInputs) {
			graphics.clearCollectedInputs();
			graphics.drawDataUse.hasInputs = false;
		}
		prevRecording = false;
	}
}

void EndScene::checkUnloadDll() {
	if (unloadDll) {
		logwrap(fputs("EndScene found DLL unload request.\n", logfile));
		fileMappingManager.joinAllThreads();
		unloadDll = false;
		logwrap(fputs("Calling FreeLibrary on self in a detached thread.\n", logfile));
		CloseHandle(CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)FreeLibrary, &__ImageBase, 0, 0));
	}
}
