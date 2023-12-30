#pragma once
#include "Camera.h"
#include "DrawData.h"
struct LogicSlot {
	CameraValues camera;
	DrawData drawData;
	bool occupied = false;
	bool consumed = true;
};