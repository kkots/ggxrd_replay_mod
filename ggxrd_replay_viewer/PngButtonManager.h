#pragma once
#include "framework.h"
#include "PngButton.h"
#include <vector>
#include <mutex>

using PngButtonCallback = void(*)(unsigned int id);

class PngButtonManager
{
public:
	class RecursiveLock {
	public:
		RecursiveLock();
		~RecursiveLock();
		void lock();
		void unlock();
	private:
		bool needUnlock = false;
		bool locked = false;
	};
	unsigned int initiallyPressedButtonId = 0;
	PngButtonCallback callback = nullptr;
	std::vector<PngButton> buttons;
	void onWmPaint(const PAINTSTRUCT& paintStruct, HDC hdc);
	void addCircularButton(
			unsigned int id,
			int x,
			int y,
			int radius,
			PngResource& resource,
			PngResource& resourceHover,
			PngResource& resourcePressed);
	void addRectangularButton(
		unsigned int id,
		int x,
		int y,
		int width,
		int height,
		PngResource& resource,
		PngResource& resourceHover,
		PngResource& resourcePressed);
	void onMouseMove(int mouseX, int mouseY, bool mouseIsDown);
	void onMouseUp(int mouseX, int mouseY);
	void onMouseDown(int mouseX, int mouseY);
	void addTooltip(unsigned int id, LPCTSTR text);
	void addResources(unsigned int id, PngResource& resource,
		PngResource& resourceHover,
		PngResource& resourcePressed);
	void moveButton(unsigned int id, unsigned int newX, unsigned int newY);
	void updateTooltip(std::vector<PngButton>::iterator buttonIterator, bool updateRectOnly, RecursiveLock* guard = nullptr);
	void setResourceIndex(unsigned int id, unsigned int resourceIndex);
	std::mutex mutex;
	DWORD mutexThreadId = 0;
	bool mutexIsLocked = false;
};

