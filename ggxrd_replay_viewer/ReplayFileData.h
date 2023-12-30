#pragma once
#include "framework.h"
#include <vector>
#include "ReplayFileStructs.h"
#include <unordered_map>
#include <mutex>
#include <string>
#include "StoredInput.h"
class ReplayFileData
{
public:
	bool startTempFile();
	void closeFile();
	bool startFile(const wchar_t* path, bool forReading);
	std::mutex mutex;
	FILE* file = nullptr;
	std::wstring path;
	std::mutex indexMutex;
	std::vector<ReplayIndexEntry> index;
	std::mutex hitboxDataMutex;
	std::unordered_map<unsigned int, std::vector<HitboxData>> hitboxData;
	std::mutex replayInputsMutex;
	std::vector<StoredInput> replayInputs[2] = { {}, {} };
	ReplayFileHeader header;
};

extern ReplayFileData replayFileData;
