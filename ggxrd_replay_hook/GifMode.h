#pragma once
#include <atomic>

class GifMode
{
public:
	std::atomic_bool hitboxDisplayDisabled{ true };
	std::atomic_bool drawPushboxCheckSeparately{ true };
	std::atomic_bool inputDisplayDisabled{ true };
};

extern GifMode gifMode;
