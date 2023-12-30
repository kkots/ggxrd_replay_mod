#include "pch.h"
#include "InputsDrawing.h"
#include "PngResource.h"
#include "resource.h"
#include "Input.h"
#include "logging.h"

InputsDrawing inputsDrawing;

bool InputsDrawing::onDllMain(HMODULE hModule) {
    if (!addImage(hModule, IDB_PBTN, pBtn)) return false;
    if (!addImage(hModule, IDB_KBTN, kBtn)) return false;
    if (!addImage(hModule, IDB_SBTN, sBtn)) return false;
	if (!addImage(hModule, IDB_HBTN, hBtn)) return false;
	if (!addImage(hModule, IDB_DBTN, dBtn)) return false;
	if (!addImage(hModule, IDB_SPBTN, spBtn)) return false;
	if (!addImage(hModule, IDB_TAUNTBTN, tauntBtn)) return false;
	if (!addImage(hModule, IDB_UPBTN, upBtn)) return false;
	if (!addImage(hModule, IDB_UPRIGHTBTN, upRightBtn)) return false;
	if (!addImage(hModule, IDB_RIGHTBTN, rightBtn)) return false;
	if (!addImage(hModule, IDB_DOWNRIGHTBTN, downRightBtn)) return false;
	if (!addImage(hModule, IDB_DOWNBTN, downBtn)) return false;
	if (!addImage(hModule, IDB_DOWNLEFTBTN, downLeftBtn)) return false;
	if (!addImage(hModule, IDB_LEFTBTN, leftBtn)) return false;
	if (!addImage(hModule, IDB_UPLEFTBTN, upLeftBtn)) return false;
	addDarkImage(pBtn, pBtnDark);
	addDarkImage(kBtn, kBtnDark);
	addDarkImage(sBtn, sBtnDark);
	addDarkImage(hBtn, hBtnDark);
	addDarkImage(dBtn, dBtnDark);
	addDarkImage(spBtn, spBtnDark);
	addDarkImage(tauntBtn, tauntBtnDark);
	addDarkImage(upBtn, upBtnDark);
	addDarkImage(upRightBtn, upRightBtnDark);
	addDarkImage(rightBtn, rightBtnDark);
	addDarkImage(downRightBtn, downRightBtnDark);
	addDarkImage(downBtn, downBtnDark);
	addDarkImage(downLeftBtn, downLeftBtnDark);
	addDarkImage(leftBtn, leftBtnDark);
	addDarkImage(upLeftBtn, upLeftBtnDark);
    packedTexture = texturePacker.getTexture();
	if (packedTexture.data.empty() || !packedTexture.width || !packedTexture.height) return false;
	return true;
}

bool InputsDrawing::addImage(HMODULE hModule, WORD resourceId, PngResource& resource) {
    if (!loadPngResource(hModule, resourceId, resource)) return false;
    texturePacker.addImage(resource);
	return true;
}

void InputsDrawing::addDarkImage(PngResource& source, PngResource& destination) {
	destination.width = source.width;
	destination.height = source.height;
	destination.data = source.data;
	for (auto it = destination.data.begin(); it != destination.data.end(); ++it) {
		PngResource::PixelA pixel = *it;
		pixel.red >>= 1;
		pixel.green >>= 1;
		pixel.blue >>= 1;
		*it = pixel;
	}
	texturePacker.addImage(destination);
}

extern bool loggedDrawingInputsOnce;
void InputsDrawing::produceData(const InputRingBufferStored& inputRingBufferStored, InputsDrawingCommandRow* result, unsigned int* const resultSize) {
	const unsigned int dataSize = inputRingBufferStored.data.size();
	unsigned int index = (inputRingBufferStored.index + 1) % dataSize;
	const PngResource* prevDirection = nullptr;
	const Input* prevInput = nullptr;
	unsigned short prevFramesHeld = 0;
	if (!loggedDrawingInputsOnce) {
		logwrap(fprintf(logfile, "InputsDrawing::produceData: inputRingBufferStored.data.size(): %u\n", inputRingBufferStored.data.size()));
	}
	for (unsigned int i = dataSize; i != 0; --i) {
		const InputRingBufferStored::InputFramesHeld& inputStored = inputRingBufferStored.data[index];
		const Input& input = inputStored.input;
		const unsigned short framesHeld = inputStored.framesHeld;
		if (framesHeld != 0) {

			if (!loggedDrawingInputsOnce) {
				logwrap(fprintf(logfile, "InputsDrawing::produceData: index: %u, input: %.4X, framesHeld: %hu\n",
					index, input, framesHeld));
			}

			produceDataFromInput(input, framesHeld, &prevDirection, &prevInput, &prevFramesHeld, &result, resultSize);
		}

		++index;
		if (index == dataSize) index = 0;
	}
}

inline PngResource& InputsDrawing::getPunchBtn(bool isDark) {
	return isDark ? pBtnDark : pBtn;
}

inline PngResource& InputsDrawing::getKickBtn(bool isDark) {
	return isDark ? kBtnDark : kBtn;
}

inline PngResource& InputsDrawing::getSlashBtn(bool isDark) {
	return isDark ? sBtnDark : sBtn;
}

inline PngResource& InputsDrawing::getHeavySlashBtn(bool isDark) {
	return isDark ? hBtnDark : hBtn;
}

inline PngResource& InputsDrawing::getDustBtn(bool isDark) {
	return isDark ? dBtnDark : dBtn;
}

inline PngResource& InputsDrawing::getSpecialBtn(bool isDark) {
	return isDark ? spBtnDark : spBtn;
}

inline PngResource& InputsDrawing::getTauntBtn(bool isDark) {
	return isDark ? tauntBtnDark : tauntBtn;
}

inline void InputsDrawing::addToResult(InputsDrawingCommandRow& row, const PngResource& resource) {
	row.cmds[row.count++] = InputsDrawingCommand{ resource.uStart, resource.uEnd, resource.vStart, resource.vEnd };
}

inline void InputsDrawing::produceDataFromInput(const Input& input, unsigned short framesHeld, const PngResource** const prevDirection,
		const Input** const prevInput, unsigned short* const prevFramesHeld, InputsDrawingCommandRow** const result, unsigned int* const resultSize) {
	const PngResource* direction = nullptr;
	const PngResource* directionDark = nullptr;
	if (input.right) {
		if (input.up) {
			direction = &upRightBtn;
			directionDark = &upRightBtnDark;
		}
		else if (input.down) {
			direction = &downRightBtn;
			directionDark = &downRightBtnDark;
		}
		else {
			direction = &rightBtn;
			directionDark = &rightBtnDark;
		}
	}
	else if (input.left) {
		if (input.up) {
			direction = &upLeftBtn;
			directionDark = &upLeftBtnDark;
		}
		else if (input.down) {
			direction = &downLeftBtn;
			directionDark = &downLeftBtnDark;
		}
		else {
			direction = &leftBtn;
			directionDark = &leftBtnDark;
		}
	}
	else if (input.up) {
		direction = &upBtn;
		directionDark = &upBtnDark;
	}
	else if (input.down) {
		direction = &downBtn;
		directionDark = &downBtnDark;
	}

	if (!loggedDrawingInputsOnce) {
		if (direction == &upBtn) {
			logwrap(fprintf(logfile, "InputsDrawing::produceData: direction: up\n"));
		}
		else if (direction == &upRightBtn) {
			logwrap(fprintf(logfile, "InputsDrawing::produceData: direction: up right\n"));
		}
		else if (direction == &rightBtn) {
			logwrap(fprintf(logfile, "InputsDrawing::produceData: direction: right\n"));
		}
		else if (direction == &downRightBtn) {
			logwrap(fprintf(logfile, "InputsDrawing::produceData: direction: down right\n"));
		}
		else if (direction == &downBtn) {
			logwrap(fprintf(logfile, "InputsDrawing::produceData: direction: down\n"));
		}
		else if (direction == &downLeftBtn) {
			logwrap(fprintf(logfile, "InputsDrawing::produceData: direction: down left\n"));
		}
		else if (direction == &leftBtn) {
			logwrap(fprintf(logfile, "InputsDrawing::produceData: direction: left\n"));
		}
		else if (direction == &upLeftBtn) {
			logwrap(fprintf(logfile, "InputsDrawing::produceData: direction: up left\n"));
		}
		else {
			logwrap(fprintf(logfile, "InputsDrawing::produceData: direction: neutral\n"));
		}
	}

	const Input* prevInputInput = *prevInput;
	bool punchPressed = input.punch && (!prevInputInput || !prevInputInput->punch);
	bool kickPressed = input.kick && (!prevInputInput || !prevInputInput->kick);
	bool slashPressed = input.slash && (!prevInputInput || !prevInputInput->slash);
	bool heavySlashPressed = input.heavySlash && (!prevInputInput || !prevInputInput->heavySlash);
	bool dustPressed = input.dust && (!prevInputInput || !prevInputInput->dust);
	bool specialPressed = input.special && (!prevInputInput || !prevInputInput->special);
	bool tauntPressed = input.taunt && (!prevInputInput || !prevInputInput->taunt);

	if (!loggedDrawingInputsOnce) {
		logwrap(fprintf(logfile, "InputsDrawing::produceData: punchPressed: %u, kickPressed: %u, slashPressed: %u, heavySlashPressed: %u,"
			" dustPressed: %u, specialPressed: %u, tauntPressed: %u\n",
			punchPressed, kickPressed, slashPressed, heavySlashPressed, dustPressed, specialPressed, tauntPressed));
	}

	if (direction && direction != *prevDirection
		|| punchPressed
		|| kickPressed
		|| slashPressed
		|| heavySlashPressed
		|| dustPressed
		|| specialPressed
		|| tauntPressed) {
		if (*prevFramesHeld >= 60  // this number is chosen on a whim
			&& !*prevDirection
			&& !prevInputInput->punch
			&& !prevInputInput->kick
			&& !prevInputInput->slash
			&& !prevInputInput->heavySlash
			&& !prevInputInput->dust
			&& !prevInputInput->special
			&& !prevInputInput->taunt) {
			if (!loggedDrawingInputsOnce) {
				logwrap(fprintf(logfile, "InputsDrawing::produceData: adding an empty line\n"));
			}
			++(*result);
			++(*resultSize);
		}

		InputsDrawingCommandRow& row = **result;
		if (direction) addToResult(row, direction != *prevDirection ? *direction : *directionDark);
		if (input.punch) addToResult(row, getPunchBtn(!punchPressed));
		if (input.kick) addToResult(row, getKickBtn(!kickPressed));
		if (input.slash) addToResult(row, getSlashBtn(!slashPressed));
		if (input.heavySlash) addToResult(row, getHeavySlashBtn(!heavySlashPressed));
		if (input.dust) addToResult(row, getDustBtn(!dustPressed));
		if (input.special) addToResult(row, getSpecialBtn(!specialPressed));
		if (input.taunt) addToResult(row, getTauntBtn(!tauntPressed));
		++(*result);
		++(*resultSize);
	}

	*prevFramesHeld = framesHeld;
	*prevInput = &input;
	*prevDirection = direction;
}
