#include "pch.h"
#include "InputRingBufferStored.h"
#include "logging.h"

void InputRingBufferStored::resize(unsigned int newSize) {
	data.resize(newSize);
}

extern bool loggedDrawingInputsOnce;
void InputRingBufferStored::update(const InputRingBuffer& inputRingBuffer, const InputRingBuffer& prevInputRingBuffer) {
	
	unsigned short indexDiff = 0;
	const bool currentEmpty = inputRingBuffer.empty();
	const bool prevEmpty = prevInputRingBuffer.empty();
	if (!loggedDrawingInputsOnce) {
		logwrap(fprintf(logfile, "InputRingBufferStored::update: currentEmpty: %u, prevEmpty: %u\n", (unsigned int)currentEmpty, (unsigned int)prevEmpty));
	}
	if (currentEmpty) {
		if (!loggedDrawingInputsOnce) {
			logwrap(fputs("InputRingBufferStored::update: currentEmpty, exiting\n", logfile));
		}
		return;
	} else if (prevEmpty && !currentEmpty) {
		indexDiff = inputRingBuffer.index + 1;
		if (!loggedDrawingInputsOnce) {
			logwrap(fprintf(logfile, "InputRingBufferStored::update: prevEmpty && !currentEmpty. indexDiff: %u\n", indexDiff));
		}
	} else {
		if (inputRingBuffer.index < prevInputRingBuffer.index) {
			indexDiff = inputRingBuffer.index + 30 - prevInputRingBuffer.index;
		} else {
			indexDiff = inputRingBuffer.index - prevInputRingBuffer.index;
		}
		if (!loggedDrawingInputsOnce) {
			logwrap(fprintf(logfile, "InputRingBufferStored::update: walked into initial else branch. indexDiff: %u\n", indexDiff));
		}
	}
	isCleared = false;

	if (data[index].framesHeld == 0 && indexDiff > 0) {
		--indexDiff;
		if (!loggedDrawingInputsOnce) {
			logwrap(fprintf(logfile, "InputRingBufferStored::update: correcting indexDiff because data is empty. indexDiff: %u\n", indexDiff));
		}
	}
	index = (index + indexDiff) % data.size();
	if (!loggedDrawingInputsOnce) {
		logwrap(fprintf(logfile, "InputRingBufferStored::update: the new index value: %u\n", index));
	}
	unsigned int destinationIndex = index;

	unsigned short sourceIndex = inputRingBuffer.index;
	if (!loggedDrawingInputsOnce) {
		logwrap(fprintf(logfile, "InputRingBufferStored::update: iterating inputRingBuffer beginning from %u\n", sourceIndex));
	}
	for (int i = 30; i != 0; --i) {
		
		if (!loggedDrawingInputsOnce) {
			logwrap(fprintf(logfile, "InputRingBufferStored::update: inputRingBuffer index %u\n", sourceIndex));
		}

		const Input& input = inputRingBuffer.inputs[sourceIndex];
		const unsigned short framesHeld = inputRingBuffer.framesHeld[sourceIndex];

		if (!loggedDrawingInputsOnce) {
			logwrap(fprintf(logfile, "InputRingBufferStored::update: input: %.4X; framesHeld: %hu\n", input, framesHeld));
		}

		if (framesHeld == 0) {

			if (!loggedDrawingInputsOnce) {
				logwrap(fprintf(logfile, "InputRingBufferStored::update: issued a break because framesHeld 0\n"));
			}

			break;
		}

		if (!loggedDrawingInputsOnce) {
			logwrap(fprintf(logfile, "InputRingBufferStored::update: destinationIndex: %u\n", destinationIndex));
		}

		data[destinationIndex].framesHeld = framesHeld;
		data[destinationIndex].input = input;

		if (destinationIndex == 0) destinationIndex = data.size() - 1;
		else --destinationIndex;

		if (sourceIndex == 0) sourceIndex = 29;
		else --sourceIndex;
	}
}

void InputRingBufferStored::clear() {
	if (isCleared) return;
	isCleared = true;
	memset(&data.front(), 0, sizeof(InputFramesHeld) * data.size());
}
