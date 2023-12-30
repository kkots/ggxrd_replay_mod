#include "pch.h"
#include "Game.h"
#include "memoryFunctions.h"
#include "Detouring.h"
#include "EndScene.h"

const char** aswEngine = nullptr;

Game game;

bool Game::onDllMain() {
	bool error = false;

	char aswEngineSig[] = "\x85\xC0\x78\x74\x83\xF8\x01";
	aswEngine = (const char**)sigscanOffset(
		"GuiltyGearXrd.exe",
		aswEngineSig,
		_countof(aswEngineSig),
		{-4, 0},
		&error, "aswEngine");

	char gameDataPtrSig[] = "\x33\xC0\x38\x41\x44\x0F\x95\xC0\xC3\xCC";
	gameDataPtr = (const char**)sigscanOffset(
		"GuiltyGearXrd.exe",
		gameDataPtrSig,
		_countof(gameDataPtrSig),
		{-4, 0},
		&error, "gameDataPtr");

	playerSideNetworkHolder = (const char**)sigscanOffset(
		"GuiltyGearXrd.exe",
		"\x8b\x0d\x00\x00\x00\x00\xe8\x00\x00\x00\x00\x3c\x10\x75\x10\xa1\x00\x00\x00\x00\x85\xc0\x74\x07\xc6\x80\x0c\x12\x00\x00\x01\xc3",
		"xx????x????xxxxx????xxxxxxxxxxxx",
		{16, 0},
		&error, "playerSideNetworkHolder");

	// Thanks to WorseThanYou for finding the location of the input ring buffers, their sizes and their structure
	// ghidra sig: 8b 44 24 14 8b d7 6b d2 7e 56 8d 8c 02 ?? ?? ?? ?? e8 ?? ?? ?? ?? 66 89 74 7c 18
	inputRingBuffersOffset = (unsigned int)sigscanOffset(
		"GuiltyGearXrd.exe",
		"\x8b\x44\x24\x14\x8b\xd7\x6b\xd2\x7e\x56\x8d\x8c\x02\x00\x00\x00\x00\xe8\x00\x00\x00\x00\x66\x89\x74\x7c\x18",
		"xxxxxxxxxxxxx????x????xxxxx",
		{ 13, 0 },
		&error, "inputRingBuffersOffset");

	// Thanks to WorseThanYou for finding the get_replay_inputs function
	// ghidra sig: e8 ?? ?? ?? ?? 85 c0 74 17 8b 4c 24 10 51 55 e8 ?? ?? ?? ?? 0f b7 c0 89 44 24 18 e9 e2 00 00 00 8b 74 ac 28
	uintptr_t sigscanResult = sigscanOffset(
		"GuiltyGearXrd.exe",
		"\xe8\x00\x00\x00\x00\x85\xc0\x74\x17\x8b\x4c\x24\x10\x51\x55\xe8\x00\x00\x00\x00\x0f\xb7\xc0\x89\x44\x24\x18\xe9\xe2\x00\x00\x00\x8b\x74\xac\x28",
		"x????xxxxxxxxxxx????xxxxxxxxxxxxxxxx",
		&error, "get_replay_inputs calling place");
	if (sigscanResult) {
		sigscanResult = followRelativeCall(sigscanResult + 15);
		logwrap(fprintf(logfile, "Got get_replay_inputs address: %.8x\n", sigscanResult));
	}
	if (sigscanResult) {
		// ghidra sig: 8b 44 24 08 8b 4c 24 04 50 51 b9 ?? ?? ?? ?? e8 ?? ?? ?? ?? c3
		if (!binaryCompareMask((const char*)sigscanResult,
				"\x8b\x44\x24\x08\x8b\x4c\x24\x04\x50\x51\xb9\x00\x00\x00\x00\xe8\x00\x00\x00\x00\xc3",
				"xxxxxxxxxxx????x????x")) {
			logwrap(fputs("Error: get_replay_inputs does not match sig.\n", logfile));
			error = true;
		}
	}
	if (!error) {
		replayDataAddr = *(const char**)(sigscanResult + 11);
		logwrap(fprintf(logfile, "replayDataAddr final address: %p\n", replayDataAddr));
		if (!replayDataAddr) error = true;
	}

	// Thanks to WorseThanYou for finding the is the match currently running value
	// ghidra sig: 83 c4 08 83 f8 01 75 52 8b 0d ?? ?? ?? ?? 39 a9 ?? ?? ?? ?? 74 44 8d 54 24 18 52 8d 44 24 14 66 0f 57 c0
	char isMatchRunningOffsetSig[] = "\x83\xc4\x08\x83\xf8\x01\x75\x52\x8b\x0d\x00\x00\x00\x00\x39\xa9\x00\x00\x00\x00\x74\x44\x8d\x54\x24\x18\x52\x8d\x44\x24\x14\x66\x0f\x57\xc0";
	char isMatchRunningOffsetMask[] = "xxxxxxxxxx????xx????xxxxxxxxxxxxxxx";
	substituteWildcard(isMatchRunningOffsetMask, isMatchRunningOffsetSig, (char*)&aswEngine, 4, 0);
	isMatchRunningOffset = (unsigned int)sigscanOffset(
		"GuiltyGearXrd.exe",
		isMatchRunningOffsetSig,
		isMatchRunningOffsetMask,
		{ 16, 0 },
		&error, "isMatchRunningOffset");

	// Thanks to WorseThanYou for finding the match timer/counter
	// ghidra sig: 83 3f 04 0f 85 82 00 00 00 a1 ?? ?? ?? ?? 83 b8 ?? ?? ?? ?? 00 7f 74 83 b8 ?? ?? ?? ?? 00 75 6b 83 b8 ?? ?? ?? ?? 00 75 62 8b 35 ?? ?? ?? ?? 8b ce e8 ?? ?? ?? ?? 3c 03
	char matchCounterOffsetSig[] = "\x83\x3f\x04\x0f\x85\x82\x00\x00\x00\xa1\x00\x00\x00\x00\x83\xb8\x00\x00\x00\x00\x00\x7f\x74\x83\xb8\x00\x00\x00\x00\x00\x75\x6b\x83\xb8\x00\x00\x00\x00\x00\x75\x62\x8b\x35\x00\x00\x00\x00\x8b\xce\xe8\x00\x00\x00\x00\x3c\x03";
	char matchCounterOffsetMask[] = "xxxxxxxxxx????xx????xxxxx????xxxxx????xxxxx????xxx????xx";
	substituteWildcard(matchCounterOffsetMask, matchCounterOffsetSig, (char*)&aswEngine, 4, 0);
	matchCounterOffset = (unsigned int)sigscanOffset(
		"GuiltyGearXrd.exe",
		matchCounterOffsetSig,
		matchCounterOffsetMask,
		{ 16, 0 },
		&error, "matchCounterOffset");

	return !error;
}

char Game::getPlayerSide() const {
	if (getGameMode() == GAME_MODE_NETWORK) {
		if (!playerSideNetworkHolder) return 2;
		// Big thanks to WorseThanYou for finding this value
		return *(char*)(*playerSideNetworkHolder + 0x1734);  // 0 for p1 side, 1 for p2 side, 2 for observer
	} else if (gameDataPtr && *gameDataPtr) {
		return *(char*)(*gameDataPtr + 0x44);  // this makes sense for training mode for example (maybe only all single player modes)
	} else {
		return 2;
	}
}

GameMode Game::getGameMode() const {
	if (!gameDataPtr || !(*gameDataPtr)) return GAME_MODE_TRAINING;
	return (GameMode)*(*gameDataPtr + 0x45);
}

bool Game::isMatchRunning() const {
	if (!aswEngine) return false;
	return *(unsigned int*)(*aswEngine + isMatchRunningOffset) != 0; // thanks to WorseThanYou for finding this
}

bool Game::isTrainingMode() const {
	return getGameMode() == GAME_MODE_TRAINING;
}

bool Game::isNonOnline() const {
	GameMode gameMode = getGameMode();
	return gameMode == GAME_MODE_ARCADE
		|| gameMode == GAME_MODE_CHALLENGE
		|| gameMode == GAME_MODE_REPLAY
		|| gameMode == GAME_MODE_STORY
		|| gameMode == GAME_MODE_TRAINING
		|| gameMode == GAME_MODE_TUTORIAL
		|| gameMode == GAME_MODE_VERSUS;
}

bool Game::currentModeIsOnline() const {
	return !isNonOnline();
}

unsigned int Game::getMatchCounter() const {
	if (!*aswEngine) return 0;
	return *(unsigned int*)(*aswEngine + matchCounterOffset);
}
