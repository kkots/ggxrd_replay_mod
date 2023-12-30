#pragma once
#include <d3d9.h>
#include <d3dx9.h>
#include <atlbase.h>
#include <vector>
#include "Entity.h"
#include <condition_variable>
#include <mutex>

using EndScene_t = HRESULT(__stdcall*)(IDirect3DDevice9*);
using Present_t = HRESULT(__stdcall*)(IDirect3DDevice9*, const RECT* pSourceRect, const RECT* pDestRect, HWND hDestWindowOverride, const RGNDATA* pDirtyRegion);
using SendUnrealPawnData_t = void(__thiscall*)(char* thisArg);
using ReadUnrealPawnData_t = void(__thiscall*)(char* thisArg);

HRESULT __stdcall hook_EndScene(IDirect3DDevice9* device);
HRESULT __stdcall hook_Present(IDirect3DDevice9* device, const RECT* pSourceRect, const RECT* pDestRect, HWND hDestWindowOverride, const RGNDATA* pDirtyRegion);

class EndScene
{
public:
	bool onDllMain();
	HRESULT presentHook(IDirect3DDevice9* device, const RECT* pSourceRect, const RECT* pDestRect, HWND hDestWindowOverride, const RGNDATA* pDirtyRegion);
	void endSceneHook(IDirect3DDevice9* device);
	void endSceneHookAfter(IDirect3DDevice9* device);
	void removeInvisChippStuff();
	void setPresentFlag();
	bool consumePresentFlag();
	EndScene_t orig_EndScene = nullptr;
	std::mutex orig_EndSceneMutex;
	Present_t orig_Present = nullptr;
	std::mutex orig_PresentMutex;
	SendUnrealPawnData_t orig_SendUnrealPawnData = nullptr;
	bool orig_SendUnrealPawnDataMutexLocked = false;
	DWORD orig_SendUnrealPawnDataMutexThreadId = 0;
	std::mutex orig_SendUnrealPawnDataMutex;
	ReadUnrealPawnData_t orig_ReadUnrealPawnData = nullptr;
	std::mutex orig_ReadUnrealPawnDataMutex;
	bool needStartRecording = false;
	bool recording = false;
	bool unloadDll = false;
private:
	void checkUnloadDll();
	void logic();
	void outOfGameLogic();
	class HookHelp {
		friend class EndScene;
		void sendUnrealPawnDataHook();
		void readUnrealPawnDataHook();
	};
	void sendUnrealPawnDataHook(char* thisArg);
	void readUnrealPawnDataHook(char* thisArg);
	struct HiddenEntity {
		Entity ent{ nullptr };
		int scaleX = 0;
		int scaleY = 0;
		int scaleZ = 0;
		int scaleDefault = 0;
		bool wasFoundOnThisFrame = false;
	};
	bool isEntityAlreadyDrawn(const Entity& ent) const;

	// The EndScene function is actually being called twice: once by GuiltyGear and one more time by the Steam overlay.
	// However, Present is only called once each frame. So we use the Present function to determine if the next EndScene
	// call should draw the boxes.
	bool presentCalled = true;

	std::vector<Entity> drawnEntities;

	unsigned int p1PreviousTimeOfTakingScreen = ~0;
	unsigned int p2PreviousTimeOfTakingScreen = ~0;

	unsigned int matchCounterForInfiniteModes = 0;
	unsigned int previouslyPreparedMatchCounter = 0;

	bool prevRecording = false;
	bool needDrawInEndSceneHook = false;
	bool isEof = true;
};

extern EndScene endScene;
