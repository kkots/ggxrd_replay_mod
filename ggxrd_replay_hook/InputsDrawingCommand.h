#pragma once
#include <utility>
struct InputsDrawingCommand {
	float uStart;
	float uEnd;
	float vStart;
	float vEnd;
	InputsDrawingCommand() = default;
	InputsDrawingCommand(const std::pair<std::pair<float, float>, std::pair<float, float>>& uvs);
	InputsDrawingCommand(float uStart, float uEnd, float vStart, float vEnd);
};
struct InputsDrawingCommandRow {
	unsigned char count = 0;
	InputsDrawingCommand cmds[8];
};