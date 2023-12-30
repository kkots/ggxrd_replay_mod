#pragma once
#include "framework.h"
#include "SampleWithBufferRef.h"
#include "LogicInfoRef.h"
#include <vector>
#include <mutex>

struct PresentationInputEntry {
	std::vector<SampleWithBufferRef> samples;
	std::vector<LogicInfoRef> logicInfo;
};

class PresentationInput
{
public:
	void addEntry(PresentationInputEntry& newEntry);
	void clear();
	void destroy();
	std::mutex mutex;
	unsigned int entriesCount = 0;
	std::vector<PresentationInputEntry> entries;
	HANDLE event = NULL;
	bool scrubbing = false;
	unsigned int scrubbingMatchCounter = 0;
	bool needReportScrubbing = false;
	bool needReportNotScrubbing = false;
	bool needRedrawFrame = false;
	bool needRedrawText = false;
};

extern PresentationInput presentationInput;
