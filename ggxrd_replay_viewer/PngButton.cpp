#include "PngButton.h"

extern HWND mainWindowHandle;

PngButton::PngButton(unsigned int id,
					 int x,
                     int y,
					 bool isCircular,
					 int width,
					 int height,
					 int radius,
					 PngResource& resource,
					 PngResource& resourceHover,
					 PngResource& resourcePressed)
                    :
					id(id),
					x(x),
					y(y),
	                isCircular(isCircular),
	                width(width),
	                height(height),
					radius(radius) {
	if (isCircular) {
		this->width = radius * 2;
		this->height = radius * 2;
	}
	addResources(resource, resourceHover, resourcePressed);
}

bool PngButton::isHit(int mouseX, int mouseY) {
	if (isCircular) {
		const float centerX = (float)x + (float)radius;
		const float centerY = (float)y + (float)radius;
		const float distanceX = (float)mouseX - centerX;
		const float distanceY = (float)mouseY - centerY;
		const float distance = distanceX * distanceX + distanceY * distanceY;
		const float radiusFloat = (float)radius;
		return distance <= radiusFloat * radiusFloat;
	} else {
		return mouseX >= x && mouseX < x + width && mouseY >= y && mouseY < y + height;
	}
}

void PngButton::draw(HDC hdc) {
	switch (state) {
	case PNG_BUTTON_STATE_NORMAL:
		drawPngResource(hdc, *resources[resourceIndex].resource, x, y);
		break;
	case PNG_BUTTON_STATE_HOVER:
		drawPngResource(hdc, *resources[resourceIndex].resourceHover, x, y);
		break;
	case PNG_BUTTON_STATE_PRESSED:
		drawPngResource(hdc, *resources[resourceIndex].resourcePressed, x, y);
		break;
	}
	needRedraw = false;
}

void PngButton::setState(PngButtonState newState) {
	if (state != newState) {
		needRedraw = true;
		state = newState;
	}
}


void PngButton::addResources(PngResource& resource,
		PngResource& resourceHover,
		PngResource& resourcePressed) {
	resources.push_back(Resources{&resource, &resourceHover, &resourcePressed});
}

void PngButton::setResourceIndex(unsigned int newResourceIndex) {
	if (resourceIndex != newResourceIndex) needRedraw = true;
	resourceIndex = newResourceIndex;
}
