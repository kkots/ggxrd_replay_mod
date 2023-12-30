#pragma once
#include "framework.h"
#include "SampleWithBufferRef.h"
#include "LogicInfoRef.h"
#include <vector>
#include <mutex>

struct FileWriterInputEntry {
	SampleWithBufferRef samples[8];
	unsigned int sampleCount = 0;
	std::vector<LogicInfoRef> logics;
};

class FileWriterInput
{
public:
	std::mutex mutex;
	void addInput(FileWriterInputEntry& input);
	void retrieve(std::vector<FileWriterInputEntry>& destination);
	void destroy();
	bool needNotifyEof = false;
	HANDLE event = NULL;
	bool processExited = false;
private:
	std::vector<FileWriterInputEntry> entries;
};

extern FileWriterInput fileWriterInput;
