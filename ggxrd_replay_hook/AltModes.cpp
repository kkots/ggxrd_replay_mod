#include "pch.h"
#include "AltModes.h"
#include "memoryFunctions.h"

AltModes altModes;

bool AltModes::onDllMain() {

	pauseMenu = (char*)sigscanOffset(
		"GuiltyGearXrd.exe",
		"\x8b\x0d\x00\x00\x00\x00\xe8\x00\x00\x00\x00\x3c\x10\x75\x16\x8b\x15\x00\x00\x00\x00\x39\xb2\xf0\x00\x00\x00\x74\x08\xc7\x44\x24\x48\xd9\x00\x00\x00",
		"xx????x????xxxxxx????xxxxxxxxxxxxxxxx",
		{17, 0},
		NULL, "pauseMenu");

	isIKCutscenePlaying = (char*)sigscanOffset(
		"GuiltyGearXrd.exe",
		"\xa1\x00\x00\x00\x00\xb9\x00\x00\x00\x00\x89\x47\x28\xe8\x00\x00\x00\x00\x8b\x0d\x00\x00\x00\x00\x8b\xb1\x00\x00\x00\x00\xa1\x00\x00\x00\x00\x8b\x0d\x00\x00\x00\x01\x0f\x57\xc0\x8b\x1e\x33\xed\x55\x8d\x54\x24\x14",
		"x????x????xxxx????xx????xx????x????xx????xxxxxxxxxxxx",
		{1, 0},
		NULL, "isIKCutscenePlaying");

	if (isIKCutscenePlaying) roundendCameraFlybyTypeRef = isIKCutscenePlaying - 16;

	versusModeMenuOpenRef = (char*)sigscanOffset(
		"GuiltyGearXrd.exe",
		"\xe8\x00\x00\x00\x00\x03\xf3\x83\xfe\x66\x7c\xe5\xa1\x00\x00\x00\x01\x39\x3d\x00\x00\x00\x01\x74\x0f\x29\x1d\x00\x00\x00\x01\x83\xf8\x05\x73\x13\x03\xc3\xeb\x0a\x3b\xc7\x0f\x84\x99\x00\x00\x00\x2b\xc3",
		"x????xxxxxxxx????xx????xxxx????xxxxxxxxxxxxxxxxxxx",
		{ 19, 0 },
		NULL, "versusModeMenuOpenRef");

	return true;
}

bool AltModes::isGameInNormalMode(bool* needToClearHitDetection, bool* timeToEof, bool* menuOpen) {
	if (isIKCutscenePlaying && *isIKCutscenePlaying) {
		if (timeToEof) *timeToEof = true;
		if (needToClearHitDetection) *needToClearHitDetection = true;
		return false;
	}
	if (pauseMenu && *(unsigned int*)(pauseMenu + 0xFC) == 1 && (*(unsigned int*)(pauseMenu + 0x10) & 0x10000) == 0
			|| versusModeMenuOpenRef && *versusModeMenuOpenRef) {
		if (menuOpen) *menuOpen = true;
		return false;
	}
	return true;
}

char AltModes::roundendCameraFlybyType() const {
	if (roundendCameraFlybyTypeRef) return *roundendCameraFlybyTypeRef;
	return 8;
}
