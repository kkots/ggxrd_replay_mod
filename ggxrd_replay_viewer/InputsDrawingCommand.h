#pragma once
#include <utility>
#include "ImageStack.h"
struct InputsDrawingCommand {
	bool isDark = false;
	const ImageStack* resource = nullptr;
	InputsDrawingCommand() = default;
	InputsDrawingCommand(const ImageStack* resource, bool isDark);
};
struct InputsDrawingCommandRow {
	unsigned int index = 0;
	unsigned int matchCounter = 0;
	unsigned int framesHeld = 0;
	unsigned int framesAfter = 0;
	unsigned char count = 0;
	unsigned int durationInRecording = 0;
	bool selected = false;
	InputsDrawingCommand cmds[8];
};