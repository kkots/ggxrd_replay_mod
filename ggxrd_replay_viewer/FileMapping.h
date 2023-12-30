#pragma once
#include "gameModes.h"
#include <thread>
#include <vector>
#include <mutex>
#include "LogicInfo.h"
#include "EncoderInput.h"
#include "characterTypes.h"

const unsigned int FILE_MAPPING_SLOTS = 2;

struct ImageInfo {
	unsigned int width = 0;
	unsigned int height = 0;
	unsigned int origWidth = 0;
	unsigned int origHeight = 0;
	unsigned int pitch = 0;
	unsigned int offset = 0;
	bool occupied = false;
	bool consumed = false;
	unsigned int matchCounter = 0;
	bool empty = true;
	bool isEof = false;
};

class FileMapping
{
public:
	LogicInfo logicInfos[FILE_MAPPING_SLOTS] = { 0 };
	ImageInfo imageInfos[FILE_MAPPING_SLOTS] = { 0 };
	GameMode gameMode = GAME_MODE_INVALID;
	CharacterType characterTypes[2] = { CHARACTER_TYPE_SOL };
	char playerSide = 2;
	bool displayBoxes = false;
	bool displayInputs = false;
	bool needStartRecording = false;
	bool recording = false;
	unsigned int replayDataPtr = 0;
	bool unloadDll = false;
};

class FileMappingGuard {
public:
	FileMappingGuard();
	~FileMappingGuard();
	FileMappingGuard(const FileMappingGuard& src) = delete;
	FileMappingGuard& operator=(const FileMappingGuard& src) = delete;
	FileMappingGuard(FileMappingGuard&& src) noexcept;
	FileMappingGuard& operator=(FileMappingGuard&& src) noexcept;
	void lock();
	void markWroteSomething();
	void unlock();
private:
	void moveFrom(FileMappingGuard& src) noexcept;
	bool locked = false;
	bool wroteSomething = false;
};

class FileMappingManager {
public:
	volatile FileMapping* view = nullptr;
	void startThread();
	void joinAllThreads();
	void sendStartRecordingCommand();
	void pushSettingsUpdate(bool displayBoxes, bool displayInputs);
private:
	std::thread thread;
	bool threadStarted = false;
	void threadLoop();
	unsigned int lastFrameMatchCounter = 0;
	bool lastFrameMatchCounterIsEmpty = true;
};
