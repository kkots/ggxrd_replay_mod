#pragma once
#include <d3d9.h>
#include "DrawDataType.h"

struct DrawBoxCallParams {
	int left = 0;
	int top = 0;
	int right = 0;
	int bottom = 0;
	D3DCOLOR fillColor{ 0 };
	D3DCOLOR outlineColor{ 0 };
	int thickness = 0;
	DrawDataType type = DRAW_DATA_TYPE_NULL;
	bool invisChippNeedToHide = false;
};
