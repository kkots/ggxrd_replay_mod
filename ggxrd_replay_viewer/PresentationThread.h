#pragma once

#include <mutex>
#include <atomic>
#include <mfobjects.h>

#include <d3d9.h>
#include <d3dx9.h>  // this header is from Direct3D 9 SDK, which also contains the d3d9.lib
// d3d9.lib/dll is needed for Direct3D

#include <atlbase.h>  // for CComPtr

#include <chrono>
#include <vector>
#include "SampleWithBufferRef.h"
#include "LogicInfoRef.h"

class PresentationThread
{
public:
	bool initializeDirect3D();
	void threadLoop();
	void destroy();
	void onWmPaint(const PAINTSTRUCT& paintStruct, HDC hdc);
	HANDLE eventResponse = NULL;
	std::atomic_bool needSwitchFiles{ false };
private:
	D3DPRESENT_PARAMETERS d3dPresentParameters{ 0 };
	std::vector<SampleWithBufferRef> samples;
	std::vector<LogicInfoRef> logics;
	bool needReportScrubbing = false;
	bool needReportNotScrubbing = false;
	bool isPlaying = false;
	struct Vertex {
		float x, y, z, rhw;
		D3DCOLOR color;
		Vertex() = default;
		Vertex(float x, float y, float z, float rhw, D3DCOLOR color);
	};
	CComPtr<IDirect3DSurface9> videoSurfaceSystemMemory;
	CComPtr<IDirect3DSurface9> videoSurfaceDefaultMemory;
	CComPtr<IDirect3DSurface9> renderTarget;
	std::chrono::steady_clock::time_point lastPresentationTime;
	bool isFirstPresentation = true;
	D3DSURFACE_DESC renderTargetDesc;
	unsigned int videoWidth = 0;
	unsigned int videoHeight = 0;
	unsigned int matchCounter = 0;
	unsigned int samplesMatchCounter = 0;
	bool scrubbing = false;
	unsigned int scrubbingMatchCounter = 0;
	bool requestPendingToEraseObsoleteFrames = false;
	bool resizeSurfaces();
	bool updateSurface();
	CComPtr<IDirect3DDevice9> d3dDevice;
	std::wstring textArena;
	unsigned int lastTextMatchCounter = 0xFFFFFFFF;
	unsigned int lastTextLastMatchCounter = 0xFFFFFFFF;
	SampleWithBufferRef lastDrawnFrame;
	bool drawFrame(bool needDrawFrame, bool allowSleep, const RECT* videoRect, const RECT* seekbarRect, bool drawLogic, LogicInfoRef logicInfo, bool drawText);
	unsigned char seekbarHeight(const RECT* videoRect, const RECT* seekbarRect) const;
	void drawSeekbar(const RECT* videoRect, const RECT* seekbarRect);
	void removeDuplicateSamples();
	void removeDuplicateLogics();
};

