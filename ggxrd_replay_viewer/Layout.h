#pragma once
#include "framework.h"
#include "InputIconSize.h"
#include <mutex>

class Layout
{
public:
	RECT seekbarVisual;
	RECT seekbarClickArea;
	RECT videoFull;
	RECT videoProportional;
	RECT buttonbar;
	RECT leftPanel;
	RECT rightPanel;
	RECT leftPanelData;
	RECT rightPanelData;
	RECT leftPanelLine;
	RECT rightPanelLine;
	RECT leftPanelHorizLine;
	RECT rightPanelHorizLine;
	RECT text;
	void getVideoAndSeekbar(RECT* videoRect, RECT* seekbarRect);
	void getAnything(RECT* destination, RECT* target);
	void update();
};

extern Layout layout;
extern std::mutex layoutMutex;