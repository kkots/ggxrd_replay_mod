#pragma once
class AltModes
{
public:
	bool onDllMain();
	bool isGameInNormalMode(bool* needToClearHitDetection, bool* timeToEof, bool* menuOpen);
	char roundendCameraFlybyType() const;
private:
	char* roundendCameraFlybyTypeRef = nullptr;
	char* versusModeMenuOpenRef = nullptr;
	char* isIKCutscenePlaying = nullptr;
	char* pauseMenu = nullptr;
};

extern AltModes altModes;
