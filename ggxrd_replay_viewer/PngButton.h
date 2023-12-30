#pragma once
#include "PngResource.h"
#include <vector>

enum PngButtonState {
	PNG_BUTTON_STATE_NORMAL,
	PNG_BUTTON_STATE_HOVER,
	PNG_BUTTON_STATE_PRESSED
};

class PngButton
{
public:
	unsigned int id = 0;
	int x = 0;
	int y = 0;
	int width = 0;
	int height = 0;
	bool isCircular = false;
	int radius = 0;
	bool needRedraw = false;
	int tooltipX = 0;
	int tooltipY = 0;
	int tooltipWidth = 0;
	int tooltipHeight = 0;
	struct Resources {
		PngResource* resource = nullptr;
		PngResource* resourceHover = nullptr;
		PngResource* resourcePressed = nullptr;
		LPCTSTR tooltip = nullptr;
	};
	bool createdTooltip = false;
	std::vector<Resources> resources;
	PngButtonState state = PNG_BUTTON_STATE_NORMAL;
	unsigned int resourceIndex = 0;

	PngButton(
		unsigned int id,
		int x,
		int y,
		bool isCircular,
		int width,
		int height,
		int radius,
		PngResource& resource,
		PngResource& resourceHover,
		PngResource& resourcePressed);

	bool isHit(int mouseX, int mouseY);
	void draw(HDC hdc);
	void setState(PngButtonState newState);
	void addResources(PngResource& resource,
		PngResource& resourceHover,
		PngResource& resourcePressed);
	void setResourceIndex(unsigned int newResourceIndex);
};

