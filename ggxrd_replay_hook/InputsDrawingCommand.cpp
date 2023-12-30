#include "pch.h"
#include "InputsDrawingCommand.h"
InputsDrawingCommand::InputsDrawingCommand(const std::pair<std::pair<float, float>, std::pair<float, float>>& uvs)
		: uStart(uvs.first.first), uEnd(uvs.second.first), vStart(uvs.first.second), vEnd(uvs.second.second) {
	
}

InputsDrawingCommand::InputsDrawingCommand(float uStart, float uEnd, float vStart, float vEnd)
		: uStart(uStart), uEnd(uEnd), vStart(vStart), vEnd(vEnd) {
}
