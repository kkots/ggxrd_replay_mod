#pragma once
#include "SampleWithBuffer.h"
#include "SampleWithBufferRef.h"
#include <vector>
#include "ImageYUVScaler.h"
#include "LogicInfoRef.h"
#include "EncoderInputFrameRef.h"

class EncoderThread
{
public:
	void threadLoop();
    void reset();
    bool crashed = false;
private:
    void clearFrames();
    std::vector<LogicInfoRef> logics;
    EncoderInputFrameRef frames[8];
    unsigned int frameCount = 0;
    std::vector<SampleWithBuffer> inputSamples{8};
    std::vector<SampleWithBufferRef> outputSamples;
    unsigned int inputSamplesCount = 0;
    unsigned int nonEmptyInputSamplesCount = 0;
    bool eofEncountered = false;
    unsigned int outputSamplesCount = 0;
    bool outputSamplesContainsEof = false;
    unsigned int ownMatchCounter = 0xFFFFFFFF;
    ImageYUVScaler scaler;
    unsigned int yuvSize = 0;
    SampleWithBuffer drainingSample;
};
