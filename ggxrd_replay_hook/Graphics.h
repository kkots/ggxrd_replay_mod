#pragma once
#include <d3d9.h>
#include <d3dx9.h>
#include <vector>
#include "Stencil.h"
#include "rectCombiner.h"
#include "BoundingRect.h"
#include <mutex>
#include "DrawData.h"
#include "LogicSlot.h"
#include "InputsDrawingCommand.h"
#include "InputRingBuffer.h"
#include "InputRingBufferStored.h"
#include "ComplicatedHurtbox.h"

const unsigned int SCREENSHOT_SURFACE_COUNT = 3;

using Reset_t = HRESULT(__stdcall*)(IDirect3DDevice9*, D3DPRESENT_PARAMETERS* pPresentationParameters);

HRESULT __stdcall hook_Reset(IDirect3DDevice9* device, D3DPRESENT_PARAMETERS* pPresentationParameters);

class Graphics
{
public:
	bool onDllMain();
	void onUnload();
	void onEndSceneStart(IDirect3DDevice9* device);
	void drawAll();
	void resetHook();
	void takeScreenshotSimple(IDirect3DDevice9* device, bool recordThisFrame, bool hasInputs, bool isEof);
	void rememberRenderTarget();
	void stopRecording();
	void clearRememberedRenderTarget();
	void clearCollectedInputs();
	
	Reset_t orig_Reset = nullptr;
	std::mutex orig_ResetMutex;
	DrawData drawDataPrepared;
	std::mutex drawDataPreparedMutex;
	DrawData drawDataUse;
	bool needNewDrawData = true;
	bool needNewCameraData = true;
	unsigned int lastSentMatchCounter = 0;

	struct ScreenshotSurface {
		CComPtr<IDirect3DSurface9> offscreenSurface = nullptr;  // how do I make this volatile? The p element is not volatile in the definition
		volatile unsigned int offscreenSurfaceWidth = 0;
		volatile unsigned int offscreenSurfaceHeight = 0;
		volatile unsigned int matchCounter = 0;
		volatile bool consumed = true;
		volatile bool occupied = false;
		volatile bool locked = false;
		volatile unsigned int* lockedRectPtr = nullptr;
		volatile int pitch = 0;
		volatile bool isEof = false;
		volatile bool empty = true;
	};
	HANDLE eventScreenshotSurfaceSet = NULL;
	ScreenshotSurface screenshotSurfaces[SCREENSHOT_SURFACE_COUNT] = { ScreenshotSurface{ }, ScreenshotSurface{ }, ScreenshotSurface{ } };
	std::mutex screenshotSurfacesMutex;
	volatile bool screenshotSurfacesRecording = false;
	volatile bool screenshotSurfacesNeedDeoccupy = false;
	HANDLE eventScreenshotSurfacesDeoccupied = NULL;

	volatile LogicSlot logicBuffer[SCREENSHOT_SURFACE_COUNT] = { LogicSlot{ }, LogicSlot{ }, LogicSlot{ }  };
	std::mutex logicBufferMutex;
	volatile bool needToFreeLogicBuffer = false;
	HANDLE eventLogicBufferSet = NULL;
	HANDLE eventLogicBufferFreed = NULL;


private:
	struct Vertex {
		float x, y, z, rhw;
		D3DCOLOR color;
		Vertex() = default;
		Vertex(float x, float y, float z, float rhw, D3DCOLOR color);
	};
	struct TextureVertex {
		float x, y, z, rhw;
		float u, v;
		TextureVertex() = default;
		TextureVertex(float x, float y, float z, float rhw, float u, float v);
	};
	Stencil stencil;
	std::vector<DrawOutlineCallParams> outlines;
	IDirect3DDevice9* device;
	std::vector<RectCombiner::Polygon> rectCombinerInputBoxes;
	std::vector<std::vector<RectCombiner::PathElement>> rectCombinerOutlines;

	std::pair<ScreenshotSurface*, volatile LogicSlot*> getOffscreenSurface(D3DSURFACE_DESC* renderTargetDescPtr,
			bool recordThisFrame, bool hasInputs, bool isEof);

	void prepareComplicatedHurtbox(const ComplicatedHurtbox& pairOfBoxesOrOneBox);

	unsigned int preparedArrayboxIdCounter = 0;
	struct PreparedArraybox {
		unsigned int id = ~0;
		unsigned int boxesPreparedSoFar = 0;
		BoundingRect boundingRect;
		bool isComplete = false;
	};
	std::vector<PreparedArraybox> preparedArrayboxes;
	std::vector<DrawOutlineCallParams> outlinesOverrideArena;
	void prepareArraybox(const DrawHitboxArrayCallParams& params, bool isComplicatedHurtbox,
						BoundingRect* boundingRect = nullptr, std::vector<DrawOutlineCallParams>* outlinesOverride = nullptr);

	bool outlinesSectionEmpty = true;
	unsigned int outlinesSectionOutlineCount = 0;
	unsigned int outlinesSectionTotalLineCount = 0;
	struct PreparedOutline {
		unsigned int linesSoFar = 0;
		bool isOnePixelThick = false;
		bool isComplete = false;
		bool hasPadding = false;
	};
	std::vector<PreparedOutline> preparedOutlines;
	void prepareOutline(const DrawOutlineCallParams& params);
	void drawOutlinesSection(bool preserveLastTwoVertices);

	unsigned int numberOfPointsPrepared = 0;
	void preparePoint(const DrawPointCallParams& params);

	void worldToScreen(const D3DXVECTOR3& vec, D3DXVECTOR3* out);
	bool getFramebufferData(IDirect3DDevice9* device,
		IDirect3DSurface9* renderTarget,
		D3DSURFACE_DESC* renderTargetDescPtr,
		bool recordThisFrame,
		bool hasInputs,
		bool isEof);
	bool initializePackedTexture();
	void getInputsAndDrawThem();

	enum RenderStateDrawingWhat {
		RENDER_STATE_DRAWING_NOTHING_YET,
		RENDER_STATE_DRAWING_COMPLICATED_HURTBOXES,
		RENDER_STATE_DRAWING_ARRAYBOXES,
		RENDER_STATE_DRAWING_BOXES,
		RENDER_STATE_DRAWING_OUTLINES,
		RENDER_STATE_DRAWING_POINTS,
		RENDER_STATE_DRAWING_TEXTURES
	} drawingWhat = RENDER_STATE_DRAWING_NOTHING_YET;
	void advanceRenderState(RenderStateDrawingWhat newState);
	bool needClearStencil;
	unsigned int lastComplicatedHurtboxId = ~0;
	unsigned int lastArrayboxId = ~0;
	BoundingRect lastBoundingRect;

	bool failedToCreateVertexBuffers = false;
	bool initializeVertexBuffers();

	std::vector<Vertex> vertexArena;
	CComPtr<IDirect3DVertexBuffer9> vertexBuffer;
	const unsigned int vertexBufferSize = 6 * 200;
	unsigned int vertexBufferRemainingSize = 0;
	unsigned int vertexBufferLength = 0;
	std::vector<Vertex>::iterator vertexIt;
	unsigned int vertexBufferPosition = 0;
	bool vertexBufferSent = false;

	enum LastThingInVertexBuffer {
		LAST_THING_IN_VERTEX_BUFFER_NOTHING,
		LAST_THING_IN_VERTEX_BUFFER_END_OF_COMPLICATED_HURTBOX,
		LAST_THING_IN_VERTEX_BUFFER_END_OF_ARRAYBOX,
		LAST_THING_IN_VERTEX_BUFFER_END_OF_BOX,
		LAST_THING_IN_VERTEX_BUFFER_END_OF_THINLINE,
		LAST_THING_IN_VERTEX_BUFFER_END_OF_THICKLINE,
		LAST_THING_IN_VERTEX_BUFFER_END_OF_POINT
	} lastThingInVertexBuffer = LAST_THING_IN_VERTEX_BUFFER_NOTHING;

	unsigned int preparedBoxesCount = 0;
	bool prepareBox(const DrawBoxCallParams& params, BoundingRect* const boundingRect = nullptr, bool ignoreFill = false, bool ignoreOutline = false);
	void resetVertexBuffer();

	std::vector<TextureVertex> textureVertexArena;
	CComPtr<IDirect3DVertexBuffer9> textureVertexBuffer;
	const unsigned int textureVertexBufferSize = 6  // 6 is how many vertices are needed per square (using a single triangle strip call) (first rectangle is only 4 vertices)
		* 3  // average number of buttons per row. 8 max - (direction) p k s h d Sp Taunt
		* 40  // rows to display (estimate)
		* 2;  // two players - two sides
	unsigned int textureVertexBufferRemainingSize = 0;
	unsigned int textureVertexBufferLength = 0;
	std::vector<TextureVertex>::iterator textureVertexIt;
	void prepareTexturedBox(int left, int top, int right, int bottom, const InputsDrawingCommand& cmd);
	void resetTextureVertexBuffer();

	void sendAllPreparedVertices();
	void sendAllPreparedTextureVertices();

	bool drawAllArrayboxes();
	void drawAllBoxes();
	bool drawAllOutlines();
	void drawAllPoints();
	void drawAllTexturedBoxes();

	void drawAllPrepared();

	bool drawIfOutOfSpace(unsigned int verticesCountRequired, unsigned int texturedVerticesCountRequired);

	CComPtr<IDirect3DSurface9> rememberedRenderTarget;
	CComPtr<IDirect3DTexture9> packedTexture;
	InputRingBuffer prevInputRingBuffers[2] = {InputRingBuffer{}};
	InputRingBufferStored inputRingBuffersStored[2] = {InputRingBufferStored{}};
	bool failedToCreatePackedTexture = false;
};

extern Graphics graphics;
