#pragma once
#include "gameModes.h"
#include <mutex>

extern const char** aswEngine;

class Game {
public:
	bool onDllMain();
	GameMode getGameMode() const;
	bool currentModeIsOnline() const;
	bool isNonOnline() const;
	char getPlayerSide() const;
	bool isMatchRunning() const;
	bool isTrainingMode() const;
	unsigned int getMatchCounter() const;
	unsigned int inputRingBuffersOffset = 0;
	const char* replayDataAddr = nullptr;
private:
	const char** gameDataPtr = nullptr;
	const char** playerSideNetworkHolder = nullptr;
	unsigned int isMatchRunningOffset = 0;
	unsigned int matchCounterOffset = 0;
};

extern Game game;
