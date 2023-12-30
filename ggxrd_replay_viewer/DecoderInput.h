#pragma once
#include <mutex>
#include <vector>
#include "LogicInfoRef.h"
#include "SampleWithBufferRef.h"

struct DecoderInputEntry {
	std::vector<LogicInfoRef> logics;
	std::vector<SampleWithBufferRef> samples;
};

class DecoderInput
{
public:
	void addEntry(DecoderInputEntry& newEntry);
	void getFirstEntry(DecoderInputEntry& destination);
	void clear();
	void destroy();
	std::mutex mutex;
	unsigned int entriesCount = 0;
	unsigned int entriesIndex = 0;
	std::vector<DecoderInputEntry> entries;
	HANDLE event = NULL;
	bool scrubbing = false;
	unsigned int scrubbingMatchCounter = 0;
};

extern DecoderInput decoderInput;
