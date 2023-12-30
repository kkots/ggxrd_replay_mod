#pragma once
#include <d3d9.h>
#include <d3dx9.h>
#include <mutex>

using updateDarken_t = void(__thiscall*)(char* thisArg);
using updateCamera_t = void(__thiscall*)(char* thisArg, char** param1, char* param2);

struct CameraValues {
	D3DXVECTOR3 pos{ 0.F, 0.F, 0.F };
	int pitch = 0;
	int yaw = 0;
	int roll = 0;
	float fov = 0.F;
	float coordCoefficient = 0.F;
	bool setValues();
	void copyTo(CameraValues& destination);
	bool sent = false;
};

class Camera
{
public:
	void onEndSceneStart();
	void worldToScreen(IDirect3DDevice9* device, const D3DXVECTOR3& vec, D3DXVECTOR3* out);
	bool onDllMain();
	void updateCameraHook(char* thisArg, char** param1, char* param2);
	CameraValues valuesPrepare;
	std::mutex valuesPrepareMutex;
	CameraValues valuesUse;
private:
	friend struct CameraValues;
	class HookHelp {
		friend class Camera;
		void updateCameraHook(char** param1, char* param2);
	};
	float forward[3]{ 0.F };
	float right[3]{ 0.F };
	float up[3]{ 0.F };
	updateCamera_t orig_updateCamera = nullptr;
	std::mutex orig_updateCameraMutex;
	unsigned int darkenValue1Offset = 0;
	unsigned int cameraOffset = 0;
	bool isSet = false;
	float clipXHalf = 0.F;
	float clipYHalf = 0.F;
	float divisor = 0.F;
	void setValues(IDirect3DDevice9* device);
	float convertCoord(float in) const;
	void angleVectors(
		const float p, const float y, const float r,
		float* forward, float* right, float* up) const;
	float vecDot(float* a, float* b) const;
	uintptr_t coordCoeffOffset = 0;
};

extern Camera camera;
