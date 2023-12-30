#include "InputsDrawing.h"
#include "PngResource.h"
#include "resource.h"
#include "Input.h"
#include "debugMsg.h"
#include "PixelA.h"
#include "InputIconSize.h"
#include "Layout.h"
#include "PngButtonManager.h"
#include "PresentationTimer.h"
#include "ReplayFileData.h"
#include <string>
#include <list>

extern HWND mainWindowHandle;
extern HDC hdcMem;
InputsDrawing inputsDrawing;
extern int scrollbarWidth;
extern PngButtonManager pngButtonManager;
extern HINSTANCE hInst;
extern debugStrStrType ctxDialogText;

extern INT_PTR CALLBACK CtxDialogMsgHandler(HWND, UINT, WPARAM, LPARAM);
extern void onSeekbarMousePress(unsigned int targetMatchCounter, bool pressedJustNow);
extern void onSeekbarMouseUp();

bool InputsDrawing::load(HDC hdc, HMODULE hModule) {
	std::vector<ImageStack*> allImgs;
	setupButton(allImgs, pBtn, IDB_PBTN, "punch");
	setupButton(allImgs, kBtn, IDB_KBTN, "kick");
	setupButton(allImgs, sBtn, IDB_SBTN, "slash");
	setupButton(allImgs, hBtn, IDB_HBTN, "heavySlash");
	setupButton(allImgs, dBtn, IDB_DBTN, "dust");
	setupButton(allImgs, spBtn, IDB_SPBTN, "special");
	setupButton(allImgs, tauntBtn, IDB_TAUNTBTN, "dust");
	setupButton(allImgs, upBtn, IDB_UPBTN, "up");
	setupButton(allImgs, upRightBtn, IDB_UPRIGHTBTN, "upRight");
	setupButton(allImgs, rightBtn, IDB_RIGHTBTN, "right");
	setupButton(allImgs, downRightBtn, IDB_DOWNRIGHTBTN, "downRight");
	setupButton(allImgs, downBtn, IDB_DOWNBTN, "down");
	setupButton(allImgs, downLeftBtn, IDB_DOWNLEFTBTN, "downLeft");
	setupButton(allImgs, leftBtn, IDB_LEFTBTN, "left");
	setupButton(allImgs, upLeftBtn, IDB_UPLEFTBTN, "upLeft");
	loadPngResource(hModule, IDB_INPUTPOINTER, inputPointer, hdc, "inputPointer", false);
	for (ImageStack* stackPtr : allImgs) {
		ImageStack& stack = *stackPtr;
		if (!addImage(hdc, hModule, stack.resourceId, stack.normal, stack.name.c_str())) return false;
		addDarkImage(stack.normal, stack.dark, stack.nameDark.c_str());
		addBlueBackgroundImage(stack.normal, stack.blue, stack.nameBlue.c_str());
		addBlueBackgroundImage(stack.dark, stack.darkBlue, stack.nameDarkBlue.c_str());
	}
	texturePacker.addImage(inputPointer);
	packedTexture = texturePacker.getTexture();
	packedTexture.createBitmap(hdc);
	blueBg = CreateSolidBrush(RGB(255 - ALPHA_OF_BLUE, 255 - ALPHA_OF_BLUE, 255));
	return true;
}

bool InputsDrawing::addImage(HDC hdc, HMODULE hModule, WORD resourceId, PngResource& resource, const char* name) {
	loadPngResource(hModule, resourceId, resource, hdc, name, false);
	texturePacker.addImage(resource);
	return true;
}

void InputsDrawing::addDarkImage(PngResource& source, PngResource& destination, const char* name) {
	destination.name = name;
	destination.width = source.width;
	destination.height = source.height;
	destination.data = source.data;
	for (auto it = destination.data.begin(); it != destination.data.end(); ++it) {
		PixelA pixel = *it;
		pixel.red >>= 1;
		pixel.green >>= 1;
		pixel.blue >>= 1;
		*it = pixel;
	}
	texturePacker.addImage(destination);
}

void InputsDrawing::addBlueBackgroundImage(PngResource& source, PngResource& destination, const char* name) {
	destination.name = name;
	destination.width = source.width;
	destination.height = source.height;
	destination.data = source.data;
	for (auto it = destination.data.begin(); it != destination.data.end(); ++it) {
		PixelA pixel = *it;
		unsigned int newAlpha = (255 * (unsigned int)pixel.alpha + 255 * (unsigned int)ALPHA_OF_BLUE - (unsigned int)ALPHA_OF_BLUE * (unsigned int)pixel.alpha) / 255;
		if (newAlpha != 0) {
			// the colors are actually BGR, but we'll keep using PixelRGB for this
			pixel.red = (unsigned char)((255 * (255 - (unsigned long long)pixel.alpha) * (unsigned long long)ALPHA_OF_BLUE + (unsigned long long)pixel.red * (unsigned long long)pixel.alpha * 255) / (255 * newAlpha));
			pixel.green = (unsigned char)((0 * (255 - (unsigned long long)pixel.alpha) * (unsigned long long)ALPHA_OF_BLUE + (unsigned long long)pixel.green * (unsigned long long)pixel.alpha * 255) / (255 * newAlpha));
			pixel.blue = (unsigned char)((0 * (255 - (unsigned long long)pixel.alpha) * (unsigned long long)ALPHA_OF_BLUE + (unsigned long long)pixel.blue * (unsigned long long)pixel.alpha * 255) / (255 * newAlpha));
			pixel.alpha = (unsigned char)newAlpha;
		}
		*it = pixel;
	}
	texturePacker.addImage(destination);
}

void InputsDrawing::produceData(const std::vector<StoredInput>& inputRingBufferStored, InputsDrawingCommandRow* result, unsigned int* indexTable,
	                            unsigned int* const resultSize, unsigned int* firstStartIndex, unsigned int* lastEndIndex, unsigned int startIndex, unsigned int endIndex) {
	const unsigned int dataSize = (unsigned int)inputRingBufferStored.size();
	
	InputsDrawingCommandRow* lastResult = nullptr;
	const ImageStack* prevDirection = nullptr;
	const StoredInput* prevInput = nullptr;
	const auto endIt = inputRingBufferStored.cbegin() + (endIndex - startIndex);
	unsigned int indexCounter = startIndex;
	unsigned int lastIndexValue = 0xFFFFFFFF;
	for (auto it = inputRingBufferStored.cbegin() + startIndex; it != endIt; ++it) {
		const StoredInput& inputStored = *it;
		const Input& input = (Input)inputStored;
		const unsigned short framesHeld = inputStored.framesHeld;
		const unsigned int framesHeldInRecording = inputStored.framesHeldInRecording;
		*indexTable = lastIndexValue;
		if (framesHeld != 0) {
			const ImageStack* direction = nullptr;
			if (input.right) {
				if (input.up) {
					direction = &upRightBtn;
				}
				else if (input.down) {
					direction = &downRightBtn;
				}
				else {
					direction = &rightBtn;
				}
			}
			else if (input.left) {
				if (input.up) {
					direction = &upLeftBtn;
				}
				else if (input.down) {
					direction = &downLeftBtn;
				}
				else {
					direction = &leftBtn;
				}
			}
			else if (input.up) {
				direction = &upBtn;
			}
			else if (input.down) {
				direction = &downBtn;
			}

			const Input* prevInputInput = prevInput;

			bool punchPressed;
			bool kickPressed;
			bool slashPressed;
			bool heavySlashPressed;
			bool dustPressed;
			bool specialPressed;
			bool tauntPressed;

			if (prevInputInput) {
				punchPressed =      input.punch      && !prevInputInput->punch;
				kickPressed =       input.kick       && !prevInputInput->kick;
				slashPressed =      input.slash      && !prevInputInput->slash;
				heavySlashPressed = input.heavySlash && !prevInputInput->heavySlash;
				dustPressed =       input.dust       && !prevInputInput->dust;
				specialPressed =    input.special    && !prevInputInput->special;
				tauntPressed =      input.taunt      && !prevInputInput->taunt;
			} else {
				punchPressed =      input.punch;
				kickPressed =       input.kick;
				slashPressed =      input.slash;
				heavySlashPressed = input.heavySlash;
				dustPressed =       input.dust;
				specialPressed =    input.special;
				tauntPressed =      input.taunt;
			}

			if (lastResult
					&& prevInput
					&& prevDirection ==              direction
					&& prevInputInput->punch ==      input.punch
					&& prevInputInput->kick ==       input.kick
					&& prevInputInput->slash ==      input.slash
					&& prevInputInput->heavySlash == input.heavySlash
					&& prevInputInput->dust ==       input.dust
					&& prevInputInput->special ==    input.special
					&& prevInputInput->taunt ==      input.taunt) {
				lastResult->framesHeld += framesHeld;
				if (lastResult->durationInRecording) {
					lastResult->durationInRecording += framesHeldInRecording;
				}

			} else if (lastResult
					&& lastResult->count
					&& framesHeld >= 60
					&& !direction
					&& !input.punch
					&& !input.kick
					&& !input.slash
					&& !input.heavySlash
					&& !input.dust
					&& !input.special
					&& !input.taunt) {
				if (*resultSize == 0xFFFFFFFE) return;
				*indexTable = *resultSize;
				*lastEndIndex = indexCounter;
				if (*firstStartIndex == 0xFFFFFFFF) *firstStartIndex = indexCounter;
				InputsDrawingCommandRow& row = *result;
				lastResult = result;
				row.index = indexCounter;
				row.framesHeld = inputStored.framesHeld;
				row.durationInRecording = inputStored.framesHeldInRecording;
				row.matchCounter = inputStored.matchCounter;
				++result;
				++(*resultSize);

			} else if (direction && direction != prevDirection
					|| punchPressed
					|| kickPressed
					|| slashPressed
					|| heavySlashPressed
					|| dustPressed
					|| specialPressed
					|| tauntPressed) {

				if (*resultSize == 0xFFFFFFFE) return;
				*indexTable = *resultSize;
				lastIndexValue = *resultSize;
				*lastEndIndex = indexCounter;
				if (*firstStartIndex == 0xFFFFFFFF) *firstStartIndex = indexCounter;
				InputsDrawingCommandRow& row = *result;
				lastResult = result;
				row.index = indexCounter;
				row.framesHeld = framesHeld;
				row.durationInRecording = inputStored.framesHeldInRecording;
				row.matchCounter = inputStored.matchCounter;
				if (direction) addToResult(row, *direction, direction == prevDirection);
				if (input.punch) addToResult(row, pBtn, !punchPressed);
				if (input.kick) addToResult(row, kBtn, !kickPressed);
				if (input.slash) addToResult(row, sBtn, !slashPressed);
				if (input.heavySlash) addToResult(row, hBtn, !heavySlashPressed);
				if (input.dust) addToResult(row, dBtn, !dustPressed);
				if (input.special) addToResult(row, spBtn, !specialPressed);
				if (input.taunt) addToResult(row, tauntBtn, !tauntPressed);
				++result;
				++(*resultSize);

			} else if (lastResult) {
				lastResult->framesAfter += framesHeld;
				if (lastResult->durationInRecording) {
					lastResult->durationInRecording += framesHeldInRecording;
				}

			}

			prevInput = &inputStored;
			prevDirection = direction;
		}
		++indexTable;
		++indexCounter;
	}
}

inline void InputsDrawing::addToResult(InputsDrawingCommandRow& row, const ImageStack& resource, bool isDark) {
	row.cmds[row.count++] = InputsDrawingCommand{ &resource, isDark };
}

void InputsDrawing::setupButton(std::vector<ImageStack*>& vec, ImageStack& resource, WORD resourceId, const char* name) {
	vec.push_back(&resource);
	resource.resourceId = resourceId;
	resource.name = name;
	resource.nameDark = resource.name + "Dark";
	resource.nameBlue = resource.name + "Blue";
	resource.nameDarkBlue = resource.name + "DarkBlue";
}

void InputsDrawing::resetFirstDraw() {
	for (int i = 0; i < 2; ++i) {
		panels[i].isFirstDraw = true;
		panels[i].selectionStart = 0xFFFFFFFF;
		panels[i].selectionEnd = 0xFFFFFFFF;
	}
}

void InputsPanel::followToInputBufferIndexCrossthread(unsigned int newInputBufferIndex) {
	PostMessage(mainWindowHandle, WM_APP, (WPARAM)newInputBufferIndex, (LPARAM)thisPanelIndex);
}

void InputsPanel::followToInputBufferIndex(unsigned int newInputBufferIndex) {
	if (!isFirstDraw && inputBufferIndex == newInputBufferIndex) return;
	inputBufferIndex = newInputBufferIndex;
	if (inputBufferIndex < 0 || inputBufferIndex >= commandsIndex.size()) {
		currentCommandIndex = 0xFFFFFFFF;
	} else {
		currentCommandIndex = commandsIndex[inputBufferIndex];
	}
	if (isFollowingPlayback && !seekbarControlledByPanel) {
		scrollToCommandIndex(currentCommandIndex, 0, true);
		updateScrollbarPos();
	} else {
		scrollToCommandIndex(scrollRow, positionOffset, false);
	}
}

void InputsPanel::scrollToCommandIndex(unsigned int newCommandIndex, int newPositionOffset, bool changeScrollMode) {
	if (!isFirstDraw && scrollRow == newCommandIndex && positionOffset == newPositionOffset && currentCommandIndex == previouslyDrawnCommandIndex) return;
	if (newCommandIndex != 0xFFFFFFFF && newCommandIndex >= commands.size()) {
		return;
	}
	int scrollRowDiff;
	if (newCommandIndex < scrollRow) {
		scrollRowDiff = -(int)(scrollRow - newCommandIndex);
	} else {
		scrollRowDiff = (int)(newCommandIndex - scrollRow);
	}
	int positionDiff = newPositionOffset - positionOffset;
	if (changeScrollMode || scrollMode == SCROLL_MODE_ROW) {
		scrollMode = SCROLL_MODE_ROW;
		scrollRow = newCommandIndex;
	} else if (scrollMode == SCROLL_MODE_FREE) {
		scrollRow = newCommandIndex;
	}
	positionOffset = newPositionOffset;
	HDC hdc = GetDC(mainWindowHandle);
	bool needFullRedraw = true;
	RECT updateRect;
	if (!isFirstDraw) {
		RECT rectDataFull;
		layout.getAnything(&rectDataFull, panelRectData);
		bool isNotWhole;
		int howManyFitOnScreen = (int)calculateHowManyFitOnScreen(&rectDataFull, &isNotWhole);
		int howManyFitInPage = howManyFitOnScreen - (isNotWhole ? 1 : 0);
		int totalPagePx = howManyFitInPage * (INPUT_ICON_SIZE + 3);
		int totalScrollPx = scrollRowDiff * (INPUT_ICON_SIZE + 3) + positionDiff;
		if (totalScrollPx < totalPagePx && totalScrollPx > -totalPagePx) {
			if (currentCommandIndex != previouslyDrawnCommandIndex || previouslyDrawnCommandIndex == 0xFFFFFFFF) {
				if (previouslyDrawnCommandIndex != 0xFFFFFFFF && previouslyDrawnCommandIndex <= previouslyDrawnCommandIndexScrollRow) {
					int arrowStartY = rectDataFull.top
							+ (previouslyDrawnCommandIndexScrollRow - previouslyDrawnCommandIndex) * (INPUT_ICON_SIZE + 3)
							+ previouslyDrawnCommandIndexPositionOffset + 3;

					int clippedHeight = 0;
					if (arrowStartY < rectDataFull.top) {
						clippedHeight = rectDataFull.top - arrowStartY;
						arrowStartY = rectDataFull.top;
					}
					if (clippedHeight < INPUT_ICON_SIZE) {
						RECT fillRect;
						fillRect.left = rectDataFull.left + 5;
						fillRect.right = rectDataFull.left + 5 + INPUT_POINTER_WIDTH;
						fillRect.top = arrowStartY;
						fillRect.bottom = arrowStartY + INPUT_ICON_SIZE - clippedHeight;
						FillRect(hdc, &fillRect, (HBRUSH)(COLOR_WINDOW + 1));
					}
				}
				if (currentCommandIndex <= scrollRow) {
					inputsDrawing.drawInputPointer(hdc, positionOffset + (scrollRow - currentCommandIndex) * (INPUT_ICON_SIZE + 3), rectDataFull.left, rectDataFull.top);
				}
				previouslyDrawnCommandIndexPositionOffset = positionOffset;
				previouslyDrawnCommandIndex = currentCommandIndex;
				previouslyDrawnCommandIndexScrollRow = scrollRow;
				rectDataFull.left += 5 + INPUT_POINTER_WIDTH + 3 - 1;
			}
			rectDataFull.right -= scrollbarWidth;
			if (totalScrollPx != 0) {
				ScrollDC(hdc, 0, totalScrollPx, &rectDataFull, &rectDataFull, NULL, &updateRect);
				PAINTSTRUCT artificialPs;
				artificialPs.rcPaint = updateRect;
				fullPaint(&artificialPs, hdc);
			}
			needFullRedraw = false;
		}
	}
	if (needFullRedraw) {
		fullPaint(nullptr, hdc);
	}
	ReleaseDC(mainWindowHandle, hdc);
}

void InputsDrawing::onWmPaint(const PAINTSTRUCT& ps, HDC hdc) {
	for (int i = 0; i < 2; ++i) {
		if (panels[i].isFirstDraw) continue;
		panels[i].fullPaint(&ps, hdc);
	}
}

void InputsPanel::fullPaint(const PAINTSTRUCT* ps, HDC hdc) {
	isFirstDraw = false;

	RECT rectDataFull;
	layout.getAnything(&rectDataFull, panelRectData);
	rectDataFull.right -= scrollbarWidth;
	RECT rectData = rectDataFull;
	unsigned int endIndex = scrollRow;
	if (ps) {
		if (ps->rcPaint.left >= rectData.right) {
			return;
		}
		if (ps->rcPaint.right <= rectData.left) {
			return;
		}
		if (ps->rcPaint.top >= rectData.bottom) {
			return;
		}
		if (ps->rcPaint.bottom <= rectData.top) {
			return;
		}
		if (ps->rcPaint.top > rectData.top) {
			rectData.top = ps->rcPaint.top;
		}
		if (ps->rcPaint.bottom < rectData.bottom) {
			rectData.bottom = ps->rcPaint.bottom;
		}
	}

	previouslyDrawnCommandIndexPositionOffset = positionOffset;
	previouslyDrawnCommandIndex = currentCommandIndex;
	previouslyDrawnCommandIndexScrollRow = scrollRow;

	FillRect(hdc, &rectData, (HBRUSH)(COLOR_WINDOW + 1));

	unsigned int yDiff = rectData.top - rectDataFull.top;
	unsigned int fullRowsSkipped = yDiff / (INPUT_ICON_SIZE + 3);
	yDiff -= fullRowsSkipped * (INPUT_ICON_SIZE + 3);
	int positionOffsetDraw = (scrollMode == SCROLL_MODE_ROW ? 0 : positionOffset) - yDiff;
	unsigned int fullRowsSkippedExtra = (-positionOffsetDraw) / (INPUT_ICON_SIZE + 3);
	positionOffsetDraw += fullRowsSkippedExtra * (INPUT_ICON_SIZE + 3);
	fullRowsSkipped += fullRowsSkippedExtra;

	if (endIndex < fullRowsSkipped) {
		endIndex = 0xFFFFFFFF;
	} else {
		endIndex -= fullRowsSkipped;
	}

	unsigned int howManyFitOnScreen = calculateHowManyFitOnScreen(&rectData);
	unsigned int startIndex = 0;
	if (endIndex == 0xFFFFFFFF) {
		return;
	}
	if (positionOffsetDraw != 0) {
		++howManyFitOnScreen;
	}

	if (endIndex >= (howManyFitOnScreen - 1)) {
		startIndex = endIndex - (howManyFitOnScreen - 1);
	}
	++endIndex;
	inputsDrawing.drawCommands(hdc, commands, positionOffsetDraw, startIndex, endIndex, rectData.left, rectData.top);
	if (currentCommandIndex != 0xFFFFFFFF && scrollRow != 0xFFFFFFFF) {
		if (currentCommandIndex < endIndex && currentCommandIndex >= startIndex) {
			inputsDrawing.drawInputPointer(hdc, positionOffsetDraw + (endIndex - 1 - currentCommandIndex) * (INPUT_ICON_SIZE + 3), rectData.left, rectData.top);
		}
	}
}

void InputsDrawing::drawCommands(HDC hdc, std::vector<InputsDrawingCommandRow>&commands, int positionOffset,
                                 unsigned int startIndex, unsigned int endIndex, int rectLeft, int rectTop) {
	if (commands.empty() || endIndex > commands.size()) return;
	HGDIOBJ oldObject = SelectObject(hdcMem, packedTexture.hBitmap);
	unsigned int y = rectTop + positionOffset + 3;
	bool initiallyClipped = (positionOffset < -3);
	unsigned int clippedHeight = -positionOffset - 3;
	if (clippedHeight > INPUT_ICON_SIZE) {
		clippedHeight = INPUT_ICON_SIZE;
	}
	auto it = commands.begin() + endIndex;
	auto endIt = commands.begin() + startIndex;
	unsigned int startX = 5 + INPUT_POINTER_WIDTH + 3 + rectLeft;
	RECT fillRect;
	fillRect.left = startX - 1;
	fillRect.right = startX + 8 * INPUT_ICON_SIZE + 7 * 3 + 5;
	while (it != endIt) {
		--it;
		InputsDrawingCommandRow& row = *it;
		unsigned int x = startX;
		if (row.selected) {
			fillRect.top = y - 3 + 1;
			fillRect.bottom = fillRect.top + INPUT_ICON_SIZE + 3;
			if (fillRect.top < rectTop) fillRect.top = rectTop;
			FillRect(hdc, &fillRect, blueBg);
		}
		InputsDrawingCommand* cmd = row.cmds;
		unsigned char cmdCount = row.count;
		for (unsigned char cmdCounter = 0; cmdCounter < cmdCount; ++cmdCounter) {
			const PngResource* resource;
			if (cmd->isDark) {
				if (row.selected) {
					resource = &cmd->resource->darkBlue;
				} else {
					resource = &cmd->resource->dark;
				}
			} else {
				if (row.selected) {
					resource = &cmd->resource->blue;
				} else {
					resource = &cmd->resource->normal;
				}
			}
			if (!initiallyClipped) {
				BitBlt(hdc, x, y, INPUT_ICON_SIZE, INPUT_ICON_SIZE, hdcMem, resource->startX, resource->startY, SRCCOPY);
			} else {
				if (clippedHeight < INPUT_ICON_SIZE) {
					BitBlt(hdc, x, y + clippedHeight, INPUT_ICON_SIZE, INPUT_ICON_SIZE - clippedHeight, hdcMem, resource->startX, resource->startY + clippedHeight, SRCCOPY);
				}
			}
			++cmd;
			x += INPUT_ICON_SIZE + 3;
		}
		initiallyClipped = false;
		y += INPUT_ICON_SIZE + 3;
	}
	SelectObject(hdcMem, oldObject);
}

void InputsDrawing::drawInputPointer(HDC hdc, int positionOffset, int rectLeft, int rectTop) {
	HGDIOBJ oldObject = SelectObject(hdcMem, packedTexture.hBitmap);
	unsigned int y = rectTop + positionOffset + 3;
	bool clipped = (positionOffset < -3);
	unsigned int clippedHeight = -positionOffset - 3;
	if (clippedHeight > INPUT_ICON_SIZE) {
		clippedHeight = INPUT_ICON_SIZE;
	}
	unsigned int x = rectLeft + 5;
	if (!clipped) {
		BitBlt(hdc, x, y, INPUT_POINTER_WIDTH, INPUT_ICON_SIZE, hdcMem, inputPointer.startX, inputPointer.startY, SRCCOPY);
	} else if (clippedHeight < INPUT_ICON_SIZE) {
		BitBlt(hdc, x, y + clippedHeight, INPUT_POINTER_WIDTH, INPUT_ICON_SIZE - clippedHeight, hdcMem, inputPointer.startX, inputPointer.startY + clippedHeight, SRCCOPY);
	}
	SelectObject(hdcMem, oldObject);
}

void InputsPanel::moveYourButton() {
	if (!panelRect) return;
	RECT panelRectLocal;
	layout.getAnything(&panelRectLocal, panelRect);
	pngButtonManager.moveButton(buttonId, panelRectLocal.left + 5, 5);
}

void InputsPanel::buttonClicked() {	
	isFollowingPlayback = !isFollowingPlayback;
	pngButtonManager.setResourceIndex(buttonId, isFollowingPlayback ? 1 : 0);
	if (isFollowingPlayback) {
		scrollMode = SCROLL_MODE_ROW;
		scrollRow = currentCommandIndex;
		positionOffset = 0;
		updateScrollbarPos();
		if (isFirstDraw || true) {
			HDC hdc = GetDC(mainWindowHandle);
			fullPaint(nullptr, hdc);
			ReleaseDC(mainWindowHandle, hdc);
		}
	}
}

void InputsPanel::updateScrollbarInfo() {
	RECT rectData;
	layout.getAnything(&rectData, panelRectData);

	if (commands.empty()) {
		SCROLLINFO scrollInfo;
		scrollInfo.cbSize = sizeof(SCROLLINFO);
		scrollInfo.fMask = SIF_DISABLENOSCROLL;
		setScrollInfo(&scrollInfo);
		return;
	}

	SCROLLINFO scrollInfo;
	scrollInfo.cbSize = sizeof(SCROLLINFO);
	scrollInfo.fMask = SIF_PAGE | SIF_POS | SIF_RANGE;
	scrollInfo.nMin = 0;
	scrollInfo.nMax = 0xFFFF;
	bool isNonWhole;
	unsigned int howManyFitOnScreen = calculateHowManyFitOnScreen(&rectData, &isNonWhole);
	unsigned int howManyFitInPage = howManyFitOnScreen - (isNonWhole ? 1 : 0);
	scrollInfo.nPage = howManyFitInPage * 0x10000 / (howManyFitOnScreen + (unsigned int)commands.size());
	if (scrollInfo.nPage < 1) {
		scrollInfo.nPage = 1;
	}
	scrollInfo.nPos = calculateNPos(scrollInfo.nPage, scrollRow);
	setScrollInfo(&scrollInfo);
}

void InputsPanel::updateScrollbarPos(const RECT* rectData) {

	if (commands.empty()) {
		return;
	}

	RECT rectDataLocal;
	if (!rectData) {
		layout.getAnything(&rectDataLocal, panelRectData);
		rectData = &rectDataLocal;
	}

	SCROLLINFO scrollInfo;
	scrollInfo.cbSize = sizeof(SCROLLINFO);
	scrollInfo.fMask = SIF_POS;
	unsigned int nMax = 0xFFFF;
	bool isNonWhole;
	unsigned int howManyFitOnScreen = calculateHowManyFitOnScreen(rectData, &isNonWhole);
	unsigned int howManyFitInPage = howManyFitOnScreen - (isNonWhole ? 1 : 0);
	unsigned int nPage = (unsigned int)((unsigned long long)howManyFitInPage * 0x10000 / ((unsigned long long)howManyFitOnScreen + commands.size()));
	if (nPage < 1) {
		nPage = 1;
	}
	scrollInfo.nPos = calculateNPos(nPage, scrollRow);
	setScrollInfo(&scrollInfo);
}

unsigned int InputsPanel::calculateNPos(unsigned int nPage, unsigned int scrollRowAsInput) {
	const unsigned int actualNMax = 0xFFFF - nPage + 1;
	if (scrollMode == SCROLL_MODE_ROW) {
		if (scrollRowAsInput == 0xFFFFFFFF) {
			return actualNMax;
		}
		return (unsigned int)(((unsigned long long)commands.size() - (unsigned long long)(scrollRowAsInput + 1)) * (unsigned long long)actualNMax / (unsigned long long)commands.size());
	}
	else if (scrollMode == SCROLL_MODE_FREE) {
		if (scrollRowAsInput == 0xFFFFFFFF) {
			return actualNMax;
		}
		if (scrollRowAsInput == 0) {
			unsigned int posBase = (unsigned int)((unsigned long long)(commands.size() - 1) * (unsigned long long)actualNMax / (unsigned long long)commands.size());
			return posBase + (actualNMax - posBase) * (-positionOffset) / (INPUT_ICON_SIZE + 3);
		}
		unsigned int pos1 = (unsigned int)(((unsigned long long)commands.size() - ((unsigned long long)scrollRowAsInput + 1)) * (unsigned long long)actualNMax / (unsigned long long)commands.size());
		unsigned int pos2 = (unsigned int)(((unsigned long long)commands.size() - (unsigned long long)scrollRowAsInput) * (unsigned long long)actualNMax / (unsigned long long)commands.size());
		unsigned int posOneLine = pos2 - pos1;
		return pos1 + posOneLine * (-positionOffset) / (INPUT_ICON_SIZE + 3);
	}
	return 0;
}

unsigned int InputsPanel::calculateHowManyFitOnScreen(const RECT* rectData, bool* isNonWhole, unsigned int* remainingNonWhole) {
	if (isNonWhole) *isNonWhole = false;
	if (remainingNonWhole) *remainingNonWhole = 0;
	unsigned int result = (rectData->bottom - rectData->top - 3);
	if (rectData->bottom - rectData->top < 3) return 0;
	const unsigned int remainder = result % (INPUT_ICON_SIZE + 3);
	result /= (INPUT_ICON_SIZE + 3);
	if (remainder) {
		++result;
		if (remainingNonWhole) *remainingNonWhole = remainder;
		if (isNonWhole) *isNonWhole = true;
	}
	return result;
}

void InputsPanel::setScrollInfo(SCROLLINFO* scrollInfo) {
	SetScrollInfo(scrollbarHandle, SB_CTL, scrollInfo, true);
}

void InputsPanel::onScrollEvent(WORD scrollingRequest, WORD scrollPosition) {
	bool isPlaying = false;
	bool isAtEnd = false;
	{
		std::unique_lock<std::mutex> presentationGuard(presentationTimer.mutex);
		isPlaying = presentationTimer.isPlaying;
		isAtEnd = (presentationTimer.matchCounter == 0xFFFFFFFF - (replayFileData.header.logicsCount - 1));
	}
	if (isFollowingPlayback && isPlaying && !isAtEnd || commands.empty()) return;
	RECT rectData;
	layout.getAnything(&rectData, panelRectData);
	unsigned int newScrollRow = scrollRow;
	switch (scrollingRequest) {
		case SB_BOTTOM: {
			scrollToCommandIndex(0xFFFFFFFF, 0, true);
			updateScrollbarPos(&rectData);
		}
		break;
		case SB_TOP: {
			scrollToCommandIndex((unsigned int)(commands.size() - 1), 0, true);
			updateScrollbarPos(&rectData);
		}
		break;
		case SB_LINEDOWN: {
			if (scrollMode == SCROLL_MODE_ROW) {
				if (newScrollRow == 0xFFFFFFFF) return;
				if (newScrollRow == 0) {
					newScrollRow = 0xFFFFFFFF;
				} else {
					--newScrollRow;
				}
				scrollToCommandIndex(newScrollRow, 0, true);
				updateScrollbarPos(&rectData);
			} else if (scrollMode == SCROLL_MODE_FREE) {
				if (newScrollRow == 0xFFFFFFFF) return;
				unsigned int newPositionOffset = positionOffset;
				if (newScrollRow == 0) {
					newScrollRow = 0xFFFFFFFF;
					newPositionOffset = 0;
				} else {
					--newScrollRow;
				}
				scrollToCommandIndex(newScrollRow, newPositionOffset, false);
				updateScrollbarPos(&rectData);
			}
		}
		break;
		case SB_LINEUP: {
			if (scrollMode == SCROLL_MODE_ROW) {
				if (newScrollRow == 0xFFFFFFFF) {
					newScrollRow = 0;
				} else {
					if (newScrollRow >= commands.size() - 1) {
						if (positionOffset != 0) {
							scrollToCommandIndex(newScrollRow, 0, false);
							updateScrollbarPos(&rectData);
						}
						return;
					}
					++newScrollRow;
				}
				scrollToCommandIndex(newScrollRow, 0, true);
				updateScrollbarPos(&rectData);
			}
			else if (scrollMode == SCROLL_MODE_FREE) {
				if (newScrollRow == 0xFFFFFFFF) {
					newScrollRow = 0;
				} else if (newScrollRow >= commands.size() - 1) {
					if (positionOffset != 0) {
						scrollToCommandIndex((unsigned int)(commands.size() - 1), 0, false);
						updateScrollbarPos(&rectData);
					}
				} else {
					++newScrollRow;
					scrollToCommandIndex(newScrollRow, positionOffset, false);
					updateScrollbarPos(&rectData);
				}
			}
		}
		break;
		case SB_PAGEDOWN: {
			bool isNonWhole;
			unsigned int howManyFitOnScreen = calculateHowManyFitOnScreen(&rectData, &isNonWhole);
			unsigned int howManyFitInPage = howManyFitOnScreen - (isNonWhole ? 1 : 0);
			if (scrollMode == SCROLL_MODE_ROW) {
				if (newScrollRow == 0xFFFFFFFF) return;
				if (newScrollRow < howManyFitInPage) {
					newScrollRow = 0xFFFFFFFF;
				} else {
					newScrollRow -= howManyFitInPage;
				}
				scrollToCommandIndex(newScrollRow, 0, true);
				updateScrollbarPos(&rectData);
			}
			else if (scrollMode == SCROLL_MODE_FREE) {
				if (newScrollRow == 0xFFFFFFFF) return;
				unsigned int newPositionOffset = positionOffset;
				if (newScrollRow < howManyFitInPage) {
					newScrollRow = 0xFFFFFFFF;
					newPositionOffset = 0;
				} else {
					newScrollRow -= howManyFitInPage;
				}
				scrollToCommandIndex(newScrollRow, newPositionOffset, false);
				updateScrollbarPos(&rectData);
			}
		}
		break;
		case SB_PAGEUP: {
			bool isNonWhole;
			unsigned int howManyFitOnScreen = calculateHowManyFitOnScreen(&rectData, &isNonWhole);
			unsigned int howManyFitInPage = howManyFitOnScreen - (isNonWhole ? 1 : 0);
			if (scrollMode == SCROLL_MODE_ROW) {
				if (newScrollRow == 0xFFFFFFFF) {
					newScrollRow = howManyFitInPage - 1;
					if (newScrollRow >= commands.size()) {
						newScrollRow = (unsigned int)(commands.size() - 1);
					}
					scrollToCommandIndex(newScrollRow, 0, true);
					updateScrollbarPos(&rectData);
				} else {
					if (newScrollRow >= commands.size() - 1) return;
					unsigned int newPositionOffset = positionOffset;
					if (commands.size() - 1 - newScrollRow < howManyFitInPage) {
						newScrollRow = (unsigned int)(commands.size() - 1);
						newPositionOffset = 0;
					}
					else {
						newScrollRow += howManyFitInPage;
					}
					scrollToCommandIndex(newScrollRow, newPositionOffset, true);
					updateScrollbarPos(&rectData);
				}
			}
			else if (scrollMode == SCROLL_MODE_FREE) {
				if (newScrollRow == 0xFFFFFFFF) {
					newScrollRow = howManyFitInPage - 1;
					if (newScrollRow >= commands.size()) {
						newScrollRow = (unsigned int)(commands.size() - 1);
					}
					scrollToCommandIndex(newScrollRow, 0, false);
					updateScrollbarPos(&rectData);
				}
				else if (newScrollRow >= commands.size() - 1) {
					if (positionOffset != 0) {
						scrollToCommandIndex(newScrollRow, 0, false);
						updateScrollbarPos(&rectData);
					}
				} else {
					unsigned int newPositionOffset = positionOffset;
					if (commands.size() - 1 - newScrollRow < howManyFitInPage) {
						newScrollRow = (unsigned int)(commands.size() - 1);
						newPositionOffset = 0;
					}
					else {
						newScrollRow += howManyFitInPage;
					}
					scrollToCommandIndex(newScrollRow, newPositionOffset, false);
					updateScrollbarPos(&rectData);
				}
			}
		}
		break;
		case SB_THUMBPOSITION:
		case SB_THUMBTRACK: {
			unsigned int nMax = 0xFFFF;
			bool isNonWhole;
			unsigned int howManyFitOnScreen = calculateHowManyFitOnScreen(&rectData, &isNonWhole);
			unsigned int howManyFitInPage = howManyFitOnScreen - (isNonWhole ? 1 : 0);
			unsigned int nPage = (unsigned int)((unsigned long long)howManyFitInPage * 0x10000 / ((unsigned long long)howManyFitOnScreen + commands.size()));
			if (nPage < 1) {
				nPage = 1;
			}
			scrollMode = SCROLL_MODE_FREE;
			unsigned int actualNMax = nMax - nPage + 1;

			if (scrollPosition == actualNMax) {
				scrollToCommandIndex(0xFFFFFFFF, 0, false);
				if (scrollingRequest == SB_THUMBPOSITION) updateScrollbarPos(&rectData);
				return;
			}
			unsigned int posPenult = (unsigned int)((commands.size() - 1) * (unsigned long long)actualNMax / commands.size());
			if (scrollPosition >= posPenult) {
				if (actualNMax == posPenult) {
					scrollToCommandIndex(0, 0, false);
				} else {
					scrollToCommandIndex(0, -(int)((unsigned long long)(scrollPosition - posPenult) * ((unsigned long long)INPUT_ICON_SIZE + 3) / (unsigned long long)(actualNMax - posPenult)), false);
				}
				if (scrollingRequest == SB_THUMBPOSITION) updateScrollbarPos(&rectData);
				return;
			}

			if (actualNMax) {
				newScrollRow = (unsigned int)((unsigned long long)scrollPosition * (unsigned long long)commands.size() / (unsigned long long)actualNMax);
			} else {
				newScrollRow = 0;
			}
			newScrollRow = (unsigned int)(commands.size() - 1) - newScrollRow;
			unsigned int nPos;
			unsigned int nPos2;
			unsigned int oldPositionOffset;

			oldPositionOffset = positionOffset;
			positionOffset = 0;
			nPos = calculateNPos(nPage, newScrollRow);

			if (newScrollRow == 0) {
				newScrollRow = 0xFFFFFFFF;
				nPos2 = calculateNPos(nPage, newScrollRow);
				newScrollRow = 0;
			} else {
				--newScrollRow;
				nPos2 = calculateNPos(nPage, newScrollRow);
				++newScrollRow;
			}

			positionOffset = oldPositionOffset;

			unsigned int nPosDiff = nPos2 - nPos;

			unsigned int newPositionOffset = 0;
			if (nPosDiff) {
				newPositionOffset = -(int)((unsigned long long)(scrollPosition - nPos) * ((unsigned long long)INPUT_ICON_SIZE + 3) / (unsigned long long)nPosDiff);
			}
			scrollToCommandIndex(newScrollRow, newPositionOffset, false);
			if (scrollingRequest == SB_THUMBPOSITION) updateScrollbarPos(&rectData);
		}
		break;
	}
}

void InputsPanel::onMouseWheel(int wheelDelta) {
	wheelDelta /= 40;
	if (wheelDelta == 0) return;
	if (!allowedToManuallyScroll()) return;
	RECT rectData;
	layout.getAnything(&rectData, panelRectData);
	unsigned int newScrollRow = scrollRow;
	if (wheelDelta < 0) {
		if (scrollMode == SCROLL_MODE_ROW) {
			if (newScrollRow == 0xFFFFFFFF) return;
			if (newScrollRow < (unsigned int)(-wheelDelta)) {
				newScrollRow = 0xFFFFFFFF;
			}
			else {
				newScrollRow += wheelDelta;
			}
			scrollToCommandIndex(newScrollRow, 0, true);
			updateScrollbarPos(&rectData);
		}
		else if (scrollMode == SCROLL_MODE_FREE) {
			if (newScrollRow == 0xFFFFFFFF) return;
			unsigned int newPositionOffset = positionOffset;
			if (newScrollRow < (unsigned int)(-wheelDelta)) {
				newScrollRow = 0xFFFFFFFF;
				newPositionOffset = 0;
			}
			else {
				newScrollRow += wheelDelta;
			}
			scrollToCommandIndex(newScrollRow, newPositionOffset, false);
			updateScrollbarPos(&rectData);
		}
	} else {
		int newPositionOffset = positionOffset;
		if (newScrollRow == 0xFFFFFFFF) {
			newScrollRow = wheelDelta - 1;
		}
		else {
			newScrollRow += wheelDelta;
		}

		if (newScrollRow > commands.size() - 1) {
			newScrollRow = (unsigned int)(commands.size() - 1);
			newPositionOffset = 0;
		}
		if (scrollRow == newScrollRow && positionOffset == newPositionOffset) return;
		scrollToCommandIndex(newScrollRow, newPositionOffset, false);
		updateScrollbarPos(&rectData);
	}
}

void InputsPanel::onMouseDown(int mouseX, int mouseY, bool isShift) {
	isSelecting = true;

	RECT rectData;
	layout.getAnything(&rectData, panelRectData);

	if (mouseY < rectData.top || mouseY >= rectData.bottom) return;

	if (mouseX < (int)(rectData.left + 5 + INPUT_POINTER_WIDTH + 3 - 1)) {
		unsigned int targetMatchCounter = calculateMatchCounter(mouseY, &rectData);
		seekbarControlledByPanel = true;
		onSeekbarMousePress(targetMatchCounter, true);
		return;
	}

	class Exiter {
	public:
		bool needRedraw = false;
		InputsPanel* panel = nullptr;
		~Exiter() {
			if (needRedraw) {
				panel->isFirstDraw = true;
				panel->scrollToCommandIndex(panel->scrollRow, panel->positionOffset, false);
			}
		}
	} exiter;
	exiter.panel = this;

	shiftSelecting = isShift;
	if (shiftSelecting) {
		if (selectionStart != 0xFFFFFFFF) {
			onMouseMove(mouseX, mouseY);
			return;
		}
	}
	deselectAll(false, &exiter.needRedraw);
	if (scrollRow == 0xFFFFFFFF || commands.empty()) {
		return;
	}

		
	selectionStart = calculateSelectedRow(mouseY, &rectData);
	if (selectionStart == 0xFFFFFFFF) return;
	commands[selectionStart].selected = true;
	selectionEnd = selectionStart;
	exiter.needRedraw = true;
}

unsigned int InputsPanel::calculateSelectedRow(int mouseY, const RECT* rectDataPtr) {
	unsigned int mouseInRect = mouseY - rectDataPtr->top - positionOffset;
	unsigned int selectedRow = mouseInRect / (INPUT_ICON_SIZE + 3);
	if (selectedRow > scrollRow) {
		return 0xFFFFFFFF;
	}
	selectedRow = scrollRow - selectedRow;
	if (selectedRow >= commands.size()) return 0xFFFFFFFF;
	return selectedRow;
}

unsigned int InputsPanel::calculateMatchCounter(int mouseY, const RECT* rectDataPtr) {
	unsigned int mouseInRect = mouseY - rectDataPtr->top - positionOffset;
	unsigned int selectedRowScreen = mouseInRect / (INPUT_ICON_SIZE + 3);
	if (selectedRowScreen > scrollRow) {
		return 0xFFFFFFFF;
	}
	unsigned int selectedRow = scrollRow - selectedRowScreen;
	if (selectedRow >= commands.size()) return 0xFFFFFFFF;

	InputsDrawingCommandRow& row = commands[selectedRow];
	if (row.durationInRecording <= 1) {
		return row.matchCounter;
	}
	unsigned int firstMatchCounter = row.matchCounter;
	unsigned int lastMatchCounter;
	if (row.matchCounter < row.durationInRecording - 1) {
		lastMatchCounter = 0;
	} else {
		lastMatchCounter = row.matchCounter - (row.durationInRecording - 1);
	}
	if (lastMatchCounter < 0xFFFFFFFF - (unsigned int)(replayFileData.index.size() - 1)) {
		lastMatchCounter = 0xFFFFFFFF - (unsigned int)(replayFileData.index.size() - 1);
	}
	int rowBottomY = rectDataPtr->top + positionOffset + (selectedRowScreen + 1) * (INPUT_ICON_SIZE + 3);
	return (unsigned int)(
		(unsigned long long)firstMatchCounter
		- (unsigned long long)(rowBottomY - mouseY)
		* (unsigned long long)(firstMatchCounter - lastMatchCounter + 1)
		/ (unsigned long long)(INPUT_ICON_SIZE + 3)
	);
}

void InputsPanel::deselectAll(bool redraw, bool* needRedraw) {
	if (needRedraw) *needRedraw = false;
	if (selectionStart == 0xFFFFFFFF) return;
	if (needRedraw) *needRedraw = true;
	unsigned int selectionMin = selectionStart;
	unsigned int selectionMax = selectionEnd;
	if (selectionMin > selectionMax) {
		std::swap(selectionMin, selectionMax);
	}
	auto endIt = commands.begin() + selectionMax + 1;
	for (auto it = commands.begin() + selectionMin; it != endIt; ++it) {
		it->selected = false;
	}
	selectionStart = 0xFFFFFFFF;
	if (redraw) {
		isFirstDraw = true;
		scrollToCommandIndex(scrollRow, positionOffset, false);
	}
}

void InputsPanel::onMouseMove(int mouseX, int mouseY) {
	class Exiter {
	public:
		bool needRedraw = false;
		InputsPanel* panel = nullptr;
		~Exiter() {
			if (needRedraw) {
				panel->isFirstDraw = true;
				panel->scrollToCommandIndex(panel->scrollRow, panel->positionOffset, false);
			}
		}
	} exiter;
	exiter.panel = this;

	if (commands.empty()) {
		return;
	}

	RECT rectData;
	layout.getAnything(&rectData, panelRectData);

	if (mouseY < rectData.top) mouseY = rectData.top;
	if (mouseY >= rectData.bottom) mouseY = rectData.bottom - 1;

	if (allowedToManuallyScroll()) {
		unsigned int newScrollRow = scrollRow;
		if (mouseY - rectData.top < 10) {
			if (scrollRow == 0xFFFFFFFF) {
				exiter.needRedraw = true;
				scrollRow = 0;
				updateScrollbarPos(&rectData);
			}
			else if (scrollRow >= commands.size() - 1) {
				if (positionOffset != 0) {
					exiter.needRedraw = true;
					positionOffset = 0;
					updateScrollbarPos(&rectData);
				}
			} else {
				exiter.needRedraw = true;
				++scrollRow;
				updateScrollbarPos(&rectData);
			}
		}
		else if (rectData.bottom - mouseY < 10) {
			if (scrollMode == SCROLL_MODE_ROW) {
				if (newScrollRow != 0xFFFFFFFF) {
					if (newScrollRow == 0) {
						newScrollRow = 0xFFFFFFFF;
					}
					else {
						--newScrollRow;
					}
					if (scrollRow != newScrollRow) {
						exiter.needRedraw = true;
						scrollRow = newScrollRow;
						updateScrollbarPos(&rectData);
					}
				}
			}
			else if (scrollMode == SCROLL_MODE_FREE) {
				if (newScrollRow != 0xFFFFFFFF) {
					unsigned int newPositionOffset = positionOffset;
					if (newScrollRow == 0) {
						newScrollRow = 0xFFFFFFFF;
						newPositionOffset = 0;
					}
					else {
						--newScrollRow;
					}
					if (scrollRow != newScrollRow || positionOffset != newPositionOffset) {
						exiter.needRedraw = true;
						scrollRow = newScrollRow;
						positionOffset = newPositionOffset;
						updateScrollbarPos(&rectData);
					}
				}
			}
		}
	}

	if (seekbarControlledByPanel) {
		unsigned int targetMatchCounter = calculateMatchCounter(mouseY, &rectData);
		onSeekbarMousePress(targetMatchCounter, false);
		return;
	}

	unsigned int selectedRow = calculateSelectedRow(mouseY, &rectData);
	unsigned int newSelectionEnd = selectedRow;
	unsigned int selectionStartTemp = selectionStart;

	if (selectionStartTemp == 0xFFFFFFFF) {
		if (newSelectionEnd == 0xFFFFFFFF) {
			auto it = commands.begin();
			while (it != commands.end() && it->selected) {
				exiter.needRedraw = true;
				it->selected = false;
				++it;
			}
			selectionEnd = 0xFFFFFFFF;
			return;
		}
		selectionStartTemp = 0;
		if (selectionEnd == 0xFFFFFFFF) {
			selectionEnd = 0;
			commands.front().selected = true;
			exiter.needRedraw = true;
		}
	} else if (selectionEnd == 0xFFFFFFFF) {
		selectionEnd = 0;
	}

	if (newSelectionEnd == 0xFFFFFFFF) newSelectionEnd = 0;

	if (newSelectionEnd >= selectionStartTemp) {
		if (selectionEnd < selectionStartTemp) {
			exiter.needRedraw = true;
			auto endIt = commands.begin() + selectionStartTemp;
			for (auto it = commands.begin() + selectionEnd; it != endIt; ++it) {
				it->selected = false;
			}
			endIt = commands.begin() + newSelectionEnd + 1;
			for (auto it = commands.begin() + selectionStartTemp; it != endIt; ++it) {
				it->selected = true;
			}
		} else if (newSelectionEnd > selectionEnd) {
			exiter.needRedraw = true;
			auto endIt = commands.begin() + newSelectionEnd + 1;
			for (auto it = commands.begin() + selectionEnd + 1; it != endIt; ++it) {
				it->selected = true;
			}
		} else if (newSelectionEnd < selectionEnd) {
			exiter.needRedraw = true;
			auto endIt = commands.begin() + selectionEnd + 1;
			for (auto it = commands.begin() + newSelectionEnd + 1; it != endIt; ++it) {
				it->selected = false;
			}
		}
	} else if (newSelectionEnd < selectionStartTemp) {
		if (selectionEnd > selectionStartTemp) {
			exiter.needRedraw = true;
			auto endIt = commands.begin() + selectionEnd + 1;
			for (auto it = commands.begin() + selectionStartTemp + 1; it != endIt; ++it) {
				it->selected = false;
			}
			endIt = commands.begin() + selectionStartTemp + 1;
			for (auto it = commands.begin() + newSelectionEnd; it != endIt; ++it) {
				it->selected = true;
			}
		} else if (newSelectionEnd > selectionEnd) {
			exiter.needRedraw = true;
			auto endIt = commands.begin() + newSelectionEnd;
			for (auto it = commands.begin() + selectionEnd; it != endIt; ++it) {
				it->selected = false;
			}
		} else if (newSelectionEnd < selectionEnd) {
			exiter.needRedraw = true;
			auto endIt = commands.begin() + selectionEnd;
			for (auto it = commands.begin() + newSelectionEnd; it != endIt; ++it) {
				it->selected = true;
			}
		}
	}
	selectionEnd = newSelectionEnd;
}

void InputsPanel::onMouseUp() {
	isSelecting = false;
	isClickedLast = true;
	if (seekbarControlledByPanel) {
		onSeekbarMouseUp();
		seekbarControlledByPanel = false;
		return;
	}
	shiftSelecting = false;
	if (selectionEnd != 0xFFFFFFFF) {
		if (selectionStart == 0xFFFFFFFF) {
			selectionStart = 0;
		}
	} else if (selectionStart != 0xFFFFFFFF) {
		selectionEnd = 0;
	}
}

void InputsPanel::onSelectAll() {
	if (commands.empty()) return;
	selectionStart = 0;
	selectionEnd = (unsigned int)(commands.size() - 1);
	for (InputsDrawingCommandRow& row : commands) {
		row.selected = true;
	}
	isFirstDraw = true;
	scrollToCommandIndex(scrollRow, positionOffset, false);
}

void InputsPanel::onCopy() {
	ctxDialogText.clear();
	if (commands.empty() || selectionStart == 0xFFFFFFFF) {
		DialogBox(hInst, MAKEINTRESOURCE(IDD_CTXDIALOG), mainWindowHandle, CtxDialogMsgHandler);
		return;
	}
	InputsDrawingCommandRow& startRow = commands[selectionStart];
	InputsDrawingCommandRow& endRow = commands[selectionEnd];
	char initialFacing = 0;
	unsigned int initialMatchCounter = startRow.matchCounter;
	unsigned int lastReadMatchCounter = 0;
	bool lastReadMatchCounterEmpty = true;
	__int64 fileOffset = 0;
	{
		std::unique_lock<std::mutex> fileGuard(replayFileData.indexMutex);
		fileOffset = replayFileData.index[0xFFFFFFFF - initialMatchCounter].fileOffset;
	}
	{
		std::unique_lock<std::mutex> fileGuard(replayFileData.mutex);
		if (replayFileData.file) {
			__int64 filePos = _ftelli64(replayFileData.file);
			_fseeki64(replayFileData.file, fileOffset, SEEK_SET);
			while (true) {
				if (_ftelli64(replayFileData.file) >= (long long)replayFileData.header.indexOffset
						|| !lastReadMatchCounterEmpty
						&& initialMatchCounter - lastReadMatchCounter > 2) {
					break;
				}
				unsigned int fileMatchCounter = readUnsignedInt();
				unsigned int logicsCount = readUnsignedInt();
				char sampleCount = readChar();
				unsigned int totalLogicsSize = readUnsignedInt();
				for (unsigned int logicCounter = 0; logicCounter < logicsCount; ++logicCounter) {
					unsigned int matchCounter = readUnsignedInt();
					lastReadMatchCounterEmpty = false;
					lastReadMatchCounter = matchCounter;
					unsigned int thisOneLogicSize = readUnsignedInt();
					unsigned int consumedSizeSoFar = 0;
					for (unsigned int i = 0; i < 2; ++i) {
						if (i == thisPanelIndex) {
							char thisFacing = readChar();
							++consumedSizeSoFar;
							if (initialMatchCounter - lastReadMatchCounter <= 2) {
								initialFacing = thisFacing;
							}
							const unsigned int nextSize = 1 + sizeof(Input) * 30 + sizeof(unsigned short) * 30 + 4 + 4;
							fseek(replayFileData.file, nextSize, SEEK_CUR);
							consumedSizeSoFar += nextSize;
						} else {
							const unsigned int nextSize = 1 + 1 + sizeof(Input) * 30 + sizeof(unsigned short) * 30 + 4 + 4;
							fseek(replayFileData.file, nextSize, SEEK_CUR);
							consumedSizeSoFar += nextSize;
						}
					}
					fseek(replayFileData.file, thisOneLogicSize - consumedSizeSoFar, SEEK_CUR);
				}
				while (sampleCount) {
					--sampleCount;
					unsigned int sampleMatchCounter = readUnsignedInt();
					unsigned int sampleSize = readUnsignedInt();
					fseek(replayFileData.file,
						8  // UINT64 decodeTimestamp
						+ 8  // LONGLONG sampleTime
						+ 4  // UINT32 pictureType
						+ 1  // char isCleanPoint
						+ 8  // UINT64 quantizationParameter
						+ sampleSize,
						SEEK_CUR);
				}
			}
			_fseeki64(replayFileData.file, filePos, SEEK_SET);
		}
	}
	const bool needMirror = (initialFacing == -1);
	bool needComma = false;
	auto it = replayFileData.replayInputs[thisPanelIndex].begin() + startRow.index;
	auto endIt = replayFileData.replayInputs[thisPanelIndex].begin() + endRow.index + 1;
	for (; it != endIt; ++it) {
		StoredInput& input = *it;
		char direction = '5';
		if (input.right) {
			if (input.up) {
				direction = '9';
			}
			else if (input.down) {
				direction = '3';
			}
			else {
				direction = '6';
			}
		}
		else if (input.left) {
			if (input.up) {
				direction = '7';
			}
			else if (input.down) {
				direction = '1';
			}
			else {
				direction = '4';
			}
		}
		else if (input.up) {
			direction = '8';
		}
		else if (input.down) {
			direction = '2';
		}
		if (needMirror) direction = inputsDrawing.mirrorDirection(direction);
		if (input.framesHeld) {
			if (needComma) {
				ctxDialogText += TEXT(',');
			} else {
				ctxDialogText += TEXT('!');
			}
			needComma = true;
			ctxDialogText += direction;
			if (input.punch) {
				ctxDialogText += TEXT('P');
			}
			if (input.kick) {
				ctxDialogText += TEXT('K');
			}
			if (input.slash) {
				ctxDialogText += TEXT('S');
			}
			if (input.heavySlash) {
				ctxDialogText += TEXT('H');
			}
			if (input.dust) {
				ctxDialogText += TEXT('D');
			}
			if (input.framesHeld > 1) {
				ctxDialogText += TEXT('*');
				ctxDialogText += debugStrNumberToString(input.framesHeld);
			}
		}
	}
	DialogBox(hInst, MAKEINTRESOURCE(IDD_CTXDIALOG), mainWindowHandle, CtxDialogMsgHandler);
}

bool InputsDrawing::aPanelIsSelecting() const {
	for (int i = 0; i < 2; ++i) {
		if (panels[i].isSelecting) return true;
	}
	return false;
}

char InputsPanel::readChar() {
	return fgetc(replayFileData.file);
}

int InputsPanel::readInt() {
	int value;
	fread(&value, 4, 1, replayFileData.file);
	return value;
}

unsigned int InputsPanel::readUnsignedInt() {
	unsigned int value;
	fread(&value, 4, 1, replayFileData.file);
	return value;
}

float InputsPanel::readFloat() {
	float value;
	fread(&value, 4, 1, replayFileData.file);
	return value;
}

UINT64 InputsPanel::readUINT64() {
	UINT64 value;
	fread(&value, 8, 1, replayFileData.file);
	return value;
}

char InputsDrawing::mirrorDirection(char direction) const {
	switch (direction) {
		case '9': return '7';
		case '6': return '4';
		case '3': return '1';
		case '7': return '9';
		case '4': return '6';
		case '1': return '3';
		default: return direction;
	}
}

bool InputsDrawing::seekbarControlledByPanel() const {
	for (int i = 0; i < 2; ++i) {
		if (panels[i].seekbarControlledByPanel) return true;
	}
	return false;
}

void InputsPanel::onArrowUpDown(bool isUp, bool shiftHeld, unsigned int step) {
	class Exiter {
	public:
		bool needRedraw = false;
		InputsPanel* panel = nullptr;
		~Exiter() {
			if (needRedraw) {
				panel->isFirstDraw = true;
				panel->scrollToCommandIndex(panel->scrollRow, panel->positionOffset, false);
			}
		}
	} exiter;
	exiter.panel = this;

	if (commands.empty()) return;
	if (isSelecting) return;

	RECT rectData;
	layout.getAnything(&rectData, panelRectData);

	bool isNonWhole;
	unsigned int remainder;
	const unsigned int howManyFit = calculateHowManyFitOnScreen(&rectData, &isNonWhole, &remainder);
	const unsigned int howManyFitOneLess = (isNonWhole ? howManyFit - 1 : howManyFit);

	const bool allowed = allowedToManuallyScroll();

	const unsigned int oldSelectionStart = selectionStart;
	const unsigned int oldSelectionEnd = selectionEnd;

	if (selectionStart == 0xFFFFFFFF) {
		const unsigned int oldScrollRow = scrollRow;
		const unsigned int oldPositionOffset = positionOffset;
		selectionStart = 0;
		selectionEnd = 0;
		commands.front().selected = true;
		if (allowed) {
			if (scrollRow == 0xFFFFFFFF) {
				scrollRow = 0;
				exiter.needRedraw = true;
			} else if (scrollRow - 0 + 1 > howManyFitOneLess) {
				scrollRow = 0 + howManyFitOneLess - 1;
				if (scrollRow >= commands.size()) scrollRow = (unsigned int)(commands.size() - 1);
			}
			if (oldScrollRow != scrollRow) {
				updateScrollbarPos(&rectData);
			}
		}
		exiter.needRedraw = (oldSelectionStart != selectionStart || oldSelectionEnd != selectionEnd || oldScrollRow != scrollRow);
		return;
	}
	if (shiftHeld) {
		if (isUp && selectionEnd < commands.size() - 1) {
			if (selectionEnd < selectionStart) {
				auto endIt = commands.begin();
				if (selectionStart - selectionEnd <= step) {
					endIt += selectionStart;
				} else {
					endIt += selectionEnd + step;
				}
				for (auto it = commands.begin() + selectionEnd; it != endIt; ++it) {
					it->selected = false;
				}
				if (commands.size() - 1 - selectionEnd <= step) {
					selectionEnd = (unsigned int)(commands.size() - 1);
				} else {
					selectionEnd += step;
				}
				if (selectionEnd > selectionStart) {
					endIt = commands.begin() + selectionEnd + 1;
					for (auto it = commands.begin() + selectionStart + 1; it != endIt; ++it) {
						it->selected = true;
					}
				}
			} else {
				if (commands.size() - 1 - selectionEnd <= step) {
					selectionEnd = (unsigned int)(commands.size() - 1);
				} else {
					selectionEnd += step;
				}
				auto endIt = commands.begin() + selectionEnd + 1;
				for (auto it = commands.begin() + oldSelectionEnd + 1; it != endIt; ++it) {
					it->selected = true;
				}
			}
		} else if (!isUp && selectionEnd != 0) {
			if (selectionEnd > selectionStart) {
				auto endIt = commands.begin() + selectionEnd + 1;
				auto startIt = commands.begin();
				if (selectionEnd - selectionStart <= step) {
					startIt += selectionStart + 1;
				} else {
					startIt += selectionEnd - step + 1;
				}
				for (auto it = startIt; it != endIt; ++it) {
					it->selected = false;
				}
				if (selectionEnd <= step) {
					selectionEnd = 0;
				} else {
					selectionEnd -= step;
				}
				if (selectionEnd < selectionStart) {
					endIt = commands.begin() + selectionStart;
					for (auto it = commands.begin() + selectionEnd; it != endIt; ++it) {
						it->selected = true;
					}
				}
			} else {
				if (selectionEnd <= step) {
					selectionEnd = 0;
				}
				else {
					selectionEnd -= step;
				}
				auto endIt = commands.begin() + oldSelectionEnd;
				for (auto it = commands.begin() + selectionEnd; it != endIt; ++it) {
					it->selected = true;
				}
			}
		}
	} else {
		deselectAll(false, &exiter.needRedraw);
		if (isUp && selectionEnd < commands.size() - 1) {
			if (commands.size() - 1 - selectionEnd <= step) {
				selectionEnd = (unsigned int)(commands.size() - 1);
			} else {
				selectionEnd += step;
			}
		} else if (!isUp && selectionEnd != 0) {
			if (selectionEnd <= step) {
				selectionEnd = 0;
			} else {	
				selectionEnd -= step;
			}
		}
		selectionStart = selectionEnd;
		commands[selectionStart].selected = true;
	}
	if (allowed) {
		bool needRedrawTemp;
		scrollIntoView(selectionEnd, &rectData, &needRedrawTemp);
		exiter.needRedraw = exiter.needRedraw || needRedrawTemp;
	}
	exiter.needRedraw = exiter.needRedraw
		|| oldSelectionStart != selectionStart
		|| oldSelectionEnd != selectionEnd;
}

void InputsPanel::onPageUpDown(bool isUp, bool shiftHeld) {
	if (commands.empty()) return;

	RECT rectData;
	layout.getAnything(&rectData, panelRectData);

	bool isNonWhole;
	const unsigned int howManyFit = calculateHowManyFitOnScreen(&rectData, &isNonWhole);
	const unsigned int howManyFitOneLess = (isNonWhole ? howManyFit - 1 : howManyFit);
	onArrowUpDown(isUp, shiftHeld, howManyFitOneLess);
}

void InputsPanel::onHomeEnd(bool isHome, bool shiftHeld) {
	class Exiter {
	public:
		bool needRedraw = false;
		InputsPanel* panel = nullptr;
		~Exiter() {
			if (needRedraw) {
				panel->isFirstDraw = true;
				panel->scrollToCommandIndex(panel->scrollRow, panel->positionOffset, false);
			}
		}
	} exiter;
	exiter.panel = this;

	if (commands.empty()) return;
	if (isSelecting) return;

	RECT rectData;
	layout.getAnything(&rectData, panelRectData);

	bool isNonWhole;
	unsigned int remainder;
	const unsigned int howManyFit = calculateHowManyFitOnScreen(&rectData, &isNonWhole, &remainder);
	const unsigned int howManyFitOneLess = (isNonWhole ? howManyFit - 1 : howManyFit);

	const bool allowed = allowedToManuallyScroll();

	const unsigned int oldSelectionStart = selectionStart;
	const unsigned int oldSelectionEnd = selectionEnd;

	if (!shiftHeld) {
		const unsigned int oldScrollRow = scrollRow;
		const unsigned int oldPositionOffset = positionOffset;
		deselectAll(false);
		if (!isHome) {
			selectionStart = 0;
			selectionEnd = 0;
			commands.front().selected = true;
			if (allowed) {
				if (scrollRow == 0xFFFFFFFF) {
					scrollRow = 0;
					exiter.needRedraw = true;
				}
				else if (scrollRow - 0 + 1 > howManyFitOneLess) {
					scrollRow = 0 + howManyFitOneLess - 1;
					if (scrollRow >= commands.size()) scrollRow = (unsigned int)(commands.size() - 1);
				}
				if (oldScrollRow != scrollRow) {
					updateScrollbarPos(&rectData);
				}
			}
		} else {
			selectionStart = (unsigned int)(commands.size() - 1);
			selectionEnd = selectionStart;
			commands.back().selected = true;
			if (allowed) {
				scrollRow = selectionStart;
				positionOffset = 0;
				if (oldScrollRow != scrollRow || oldPositionOffset != positionOffset) {
					updateScrollbarPos(&rectData);
				}
			}
		}
		exiter.needRedraw = (oldSelectionStart != selectionStart
			|| oldSelectionEnd != selectionEnd
			|| oldScrollRow != scrollRow
			|| oldPositionOffset != positionOffset);
		return;
	}
	if (shiftHeld) {
		if (isHome && selectionEnd < commands.size() - 1) {
			if (selectionEnd < selectionStart) {
				auto endIt = commands.begin() + selectionStart;
				for (auto it = commands.begin() + selectionEnd; it != endIt; ++it) {
					it->selected = false;
				}
				for (auto it = commands.begin() + selectionStart + 1; it != commands.end(); ++it) {
					it->selected = true;
				}
			}
			else {
				for (auto it = commands.begin() + selectionEnd + 1; it != commands.end(); ++it) {
					it->selected = true;
				}
				commands[selectionEnd].selected = true;
			}
			selectionEnd = (unsigned int)(commands.size() - 1);
		}
		else if (!isHome && selectionEnd != 0) {
			if (selectionEnd > selectionStart) {
				auto endIt = commands.begin() + selectionEnd + 1;
				for (auto it = commands.begin() + selectionStart + 1; it != endIt; ++it) {
					it->selected = false;
				}
				endIt = commands.begin() + selectionStart;
				for (auto it = commands.begin(); it != endIt; ++it) {
					it->selected = true;
				}
			}
			else {
				auto endIt = commands.begin() + selectionEnd;
				for (auto it = commands.begin(); it != endIt; ++it) {
					it->selected = true;
				}
			}
			selectionEnd = 0;
		}
	}
	if (allowed) {
		bool needRedrawTemp;
		scrollIntoView(selectionEnd, &rectData, &needRedrawTemp);
		exiter.needRedraw = exiter.needRedraw || needRedrawTemp;
	}
	exiter.needRedraw = exiter.needRedraw
		|| oldSelectionStart != selectionStart
		|| oldSelectionEnd != selectionEnd;
}

bool InputsPanel::allowedToManuallyScroll() {
	bool isPlaying = false;
	bool isAtEnd = false;
	{
		std::unique_lock<std::mutex> presentationGuard(presentationTimer.mutex);
		isPlaying = presentationTimer.isPlaying;
		isAtEnd = (presentationTimer.matchCounter == 0xFFFFFFFF - (replayFileData.header.logicsCount - 1));
	}
	if (isFollowingPlayback && isPlaying && !isAtEnd || commands.empty()) return false;
	return true;
}

void InputsPanel::scrollIntoView(unsigned int rowIndex, const RECT* rectData, bool* needRedraw) {
	if (needRedraw) *needRedraw = false;
	unsigned int remainder;
	const unsigned int howManyFit = calculateHowManyFitOnScreen(rectData, nullptr, &remainder);
	const unsigned int oldScrollRow = scrollRow;
	const int oldPositionOffset = positionOffset;
	if (rowIndex > scrollRow) {
		scrollRow = rowIndex;
		positionOffset = 0;
	}
	else if (rowIndex == scrollRow && positionOffset != 0) {
		positionOffset = 0;
	}
	else if (scrollRow - rowIndex + 1 > howManyFit) {
		scrollMode = SCROLL_MODE_FREE;
		scrollRow = rowIndex + howManyFit - 1;
		positionOffset = -(int)((int)INPUT_ICON_SIZE + 3 - (int)remainder);
	}
	else {
		int rowBottomY = rectData->top + positionOffset + (scrollRow - rowIndex + 1) * (INPUT_ICON_SIZE + 3) - 1;
		if (rowBottomY >= rectData->bottom) {
			scrollMode = SCROLL_MODE_FREE;
			unsigned int diff = rowBottomY - rectData->bottom;
			unsigned int rowDiff = diff / (INPUT_ICON_SIZE + 3);
			scrollRow -= rowDiff;
			diff -= rowDiff * (INPUT_ICON_SIZE + 3);
			positionOffset -= diff;
			rowDiff = (-positionOffset) / (INPUT_ICON_SIZE + 3);
			positionOffset += rowDiff * (INPUT_ICON_SIZE + 3);
			scrollRow += rowDiff;
		}
	}
	if (oldScrollRow != scrollRow || oldPositionOffset != positionOffset) {
		if (needRedraw) *needRedraw = true;
		updateScrollbarPos(rectData);
	}
}
