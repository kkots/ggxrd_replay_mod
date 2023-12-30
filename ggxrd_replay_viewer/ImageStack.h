#pragma once
#include "framework.h"
#include "PngResource.h"
#include <string>
struct ImageStack {
	WORD resourceId;
	std::string name;
	std::string nameDark;
	std::string nameBlue;
	std::string nameDarkBlue;
	PngResource normal;
	PngResource dark;
	PngResource blue;
	PngResource darkBlue;
};
