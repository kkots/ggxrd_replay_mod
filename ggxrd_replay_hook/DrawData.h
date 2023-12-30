#pragma once
#include "DrawHitboxArrayCallParams.h"
#include "DrawOutlineCallParams.h"
#include "DrawPointCallParams.h"
#include "DrawBoxCallParams.h"
#include "ReplayState.h"
#include "InputRingBuffer.h"
#include "ComplicatedHurtbox.h"
#include "ThrowInfo.h"
#include "gameModes.h"
#include "characterTypes.h"
struct DrawData {
	std::vector<ComplicatedHurtbox> hurtboxes;
	std::vector<DrawHitboxArrayCallParams> hitboxes;
	std::vector<DrawBoxCallParams> pushboxes;
	std::vector<DrawPointCallParams> points;
	std::vector<DrawBoxCallParams> throwBoxes;
	std::vector<ThrowInfo> throwInfos;
	void clear();
	void copyTo(DrawData* destination);
	void copyInputsTo(DrawData* destination);
	bool empty = false;
	bool recordThisFrame = false;
	bool recording = false;
	bool isFirst = false;
	bool isPrepared = false;
	bool hitboxDisplayDisabled = true;
	bool inputDisplayDisabled = true;
	unsigned int matchCounter = 0;
	ReplayState replayStates[2];
	InputRingBuffer inputRingBuffers[2];
	GameMode gameMode = GAME_MODE_INVALID;
	char playerSide = 2;
	CharacterType characterTypes[2] = { CHARACTER_TYPE_SOL };
	char facings[2] = {0};
	void* replayDataInputsPtr = nullptr;
	bool hasInputs = false;
	bool isEof = false;
	bool wontHaveFrame = false;
};
