#include "pch.h"
#include "EntityList.h"
#include "Game.h"
#include "logging.h"

EntityList entityList;

void EntityList::populate() {

	count = *(const int*)(*aswEngine + 0xB4);
	list = (const char**)(*aswEngine + 0x1FC);
	slots = (const char**)(*aswEngine + 0xC8);

}

unsigned int EntityList::getCurrentTime() {
	if (slots && count >= 0) {
		unsigned int p1Time = *(unsigned int*)(slots[0] + 0x130);
		if (count >= 2) {
			unsigned int p2Time = *(unsigned int*)(slots[1] + 0x130);
			if (previousTime != p2Time) {
				previousTime = p2Time;
				return p2Time;
			}
		}
		if (previousTime != p1Time) {
			previousTime = p1Time;
			return p1Time;
		}
		return previousTime;
	}
	return 0;
}

bool EntityList::areAnimationsNormal() const {
	if (slots && count >= 1) {
		if (!isAnimationNormal(slots[0] + 0x2444)) return false;
	}
	if (slots && count >= 2) {
		if (!isAnimationNormal(slots[1] + 0x2444)) return false;
	}
	return true;
}

bool EntityList::isAnimationNormal(const char* animationName) const {
	// Ichigeki is instant kill but the animation is that even before the IK lands so we can't use it
	if (strcmp(animationName, "CmnActResultLose") == 0) return false;
	if (strcmp(animationName, "CmnActResultWin") == 0) return false;
	if (strcmp(animationName, "CmnActMatchWin") == 0) return false;  // victory animation, is skipped on IK
	return true;
}
