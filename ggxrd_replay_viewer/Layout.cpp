#include "Layout.h"
#include "EncodedImageSize.h"
#include "InputIconSize.h"

Layout layout;
std::mutex layoutMutex;

extern HWND mainWindowHandle;
extern int scrollbarWidth;
extern unsigned int BUTTONBAR_BUTTON_COUNT;
extern TEXTMETRIC textMetric;

void Layout::update() {
	std::unique_lock<std::mutex> guard(layoutMutex);
	RECT clientRect;
	GetClientRect(mainWindowHandle, &clientRect);

	if (clientRect.bottom - clientRect.top < 300) {
		clientRect.bottom = clientRect.top + 300;
	}

	leftPanel.left = 0;

	// 5 pixel space on the edge
	// input pointer + 3 px spacing on right
	// 8 icons
	// 3 pixel space between each pair of neighboring icons
	// 5 pixel space on the edge
	leftPanel.right = 5 + INPUT_POINTER_WIDTH + 3 + INPUT_ICON_SIZE * 8 + 3 * 7 + scrollbarWidth + 5;

	leftPanel.top = 0;
	leftPanel.bottom = clientRect.bottom;

	leftPanelData.left = leftPanel.left;
	leftPanelData.right = leftPanel.right;
	leftPanelData.top = leftPanel.top + FOLLOW_BUTTON_HEIGHT + 5 + 5 + 2;
	leftPanelData.bottom = leftPanel.bottom;

	leftPanelLine.left = leftPanel.right;
	leftPanelLine.right = leftPanelLine.left + 2;
	leftPanelLine.top = leftPanel.top;
	leftPanelLine.bottom = leftPanel.bottom;

	leftPanelHorizLine.top = leftPanelData.top - 2;
	leftPanelHorizLine.bottom = leftPanelHorizLine.top + 2;
	leftPanelHorizLine.left = leftPanelData.left;
	leftPanelHorizLine.right = leftPanelData.right;
	
	buttonbar.left = leftPanelLine.right;
	buttonbar.top = 0;
	buttonbar.right = clientRect.right - (leftPanelLine.right - leftPanel.left);
	const unsigned int buttonbarMinWidthButtonsOnly = (BUTTONBAR_BUTTON_COUNT == 0 ? 0 : 50 * BUTTONBAR_BUTTON_COUNT + 5 * (BUTTONBAR_BUTTON_COUNT - 1));
	const unsigned int textWidth = textMetric.tmAveCharWidth * 20;
	const unsigned int buttonbarMinWidth = buttonbarMinWidthButtonsOnly + 10 + textWidth;
	if (buttonbar.right < buttonbar.left + (int)buttonbarMinWidth) {
		buttonbar.right = buttonbar.left + (int)buttonbarMinWidth;
	}
	buttonbar.bottom = 60;

	text.left = buttonbar.left + buttonbarMinWidthButtonsOnly + 10;
	text.right = text.left + textWidth;
	if (textMetric.tmHeight >= 50) {
		text.top = buttonbar.top + 5;
		text.bottom = text.top + textMetric.tmHeight;
	} else {
		text.top = buttonbar.top + 5 + (50 - textMetric.tmHeight) / 2;
		text.bottom = text.top + textMetric.tmHeight;
	}

	rightPanel.left = buttonbar.right + 2;
	rightPanel.right = rightPanel.left + (leftPanelLine.right - leftPanel.left);
	rightPanel.top = leftPanel.top;
	rightPanel.bottom = leftPanel.bottom;

	rightPanelData.left = rightPanel.left;
	rightPanelData.right = rightPanel.right;
	rightPanelData.top = rightPanel.top + FOLLOW_BUTTON_HEIGHT + 5 + 5 + 2;
	rightPanelData.bottom = rightPanel.bottom;

	rightPanelHorizLine.top = rightPanelData.top - 2;
	rightPanelHorizLine.bottom = rightPanelHorizLine.top + 2;
	rightPanelHorizLine.left = rightPanelData.left;
	rightPanelHorizLine.right = rightPanelData.right;

	rightPanelLine.right = rightPanel.left;
	rightPanelLine.left = rightPanelLine.right - 2;
	rightPanelLine.top = rightPanel.top;
	rightPanelLine.bottom = rightPanel.bottom;

	videoFull.left = leftPanelLine.right;
	videoFull.right = rightPanelLine.left;
	videoFull.top = buttonbar.bottom;
	videoFull.bottom = clientRect.bottom;

	if (videoFull.right - videoFull.left == 0) {
		videoProportional.left = videoFull.left;
		videoProportional.right = videoFull.left + 1;
		videoProportional.top = videoFull.top;
		videoProportional.bottom = videoFull.top + 1;
	} else if ((float)(encodedImageHeight + 5) / (float)encodedImageWidth < (float)(videoFull.bottom - videoFull.top) / (float)(videoFull.right - videoFull.left)) {
		videoProportional.left = videoFull.left;
		videoProportional.right = videoFull.right;
		unsigned int newHeight = (encodedImageHeight + 5) * (videoFull.right - videoFull.left) / encodedImageWidth;
		unsigned int heightDiff = (videoFull.bottom - videoFull.top) - newHeight;
		videoProportional.top = buttonbar.bottom + heightDiff / 2;
		videoProportional.bottom = videoProportional.top + newHeight - 5;
	} else {
		videoProportional.top = videoFull.top;
		videoProportional.bottom = videoFull.bottom - 5;
		unsigned int newWidth = encodedImageWidth * (videoFull.bottom - videoFull.top) / (encodedImageHeight + 5);
		unsigned int widthDiff;
		if ((unsigned int)(videoFull.right - videoFull.left) >= newWidth) {
			widthDiff = (videoFull.right - videoFull.left) - newWidth;
		} else {
			// this actually happened, beware rounding errors
			widthDiff = 0;
		}
		videoProportional.left = buttonbar.left + widthDiff / 2;
		videoProportional.right = videoProportional.left + newWidth;
	}

	seekbarVisual.left = videoProportional.left;
	seekbarVisual.right = videoProportional.right;
	seekbarVisual.top = videoProportional.bottom;
	seekbarVisual.bottom = videoProportional.bottom + 5;

	seekbarClickArea.left = seekbarVisual.left;
	seekbarClickArea.right = seekbarVisual.right;
	seekbarClickArea.top = seekbarVisual.top - 5;
	seekbarClickArea.bottom = seekbarVisual.bottom + 5;
	if (seekbarClickArea.bottom > clientRect.bottom) {
		seekbarClickArea.bottom = clientRect.bottom;
	}
}

void Layout::getVideoAndSeekbar(RECT* videoRect, RECT* seekbarRect) {
	std::unique_lock<std::mutex> layoutGuard(layoutMutex);
	*videoRect = layout.videoProportional;
	*seekbarRect = layout.seekbarVisual;
}

void Layout::getAnything(RECT* destination, RECT* target) {
	std::unique_lock<std::mutex> layoutGuard(layoutMutex);
	*destination = *target;
}
