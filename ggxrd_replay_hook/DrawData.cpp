#include "pch.h"
#include "DrawData.h"
#include "logging.h"
void DrawData::clear() {
	hurtboxes.clear();
	hitboxes.clear();
	pushboxes.clear();
	points.clear();
	throwBoxes.clear();
	throwInfos.clear();
	recording = false;
	isFirst = false;
	isPrepared = false;
	recordThisFrame = false;
	hitboxDisplayDisabled = true;
	inputDisplayDisabled = true;
	matchCounter = 0;
	memset(replayStates, 0, sizeof(replayStates));
	memset(inputRingBuffers, 0, sizeof(inputRingBuffers));
	gameMode = GAME_MODE_INVALID;
	playerSide = 2;
	characterTypes[0] = CHARACTER_TYPE_SOL;
	characterTypes[1] = CHARACTER_TYPE_SOL;
	facings[0] = 0;
	facings[1] = 0;
	replayDataInputsPtr = nullptr;
	hasInputs = false;
	empty = true;
	isEof = false;
}

void DrawData::copyTo(DrawData* destination) {
	destination->hurtboxes.insert(destination->hurtboxes.begin(), hurtboxes.begin(), hurtboxes.end());
	destination->hitboxes.insert(destination->hitboxes.begin(), hitboxes.begin(), hitboxes.end());
	destination->pushboxes.insert(destination->pushboxes.begin(), pushboxes.begin(), pushboxes.end());
	destination->points.insert(destination->points.begin(), points.begin(), points.end());
	destination->throwBoxes.insert(destination->throwBoxes.begin(), throwBoxes.begin(), throwBoxes.end());
	destination->throwInfos.insert(destination->throwInfos.begin(), throwInfos.begin(), throwInfos.end());
	destination->recordThisFrame = recordThisFrame;
	destination->isFirst = isFirst;
	destination->isPrepared = isPrepared;
	destination->recording = recording;
	destination->hitboxDisplayDisabled = hitboxDisplayDisabled;
	destination->matchCounter = matchCounter;
	memcpy(destination->replayStates, replayStates, sizeof(replayStates));
	memcpy(destination->facings, facings, sizeof(facings));
	destination->gameMode = gameMode;
	destination->playerSide = playerSide;
	memcpy(destination->characterTypes, characterTypes, sizeof(characterTypes));
	destination->replayDataInputsPtr = replayDataInputsPtr;
	destination->isEof = isEof;
	destination->empty = empty;
	copyInputsTo(destination);
}

void DrawData::copyInputsTo(DrawData* destination) {
	memcpy(destination->inputRingBuffers, inputRingBuffers, sizeof(inputRingBuffers));
	memcpy(destination->facings, facings, sizeof(facings));
	destination->inputDisplayDisabled = inputDisplayDisabled;
	destination->isFirst = isFirst;
	destination->isPrepared = isPrepared;
	destination->gameMode = gameMode;
	destination->playerSide = playerSide;
	memcpy(destination->characterTypes, characterTypes, sizeof(characterTypes));
	destination->replayDataInputsPtr = replayDataInputsPtr;
	destination->hasInputs = hasInputs;
	destination->recording = recording;
	destination->matchCounter = matchCounter;
	destination->isEof = isEof;
}
