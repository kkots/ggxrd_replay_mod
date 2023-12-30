#pragma once
#include "framework.h"
#include <mutex>
class PresentationTimer
{
public:
	std::mutex mutex;
	unsigned int matchCounter = 0;
	unsigned int scrubbingMatchCounter = 0;
	bool isPlaying = false;
	float speed = 1.F;
	bool isFileOpen = false;
	bool scrubbing = false;
	bool reachedEnd = false;
	HANDLE eventForFileReader = NULL;
	HANDLE eventForPresentationThread = NULL;
};

extern PresentationTimer presentationTimer;
