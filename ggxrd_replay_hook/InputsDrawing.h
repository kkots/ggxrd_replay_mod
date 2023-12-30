#pragma once
#include "PngResource.h"
#include "TexturePacker.h"
#include "InputRingBufferStored.h"
#include <vector>
#include "InputsDrawingCommand.h"
class InputsDrawing
{
public:
	bool onDllMain(HMODULE hModule);
	void produceData(const InputRingBufferStored& inputRingBufferStored, InputsDrawingCommandRow* result, unsigned int* const resultSize);
	PngResource packedTexture;
private:
	PngResource pBtn;
	PngResource kBtn;
	PngResource sBtn;
	PngResource hBtn;
	PngResource dBtn;
	PngResource spBtn;
	PngResource tauntBtn;
	PngResource pBtnDark;
	PngResource kBtnDark;
	PngResource sBtnDark;
	PngResource hBtnDark;
	PngResource dBtnDark;
	PngResource spBtnDark;
	PngResource tauntBtnDark;
	PngResource upBtn;
	PngResource upBtnDark;
	PngResource upRightBtn;
	PngResource upRightBtnDark;
	PngResource rightBtn;
	PngResource rightBtnDark;
	PngResource downRightBtn;
	PngResource downRightBtnDark;
	PngResource downBtn;
	PngResource downBtnDark;
	PngResource downLeftBtn;
	PngResource downLeftBtnDark;
	PngResource leftBtn;
	PngResource leftBtnDark;
	PngResource upLeftBtn;
	PngResource upLeftBtnDark;
	TexturePacker texturePacker;
	inline void addToResult(InputsDrawingCommandRow& row, const PngResource& resource);
	bool addImage(HMODULE hModule, WORD resourceId, PngResource& resource);
	inline PngResource& getPunchBtn(bool isDark);
	inline PngResource& getKickBtn(bool isDark);
	inline PngResource& getSlashBtn(bool isDark);
	inline PngResource& getHeavySlashBtn(bool isDark);
	inline PngResource& getDustBtn(bool isDark);
	inline PngResource& getSpecialBtn(bool isDark);
	inline PngResource& getTauntBtn(bool isDark);
	void addDarkImage(PngResource& source, PngResource& destination);
	inline void produceDataFromInput(const Input& input, unsigned short framesHeld, const PngResource** const prevDirection,
			const Input** const prevInput, unsigned short* const prevFramesHeld, InputsDrawingCommandRow** const result, unsigned int* const resultSize);
};

extern InputsDrawing inputsDrawing;
