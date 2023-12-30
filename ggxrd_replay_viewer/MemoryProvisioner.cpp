#include "MemoryProvisioner.h"
#include "Codec.h"

MemoryProvisioner memoryProvisioner;

EncoderInputFrameRef MemoryProvisioner::createEncoderInputFrame(unsigned int pitch, unsigned int width, unsigned int height) {
	std::unique_lock<std::mutex> guard(encoderInputFramesMutex);
	for (auto it = encoderInputFrames.begin(); it != encoderInputFrames.end(); ++it) {
		std::vector<EncoderInputFrame>& vec = *it;
		for (auto jt = vec.begin(); jt != vec.end(); ++jt) {
			EncoderInputFrame& encoderInputFrame = *jt;
			if (!encoderInputFrame.refCount) {
				encoderInputFrame.resize(pitch, width, height);
				return EncoderInputFrameRef{ encoderInputFrame };
			}
		}
	}
	encoderInputFrames.emplace_back(8);
	std::vector<EncoderInputFrame>& vec = encoderInputFrames.back();
	EncoderInputFrame& encoderInputFrame = vec.front();
	encoderInputFrame.resize(pitch, width, height);
	return EncoderInputFrameRef{ encoderInputFrame };
}

LogicInfoRef MemoryProvisioner::createLogicInfo() {
	std::unique_lock<std::mutex> guard(logicInfosMutex);
	for (auto it = logicInfos.begin(); it != logicInfos.end(); ++it) {
		std::vector<LogicInfoAllocated>& vec = *it;
		for (auto jt = vec.begin(); jt != vec.end(); ++jt) {
			if (!jt->refCount) {
				return LogicInfoRef{ *jt };
			}
		}
	}
	logicInfos.emplace_back(24);
	std::vector<LogicInfoAllocated>& vec = logicInfos.back();
	memset(&vec.front(), 0, sizeof(LogicInfoAllocated) * 24);
	return LogicInfoRef{ vec.front() };
}

SampleWithBufferRef MemoryProvisioner::createEncoderOutputSample() {
	std::unique_lock<std::mutex> guard(encoderOutputSamplesMutex);
	for (auto it = encoderOutputSamples.begin(); it != encoderOutputSamples.end(); ++it) {
		std::vector<SampleWithBuffer>& vec = *it;
		for (auto jt = vec.begin(); jt != vec.end(); ++jt) {
			SampleWithBuffer& swb = *jt;
			if (!swb.refCount) {
				swb.resize(codec.encoderOutputStreamInfo.cbAlignment, codec.encoderOutputStreamInfo.cbSize);
				return SampleWithBufferRef{ swb };
			}
		}
	}
	encoderOutputSamples.emplace_back(10);
	std::vector<SampleWithBuffer>& vec = encoderOutputSamples.back();
	SampleWithBuffer& swb = vec.front();
	swb.resize(codec.encoderOutputStreamInfo.cbAlignment, codec.encoderOutputStreamInfo.cbSize);
	return SampleWithBufferRef{ swb };
}

SampleWithBufferRef MemoryProvisioner::createDecoderInputSample() {
	std::unique_lock<std::mutex> guard(decoderInputSamplesMutex);
	for (auto it = decoderInputSamples.begin(); it != decoderInputSamples.end(); ++it) {
		std::vector<SampleWithBuffer>& vec = *it;
		for (auto jt = vec.begin(); jt != vec.end(); ++jt) {
			SampleWithBuffer& swb = *jt;
			if (!swb.refCount) {
				swb.resize(codec.encoderOutputStreamInfo.cbAlignment, codec.encoderOutputStreamInfo.cbSize);
				return SampleWithBufferRef{ swb };
			}
		}
	}
	decoderInputSamples.emplace_back(10);
	std::vector<SampleWithBuffer>& vec = decoderInputSamples.back();
	SampleWithBuffer& swb = vec.front();
	swb.resize(codec.encoderOutputStreamInfo.cbAlignment, codec.encoderOutputStreamInfo.cbSize);
	return SampleWithBufferRef{ swb };
}

SampleWithBufferRef MemoryProvisioner::createDecoderOutputSample() {
	std::unique_lock<std::mutex> guard(decoderOutputSamplesMutex);
	for (auto it = decoderOutputSamples.begin(); it != decoderOutputSamples.end(); ++it) {
		std::vector<SampleWithBuffer>& vec = *it;
		for (auto jt = vec.begin(); jt != vec.end(); ++jt) {
			SampleWithBuffer& swb = *jt;
			if (!swb.refCount) {
				swb.resize(codec.decoderOutputStreamInfo.cbAlignment, codec.decoderOutputStreamInfo.cbSize);
				return SampleWithBufferRef{ swb };
			}
		}
	}
	decoderOutputSamples.emplace_back(10);
	std::vector<SampleWithBuffer>& vec = decoderOutputSamples.back();
	SampleWithBuffer& swb = vec.front();
	swb.resize(codec.decoderOutputStreamInfo.cbAlignment, codec.decoderOutputStreamInfo.cbSize);
	return SampleWithBufferRef{ swb };
}

void MemoryProvisioner::destroy() {
	encoderInputFrames.clear();
	logicInfos.clear();
	encoderOutputSamples.clear();
	decoderInputSamples.clear();
	decoderOutputSamples.clear();
}
