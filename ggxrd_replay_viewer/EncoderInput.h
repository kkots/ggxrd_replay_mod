#pragma once
#include "framework.h"
#include <mutex>
#include <vector>
#include "EncoderInputFrameRef.h"
#include "LogicInfoRef.h"

class EncoderInput
{
public:
	std::mutex mutex;
	HANDLE event = NULL;
	void addFrame(EncoderInputFrameRef& ref);
	void addLogic(LogicInfoRef& ref);
	void retrieveLogics(std::vector<LogicInfoRef>& destination);
	void retrieveFrames(EncoderInputFrameRef* destination, unsigned int& destinationFrameCount);
	void destroy();
	bool processExited = false;
private:
	EncoderInputFrameRef frames[8];
	unsigned char frameCount = 0;
	std::vector<LogicInfoRef> logics;
};

extern EncoderInput encoderInput;
