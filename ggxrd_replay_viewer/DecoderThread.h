#pragma once
#include "framework.h"
#include <atomic>
#include "SampleWithBuffer.h"
#include "DecoderInput.h"
#include "PresentationInput.h"

class DecoderThread
{
public:
	void threadLoop();
	HANDLE eventResponse = NULL;
	std::atomic_bool needSwitchFiles{ false };
	bool crashed = false;
private:
	SampleWithBuffer drainingSample;
	bool scrubbing = false;
	unsigned int scrubbingMatchCounter = 0;
	DecoderInputEntry entry;
	PresentationInputEntry newEntry;
};

