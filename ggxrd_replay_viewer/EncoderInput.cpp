#include "EncoderInput.h"
EncoderInput encoderInput;

void EncoderInput::addFrame(EncoderInputFrameRef& ref) {
	if (frameCount == 8) return;
	frames[frameCount++] = std::move(ref);
}

void EncoderInput::addLogic(LogicInfoRef& ref) {
	logics.emplace_back(std::move(ref));
}

void EncoderInput::retrieveLogics(std::vector<LogicInfoRef>& destination) {
	destination.insert(destination.end(), logics.begin(), logics.end());
	logics.clear();
}

void EncoderInput::retrieveFrames(EncoderInputFrameRef* destination, unsigned int& destinationFrameCount) {
	EncoderInputFrameRef* source = frames;
	destination += destinationFrameCount;
	while (destinationFrameCount < 8 && frameCount > 0) {
		*destination = std::move(*source);
		++source;
		--frameCount;
		++destination;
		++destinationFrameCount;
	}
	while (frameCount) {
		source->clear();
		--frameCount;
		++source;
	}
}

void EncoderInput::destroy() {
	while (frameCount) {
		--frameCount;
		frames[frameCount].clear();
	}
	logics.clear();
}
