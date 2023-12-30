#pragma once
#include "InputRingBuffer.h"
#include "ReplayState.h"
#include "gameModes.h"
#include <thread>
#include <mutex>
#include "Camera.h"
#include "DrawHitboxArrayCallParams.h"
#include "ComplicatedHurtbox.h"
#include "DrawBoxCallParams.h"
#include "DrawPointCallParams.h"
#include "ThrowInfo.h"
#include <vector>

const unsigned int FILE_MAPPING_SLOTS = 2;

struct LogicInfo {
	unsigned int boxesOffset = 0;  // into the file mapping
	unsigned int boxesSize = 0;
	unsigned int matchCounter = 0;
	bool occupied = false;
	bool consumed = true;
	ReplayState replayStates[2] = {0};
	InputRingBuffer inputRingBuffers[2] = {0};
	char facings[2] = {0};
	bool isEof = false;
	bool wontHaveFrame = false;
};

struct ImageInfo {
	unsigned int width = 0;
	unsigned int height = 0;
	unsigned int origWidth = 0;
	unsigned int origHeight = 0;
	unsigned int pitch = 0;
	unsigned int offset = 0;  // into the file mapping
	bool occupied = false;
	bool consumed = true;
	unsigned int matchCounter = 0;
	bool empty = true;
	bool isEof = false;
};

class FileMapping
{
public:
	LogicInfo logicInfos[FILE_MAPPING_SLOTS] = {0};
	ImageInfo imageInfos[FILE_MAPPING_SLOTS] = {0};
	GameMode gameMode = GAME_MODE_INVALID;
	CharacterType characterTypes[2] = { CHARACTER_TYPE_SOL };
	char playerSide = 2;
	bool displayBoxes = false;
	bool displayInputs = false;
	bool needStartRecording = false;
	bool recording = false;
	unsigned int replayDataPtr = 0;  // into the GuiltyGearXrd.exe memory address space, to be read through ReadProcessMemory
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
	bool onDllMain();
	void onDllDetach();
	void joinAllThreads();
	HANDLE mapping = NULL;
	volatile FileMapping* view = nullptr;
	HANDLE eventChangesMade = NULL;
	HANDLE eventChangesReceived = NULL;
	HANDLE mutex = NULL;
	HANDLE eventExiting = NULL;
	std::mutex allJoinMutex;
	std::thread thread;
	bool threadsJoined = false;
	std::thread framesThread;
	std::thread logicThread;
	bool needLogicBufferFreed = false;
	HANDLE eventLogicBufferFreed = NULL;
private:
	void threadLoop();
	void framesThreadLoop();
	void logicThreadLoop();
	void readChanges(FileMappingGuard& guard);
	void writeChar(char*& ptr, char value);
	void writeUnsignedInt(char*& ptr, unsigned int value);
	void writeInt(char*& ptr, int value);
	void writeFloat(char*& ptr, float value);
	void serializeArraybox(char*& ptr, const DrawHitboxArrayCallParams& arraybox);
	void serializeCamera(char*& ptr, const CameraValues& camera);
	void serializeHurtboxes(char*& ptr, const std::vector<ComplicatedHurtbox>& hurtboxes);
	void serializeHitboxes(char*& ptr, const std::vector<DrawHitboxArrayCallParams>& hitboxes);
	void serializePushboxes(char*& ptr, const std::vector<DrawBoxCallParams>& pushboxes);
	void serializePoints(char*& ptr, const std::vector<DrawPointCallParams>& points);
	void serializeThrowInfos(char*& ptr, const std::vector<ThrowInfo>& throwInfos);
};

extern FileMappingManager fileMappingManager;
