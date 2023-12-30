#pragma once
#include "framework.h"
#include "PngResource.h"
#include "TexturePacker.h"
#include "StoredInput.h"
#include <vector>
#include "InputsDrawingCommand.h"
#include "ImageStack.h"
#include <vector>
#include <mutex>

struct InputsPanel {
	int thisPanelIndex = 0;
	std::vector<InputsDrawingCommandRow> commands;
	std::vector<unsigned int> commandsIndex;
	unsigned int inputBufferIndex = 0;
	unsigned int currentCommandIndex = 0;
	unsigned int firstStartIndex = 0xFFFFFFFF;
	unsigned int lastEndIndex = 0;
	int positionOffset = 0;
	enum ScrollMode {
		SCROLL_MODE_ROW,
		SCROLL_MODE_FREE
	} scrollMode = SCROLL_MODE_ROW;
	unsigned int scrollRow = 0;
	bool isFirstDraw = true;
	unsigned int previouslyDrawnCommandIndex = 0xFFFFFFFF;
	unsigned int previouslyDrawnCommandIndexScrollRow = 0xFFFFFFFF;
	int previouslyDrawnCommandIndexPositionOffset = 0;
	bool seekbarControlledByPanel = false;
	void followToInputBufferIndex(unsigned int newInputBufferIndex);
	void followToInputBufferIndexCrossthread(unsigned int newInputBufferIndex);
	void scrollToCommandIndex(unsigned int newCommandIndex, int newPositionOffset, bool changeScrollMode);
	void fullPaint(const PAINTSTRUCT* ps, HDC hdc);
	RECT* panelRect = nullptr;
	RECT* panelRectData = nullptr;
	unsigned int buttonId = 0;
	bool isFollowingPlayback = true;
	void moveYourButton();
	void buttonClicked();
	void updateScrollbarInfo();
	void updateScrollbarPos(const RECT* rectData = nullptr);
	void setScrollInfo(SCROLLINFO* scrollInfo);
	HWND scrollbarHandle = NULL;
	unsigned int calculateHowManyFitOnScreen(const RECT* rectData, bool* isNonWhole = nullptr, unsigned int* remainingNonWhole = nullptr);
	void onScrollEvent(WORD scrollingRequest, WORD scrollPosition);
	unsigned int calculateNPos(unsigned int nPage, unsigned int scrollRowAsInput);
	void onMouseWheel(int wheelDelta);
	void onMouseDown(int mouseX, int mouseY, bool isShift);
	void onMouseMove(int mouseX, int mouseY);
	void onMouseUp();
	unsigned int selectionStart = 0xFFFFFFFF;
	unsigned int selectionEnd = 0xFFFFFFFF;  // inclusive
	bool isSelecting = false;
	bool shiftSelecting = false;
	bool isClickedLast = false;
	void deselectAll(bool redraw, bool* needRedraw = nullptr);
	void onSelectAll();
	void onCopy();
	char readChar();
	int readInt();
	unsigned int readUnsignedInt();
	float readFloat();
	UINT64 readUINT64();
	unsigned int calculateSelectedRow(int mouseY, const RECT* rectDataPtr);
	unsigned int calculateMatchCounter(int mouseY, const RECT* rectDataPtr);
	void onArrowUpDown(bool isUp, bool shiftHeld, unsigned int step = 1);
	void onPageUpDown(bool isUp, bool shiftHeld);
	void onHomeEnd(bool isHome, bool shiftHeld);
	bool allowedToManuallyScroll();
	void scrollIntoView(unsigned int rowIndex, const RECT* rectData, bool* needRedraw = nullptr);
};

class InputsDrawing
{
public:
	bool load(HDC hdc, HMODULE hModule);
	void produceData(const std::vector<StoredInput>& inputRingBufferStored, InputsDrawingCommandRow* result, unsigned int* indexTable,
	                 unsigned int* const resultSize, unsigned int* firstStartIndex, unsigned int* lastEndIndex, unsigned int startIndex, unsigned int endIndex);
	PngResource packedTexture;
	void resetFirstDraw();
	void onWmPaint(const PAINTSTRUCT& ps, HDC hdc);
	InputsPanel panels[2];
	std::mutex paintMutex;
	bool paintMutexLocked = false;;
	DWORD paintMutexThreadId = 0;
	void drawCommands(HDC hdc, std::vector<InputsDrawingCommandRow>& commands, int positionOffset, unsigned int startIndex, unsigned int endIndex, int rectLeft, int rectTop);
	void drawInputPointer(HDC hdc, int positionOffset, int rectLeft, int rectTop);
	bool aPanelIsSelecting() const;
	char mirrorDirection(char direction) const;
	bool seekbarControlledByPanel() const;
private:
	ImageStack pBtn;
	ImageStack kBtn;
	ImageStack sBtn;
	ImageStack hBtn;
	ImageStack dBtn;
	ImageStack spBtn;
	ImageStack tauntBtn;
	ImageStack upBtn;
	ImageStack upRightBtn;
	ImageStack rightBtn;
	ImageStack downRightBtn;
	ImageStack downBtn;
	ImageStack downLeftBtn;
	ImageStack leftBtn;
	ImageStack upLeftBtn;
	PngResource inputPointer;
	TexturePacker texturePacker;
	void setupButton(std::vector<ImageStack*>& vec, ImageStack& resource, WORD resourceId, const char* name);
	inline void addToResult(InputsDrawingCommandRow& row, const ImageStack& resource, bool isDark);
	bool addImage(HDC hdc, HMODULE hModule, WORD resourceId, PngResource& resource, const char* name);
	void addDarkImage(PngResource& source, PngResource& destination, const char* name);
	void addBlueBackgroundImage(PngResource& source, PngResource& destination, const char* name);
	HBRUSH blueBg = NULL;
};

extern InputsDrawing inputsDrawing;
