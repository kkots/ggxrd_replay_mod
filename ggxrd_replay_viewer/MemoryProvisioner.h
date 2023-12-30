#pragma once
#include "EncoderInputFrame.h"
#include "EncoderInputFrameRef.h"
#include "LogicInfoAllocated.h"
#include "LogicInfoRef.h"
#include <list>
#include <vector>
#include <mutex>
#include "SampleWithBufferRef.h"
class MemoryProvisioner
{
public:
	EncoderInputFrameRef createEncoderInputFrame(unsigned int pitch, unsigned int width, unsigned int height);
	LogicInfoRef createLogicInfo();
	SampleWithBufferRef createEncoderOutputSample();
	SampleWithBufferRef createDecoderInputSample();
	SampleWithBufferRef createDecoderOutputSample();
	void destroy();
private:
	std::mutex encoderInputFramesMutex;
	std::list<std::vector<EncoderInputFrame>> encoderInputFrames;
	std::mutex logicInfosMutex;
	std::list<std::vector<LogicInfoAllocated>> logicInfos;
	std::mutex encoderOutputSamplesMutex;
	std::list<std::vector<SampleWithBuffer>> encoderOutputSamples;
	std::mutex decoderInputSamplesMutex;
	std::list<std::vector<SampleWithBuffer>> decoderInputSamples;
	std::mutex decoderOutputSamplesMutex;
	std::list<std::vector<SampleWithBuffer>> decoderOutputSamples;
};

extern MemoryProvisioner memoryProvisioner;