#pragma once
#include "framework.h"
#include <atomic>
#include "ReplayFileStructs.h"
#include "DecoderInput.h"

class FileReaderThread
{
public:
	void threadLoop();
	HANDLE eventResponse = NULL;
	std::atomic_bool needSwitchFiles{ false };
private:
	unsigned int matchCounter = 0;
	bool matchCounterEmpty = true;
	int currentFilePosition = 0;
	char readChar();
	int readInt();
	unsigned int readUnsignedInt();
	float readFloat();
	UINT64 readUINT64();
	bool scrubbing = false;
	unsigned int scrubbingMatchCounter = 0;
	ReplayIndexEntry scrubbingIndexEntry;
	bool scrubbingIndexEntryInvalidated = true;
	DecoderInputEntry newEntry;
};

