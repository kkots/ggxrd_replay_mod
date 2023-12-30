#pragma once
#include "DrawHitboxArrayCallParams.h"
#include <vector>
#include "Entity.h"
#include <mutex>

using determineHitType_t = int(__thiscall*)(void*, void*, BOOL, unsigned int*, unsigned int*);

class HitDetector
{
public:
	
	struct WasHitInfo {
		bool wasHit = false;
		DrawHitboxArrayCallParams hurtbox;
	};

	bool onDllMain();
	void clearAllBoxes();
	void drawHits();
	WasHitInfo wasThisHitPreviously(Entity ent, const DrawHitboxArrayCallParams& currentHurtbox);
	determineHitType_t orig_determineHitType = nullptr;
	std::mutex orig_determineHitTypeMutex;
private:
	
	struct DetectedHitboxes {
		Entity entity{nullptr};
		int team = 0;
		DrawHitboxArrayCallParams hitboxes;
		int activeTime = 0;  // this is needed for Chipp's Gamma Blade, it stops being active on the frame after it hits
		int counter = 0;
		unsigned int previousTime = 0;
		bool invisChippNeedToHide = false;
		bool timeHasChanged(bool globalTimeHasChanged);
	};

	class HookHelp {
	private:
		friend class HitDetector;
		int determineHitTypeHook(void* defender, BOOL wasItType10Hitbox, unsigned int* param3, unsigned int* hpPtr);
	};

	struct Rejection {
		Entity owner{nullptr};
		int left = 0;
		int top = 0;
		int right = 0;
		int bottom = 0;
		int counter = 0;
		int skipFrame = 0;
		int activeFrame = 0;
		bool firstFrame = false;
		bool invisChippNeedToHide = false;
	};

	std::vector<DetectedHitboxes> hitboxesThatHit;
	std::vector<DetectedHitboxes> hurtboxesThatGotHit;
	std::vector<Rejection> rejections;
	std::mutex mutex;

	unsigned int previousTime = 0;
};

extern HitDetector hitDetector;
