#pragma once
#include "characterTypes.h"

enum CodecType {
	CODEC_TYPE_H264
};

struct HitboxData {
	float offX = 0.F;
	float offY = 0.F;
	float sizeX = 0.F;
	float sizeY = 0.F;
	HitboxData() = default;
	HitboxData(const HitboxData& source) = default;
	HitboxData(HitboxData&& source) noexcept = default;
	HitboxData& operator=(const HitboxData& source) = default;
	HitboxData& operator=(HitboxData&& source) noexcept = default;
	HitboxData(float offX, float offY, float sizeX, float sizeY);
};

struct ReplayIndexEntry {
	unsigned long long fileOffset = 0;
};

struct ReplayFileHeader {
	char magicNumbers[13] = "ggxrd_replay";
	int version = 0;
	int codec = CODEC_TYPE_H264;
	CharacterType characterTypes[2] = { CHARACTER_TYPE_SOL };
	unsigned long long logicsCount = 0;
	unsigned long long indexOffset = 0;
	unsigned long long hitboxDataOffset = 0;
	unsigned int hitboxesCount = 0;
	unsigned long long replayInputsOffset[2] = { 0 };
	unsigned int replayInputsSize[2] = { 0 };
};
