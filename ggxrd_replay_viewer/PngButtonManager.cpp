#include "PngButtonManager.h"
#include "WinError.h"
#include "debugMsg.h"
#include <thread>
#include <commctrl.h>  // for tooltips

extern HWND mainWindowHandle;
extern HINSTANCE hInst;
extern HWND globalTooltipHandle;
extern PngButtonManager pngButtonManager;

void PngButtonManager::onWmPaint(const PAINTSTRUCT& paintStruct, HDC hdc) {
	RecursiveLock guard;
	for (auto it = buttons.begin(); it != buttons.end(); ++it) {
		if (it->x <= paintStruct.rcPaint.right
				&& it->x + it->width > paintStruct.rcPaint.left
				&& it->y <= paintStruct.rcPaint.bottom
				&& it->y + it->height > paintStruct.rcPaint.top) {
			it->draw(hdc);
		}
	}
}

void PngButtonManager::addCircularButton(
			unsigned int id,
			int x,
			int y,
			int radius,
			PngResource& resource,
			PngResource& resourceHover,
			PngResource& resourcePressed) {
	buttons.emplace_back(id, x, y, true, 0, 0, radius, resource, resourceHover, resourcePressed);
}

void PngButtonManager::addRectangularButton(
	unsigned int id,
	int x,
	int y,
	int width,
	int height,
	PngResource& resource,
	PngResource& resourceHover,
	PngResource& resourcePressed) {
	buttons.emplace_back(id, x, y, false, width, height, 0, resource, resourceHover, resourcePressed);
}

void PngButtonManager::onMouseMove(int mouseX, int mouseY, bool mouseIsDown) {
	RecursiveLock guard;
	HDC hdc = NULL;
	bool somethingAlreadyHit = false;
	for (auto it = buttons.begin(); it != buttons.end(); ++it) {
		if (somethingAlreadyHit
				|| !it->isHit(mouseX, mouseY)
				|| mouseIsDown && it->id != initiallyPressedButtonId) {
			it->setState(PNG_BUTTON_STATE_NORMAL);
		} else {
			somethingAlreadyHit = true;
			it->setState(mouseIsDown ? PNG_BUTTON_STATE_PRESSED : PNG_BUTTON_STATE_HOVER);
		}
		if (it->needRedraw) {
			if (!hdc) hdc = GetDC(mainWindowHandle);
			it->draw(hdc);
		}
	}
	if (hdc) ReleaseDC(mainWindowHandle, hdc);
}

void PngButtonManager::onMouseUp(int mouseX, int mouseY) {
	RecursiveLock guard;
	HDC hdc = NULL;
	bool somethingAlreadyHit = false;
	for (auto it = buttons.begin(); it != buttons.end(); ++it) {
		if (somethingAlreadyHit
				|| !it->isHit(mouseX, mouseY)
				|| it->id != initiallyPressedButtonId) {
			it->setState(PNG_BUTTON_STATE_NORMAL);
		} else {
			somethingAlreadyHit = true;
			if (it->state == PNG_BUTTON_STATE_PRESSED) {
				if (callback) {
					callback(it->id);
				}
			}
			it->setState(PNG_BUTTON_STATE_HOVER);
		}
		if (it->needRedraw) {
			if (!hdc) hdc = GetDC(mainWindowHandle);
			it->draw(hdc);
		}
	}
	if (hdc) ReleaseDC(mainWindowHandle, hdc);
}

void PngButtonManager::onMouseDown(int mouseX, int mouseY) {
	RecursiveLock guard;
	HDC hdc = NULL;
	initiallyPressedButtonId = 0;
	bool somethingAlreadyHit = false;
	for (auto it = buttons.begin(); it != buttons.end(); ++it) {
		if (somethingAlreadyHit || !it->isHit(mouseX, mouseY)) {
			it->setState(PNG_BUTTON_STATE_NORMAL);
		} else {
			somethingAlreadyHit = true;
			it->setState(PNG_BUTTON_STATE_PRESSED);
			initiallyPressedButtonId = it->id;
		}
		if (it->needRedraw) {
			if (!hdc) hdc = GetDC(mainWindowHandle);
			it->draw(hdc);
		}
	}
	if (hdc) ReleaseDC(mainWindowHandle, hdc);
}

void PngButtonManager::addTooltip(unsigned int id, LPCTSTR text) {
	auto found = buttons.end();
	for (auto it = buttons.begin(); it != buttons.end(); ++it) {
		if (it->id == id) {
			found = it;
			break;
		}
	}
	if (found == buttons.end()) {
		debugMsg("addTooltip: could not find button with id: %u.\n", id);
		return;
	}

	if (!found->createdTooltip) {
		// Set up "tool" information.

		TOOLINFO ti = { 0 };
		ti.cbSize = sizeof(TOOLINFO);
		ti.uId = id;
		ti.uFlags = TTF_SUBCLASS;
		ti.hwnd = mainWindowHandle;
		ti.hinst = hInst;
		ti.lpszText = (LPTSTR)text;
		ti.rect.left = found->x;
		ti.rect.top = found->y;
		ti.rect.right = found->x + found->width;
		ti.rect.bottom = found->y + found->height;

		// Associate the tooltip with the "tool" window.
		if (!SendMessage(globalTooltipHandle, TTM_ADDTOOL, 0, (LPARAM)&ti)) {
			WinError winErr;
			debugMsg("TTM_ADDTOOL failed: %s\n", winErr.getMessageA().c_str());
			// the fallback should be to use the non-unicode tooltips. Use CreateWindowExA, TOOLTIPS_CLASSA, TOOLINFOA, SendMessageA, TTM_ADDTOOLA and non-unicode string in lpszText
			return;
		}
		found->createdTooltip = true;
	}

	bool foundEmptyTooltipSlot = false;
	for (auto it = found->resources.begin(); it != found->resources.end(); ++it) {
		if (!it->tooltip) {
			it->tooltip = text;
			foundEmptyTooltipSlot = true;
			break;
		}
	}
	if (!foundEmptyTooltipSlot) {
		found->resources.push_back(PngButton::Resources{});
		found->resources.back().tooltip = text;
	}

}

void PngButtonManager::addResources(unsigned int id, PngResource& resource,
		PngResource& resourceHover,
		PngResource& resourcePressed) {
	auto found = buttons.end();
	for (auto it = buttons.begin(); it != buttons.end(); ++it) {
		if (it->id == id) {
			found = it;
			break;
		}
	}
	if (found == buttons.end()) {
		debugMsg("addResources: could not find button with id: %u.\n", id);
		return;
	}
	found->addResources(resource, resourceHover, resourcePressed);
}

void PngButtonManager::moveButton(unsigned int id, unsigned int newX, unsigned int newY) {
	RecursiveLock guard;
	auto found = buttons.end();
	for (auto it = buttons.begin(); it != buttons.end(); ++it) {
		if (it->id == id) {
			found = it;
			break;
		}
	}
	if (found == buttons.end()) {
		debugMsg("moveButton: could not find button with id: %u.\n", id);
		return;
	}
	found->x = newX;
	found->y = newY;
	found->needRedraw = true;
	updateTooltip(found, true, &guard);
}

void PngButtonManager::setResourceIndex(unsigned int id, unsigned int resourceIndex) {
	RecursiveLock guard;
	auto found = buttons.end();
	for (auto it = buttons.begin(); it != buttons.end(); ++it) {
		if (it->id == id) {
			found = it;
			break;
		}
	}
	if (found == buttons.end()) {
		debugMsg("setResourceIndex: could not find button with id: %u.\n", id);
		return;
	}
	found->setResourceIndex(resourceIndex);
	if (found->needRedraw) {
		RECT redrawRect;
		redrawRect.left = found->x;
		redrawRect.right = found->x + found->width;
		redrawRect.top = found->y;
		redrawRect.bottom = found->y + found->height;
		InvalidateRect(mainWindowHandle, &redrawRect, FALSE);

		updateTooltip(found, false, &guard);
	}
}

PngButtonManager::RecursiveLock::RecursiveLock() {
	needUnlock = !(pngButtonManager.mutexIsLocked && pngButtonManager.mutexThreadId == GetCurrentThreadId());
	lock();
}

PngButtonManager::RecursiveLock::~RecursiveLock() {
	unlock();
}

void PngButtonManager::RecursiveLock::unlock() {
	if (locked) {
		locked = false;
		pngButtonManager.mutexIsLocked = false;
		pngButtonManager.mutex.unlock();
	}
}

void PngButtonManager::RecursiveLock::lock() {
	if (!needUnlock) return;
	pngButtonManager.mutex.lock();
	locked = true;
	pngButtonManager.mutexIsLocked = true;
	pngButtonManager.mutexThreadId = GetCurrentThreadId();
}

void PngButtonManager::updateTooltip(std::vector<PngButton>::iterator buttonIterator, bool updateRectOnly, RecursiveLock* guard) {
	if (updateRectOnly) {
		if (buttonIterator->tooltipX == buttonIterator->x && buttonIterator->tooltipY == buttonIterator->y
				&& buttonIterator->tooltipWidth == buttonIterator->width && buttonIterator->tooltipHeight == buttonIterator->height) {
			return;
		}
	}
	buttonIterator->tooltipX = buttonIterator->x;
	buttonIterator->tooltipY = buttonIterator->y;
	buttonIterator->tooltipWidth = buttonIterator->width;
	buttonIterator->tooltipHeight = buttonIterator->height;
	PngButton::Resources& slotResources = buttonIterator->resources[buttonIterator->resourceIndex];
	LPCTSTR tooltip = slotResources.tooltip;
	if (tooltip) {
		TOOLINFO ti = { 0 };
		ti.cbSize = sizeof(TOOLINFO);
		ti.uId = buttonIterator->id;
		ti.uFlags = TTF_SUBCLASS;
		ti.hwnd = mainWindowHandle;
		ti.hinst = hInst;
		ti.lpszText = (LPTSTR)tooltip;
		ti.rect.left = buttonIterator->x;
		ti.rect.top = buttonIterator->y;
		ti.rect.right = buttonIterator->x + buttonIterator->width;
		ti.rect.bottom = buttonIterator->y + buttonIterator->height;

		if (guard) guard->unlock();  // SendMessage is blocking call, may cause a paint event that tries to lock RecursiveLock
		// Associate the tooltip with the "tool" window.
		SendMessage(globalTooltipHandle, updateRectOnly ? TTM_NEWTOOLRECT : TTM_UPDATETIPTEXT, 0, (LPARAM)&ti);
	}
}