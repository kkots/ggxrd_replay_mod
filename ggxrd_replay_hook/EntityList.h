#pragma once

class EntityList
{
public:
	void populate();
	int count = 0;
	const char** list = nullptr;
	const char** slots = nullptr;
	unsigned int getCurrentTime();
	bool areAnimationsNormal() const;
	bool isAnimationNormal(const char* animationName) const;
private:
	unsigned int previousTime = 0;
};

extern EntityList entityList;
