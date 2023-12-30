#include "pch.h"
#include "Graphics.h"
#include "Detouring.h"
#include "Direct3DVTable.h"
#include "Hitbox.h"
#include "Camera.h"
#include "BoundingRect.h"
#include "logging.h"
#include "pi.h"
#include "colors.h"
#include "FileMapping.h"
#include "InputsDrawing.h"

Graphics graphics;

bool loggedDrawingOperationsOnce = false;

bool Graphics::onDllMain() {

	logwrap(fputs("Graphics::onDllMain called\n", logfile));

	for (unsigned int i = 0; i < _countof(inputRingBuffersStored); ++i) {
		inputRingBuffersStored[i].resize(100);
	}

	orig_Reset = (Reset_t)direct3DVTable.getDirect3DVTable()[16];
	if (!detouring.attach(
		&(PVOID&)(orig_Reset),
		hook_Reset,
		&orig_ResetMutex,
		"Reset")) return false;

	drawDataUse.isEof = true;

	return true;
}

HRESULT __stdcall hook_Reset(IDirect3DDevice9* device, D3DPRESENT_PARAMETERS* pPresentationParameters) {
	++detouring.hooksCounter;
	detouring.markHookRunning("Reset", true);
	HRESULT result;
	{
		graphics.resetHook();
		{
			std::unique_lock<std::mutex> guard(graphics.orig_ResetMutex);
			result = graphics.orig_Reset(device, pPresentationParameters);
		}
	}
	detouring.markHookRunning("Reset", false);
	--detouring.hooksCounter;
	return result;
}

void Graphics::resetHook() {
	stencil.surface = NULL;
	packedTexture = NULL;
	vertexBuffer = NULL;
	textureVertexBuffer = NULL;
	stencil.direct3DSuccess = false;
	std::unique_lock<std::mutex> guard(screenshotSurfacesMutex);
	bool hasOccupiedSurfaces = false;
	for (unsigned int i = 0; i < SCREENSHOT_SURFACE_COUNT; ++i) {
		ScreenshotSurface& screenshotSurface = screenshotSurfaces[i];
		hasOccupiedSurfaces = hasOccupiedSurfaces || screenshotSurface.occupied;
		if (screenshotSurface.offscreenSurface && !screenshotSurface.occupied) {
			if (screenshotSurface.locked) {
				if (!screenshotSurface.offscreenSurface) {
					logwrap(fputs("Error: offscreenSurface is null in resetHook\n", logfile));
				}
				screenshotSurface.offscreenSurface->UnlockRect();
				screenshotSurface.locked = false;
			}
			screenshotSurface.offscreenSurface = nullptr;
			screenshotSurface.offscreenSurfaceWidth = 0;
			screenshotSurface.offscreenSurfaceHeight = 0;
		}
	}
	if (hasOccupiedSurfaces) {
		screenshotSurfacesNeedDeoccupy = true;
		HANDLE objects[2] = { fileMappingManager.eventExiting, eventScreenshotSurfacesDeoccupied };
		guard.unlock();
		DWORD waitResult = WaitForMultipleObjects(2, objects, FALSE, INFINITE);
		if (waitResult == WAIT_OBJECT_0 || waitResult == WAIT_OBJECT_0 + 1) {
			if (waitResult == WAIT_OBJECT_0) {
				fileMappingManager.joinAllThreads();
			}
			guard.lock();
			screenshotSurfacesNeedDeoccupy = false;
			ResetEvent(eventScreenshotSurfacesDeoccupied);
			for (unsigned int i = 0; i < SCREENSHOT_SURFACE_COUNT; ++i) {
				ScreenshotSurface& screenshotSurface = screenshotSurfaces[i];
				if (screenshotSurface.offscreenSurface) {
					if (screenshotSurface.locked) {
						if (!screenshotSurface.offscreenSurface) {
							logwrap(fputs("Error: offscreenSurface is null in resetHook (2)\n", logfile));
						}
						screenshotSurface.offscreenSurface->UnlockRect();
						screenshotSurface.locked = false;
					}
					screenshotSurface.offscreenSurface = nullptr;
					screenshotSurface.offscreenSurfaceWidth = 0;
					screenshotSurface.offscreenSurfaceHeight = 0;
				}
			}

		}
	}
}

void Graphics::onUnload() {
	resetHook();
}

void Graphics::onEndSceneStart(IDirect3DDevice9* device) {
	this->device = device;
	stencil.onEndSceneStart();
}

bool Graphics::prepareBox(const DrawBoxCallParams& params, BoundingRect* const boundingRect, bool ignoreFill, bool ignoreOutline) {
	if (params.left == params.right
		|| params.top == params.bottom) return false;

	int left;
	int right;
	int top;
	int bottom;
	if (params.left > params.right) {
		left = params.right;
		right = params.left;
	} else {
		left = params.left;
		right = params.right;
	}
	if (params.top > params.bottom) {
		top = params.bottom;
		bottom = params.top;
	} else {
		top = params.top;
		bottom = params.bottom;
	}

	D3DXVECTOR3 v1{ (float)left, 0.F, (float)top };
	D3DXVECTOR3 v2{ (float)left, 0.F, (float)bottom };
	D3DXVECTOR3 v3{ (float)right, 0.F, (float)top };
	D3DXVECTOR3 v4{ (float)right, 0.F, (float)bottom };

	D3DXVECTOR3 sp1, sp2, sp3, sp4;
	/*logOnce2(fprintf(logfile, "Drawing box v1 { x: %f, z: %f }, v2 { x: %f, z: %f }, v3 { x: %f, z: %f }, v4 { x: %f, z: %f }\n",
		(double)v1.x, (double)v1.z, (double)v2.x, (double)v2.z, (double)v3.x, (double)v3.z, (double)v4.x, (double)v4.z));*/
	worldToScreen(v1, &sp1);
	worldToScreen(v2, &sp2);
	worldToScreen(v3, &sp3);
	worldToScreen(v4, &sp4);

	bool drewRect = false;

	if ((params.fillColor & 0xFF000000) != 0 && !ignoreFill) {
		drewRect = true;
		if (boundingRect) {
			boundingRect->addX(sp1.x);
			boundingRect->addX(sp2.x);
			boundingRect->addX(sp3.x);
			boundingRect->addX(sp4.x);

			boundingRect->addY(sp1.y);
			boundingRect->addY(sp2.y);
			boundingRect->addY(sp3.y);
			boundingRect->addY(sp4.y);
		}

		/*logOnce2(fprintf(logfile,
			"Box. Red: %u; Green: %u; Blue: %u; Alpha: %u;\n",
			(params.fillColor >> 16) & 0xff, (params.fillColor >> 8) & 0xff, params.fillColor & 0xff, (params.fillColor >> 24) & 0xff));
		logOnce2(fprintf(logfile,
			"sp1 { x: %f; y: %f; }; sp2 { x: %f; y: %f; }; sp3 { x: %f; y: %f; }; sp4 { x: %f; y: %f; }\n",
			(double)sp1.x, (double)sp1.y, (double)sp2.x, (double)sp2.y, (double)sp3.x, (double)sp3.y, (double)sp4.x, (double)sp4.y));*/

		const D3DCOLOR fillColor = params.fillColor;
		if (lastThingInVertexBuffer == LAST_THING_IN_VERTEX_BUFFER_END_OF_BOX) {
			const bool drew = drawIfOutOfSpace(6, 0);
			if (!drew) {
				*vertexIt = *(vertexIt - 1);
				++vertexIt;
				const Vertex firstVertex{ sp1.x, sp1.y, 0.F, 1.F, fillColor };
				*vertexIt = firstVertex;
				++vertexIt;
				*vertexIt = firstVertex;
				++vertexIt;
				vertexBufferLength += 6;
				vertexBufferRemainingSize -= 6;
			} else {
				vertexBufferLength += 4;
				vertexBufferRemainingSize -= 4;
				*vertexIt = Vertex{ sp1.x, sp1.y, 0.F, 1.F, fillColor };
				++vertexIt;
			}
		} else {
			drawIfOutOfSpace(4, 0);
			vertexBufferLength += 4;
			vertexBufferRemainingSize -= 4;
			*vertexIt = Vertex{ sp1.x, sp1.y, 0.F, 1.F, fillColor };
			++vertexIt;
		}
		*vertexIt = Vertex{ sp2.x, sp2.y, 0.F, 1.F, fillColor };
		++vertexIt;
		*vertexIt = Vertex{ sp3.x, sp3.y, 0.F, 1.F, fillColor };
		++vertexIt;
		*vertexIt = Vertex{ sp4.x, sp4.y, 0.F, 1.F, fillColor };
		++vertexIt;
		++preparedBoxesCount;
		lastThingInVertexBuffer = LAST_THING_IN_VERTEX_BUFFER_END_OF_BOX;
	}

	if (params.thickness && !ignoreOutline) {
		DrawOutlineCallParams drawOutlineCallParams;
		drawOutlineCallParams.reserveSize(4);

		drawOutlineCallParams.addPathElem(sp1.x, sp1.y, left, top, 1, 1);
		drawOutlineCallParams.addPathElem(sp2.x, sp2.y, left, bottom, 1, -1);
		drawOutlineCallParams.addPathElem(sp4.x, sp4.y, right, bottom, -1, -1);
		drawOutlineCallParams.addPathElem(sp3.x, sp3.y, right, top, -1, 1);
		drawOutlineCallParams.outlineColor = params.outlineColor;
		drawOutlineCallParams.thickness = params.thickness;
		outlines.push_back(drawOutlineCallParams);
	}
	return drewRect;
}

void Graphics::prepareTexturedBox(int left, int top, int right, int bottom, const InputsDrawingCommand& cmd) {
	if (left == right || top == bottom) return;

	/*logOnce2(fprintf(logfile, "drawTexture: left: %d, top: %d, right: %d, bottom: %d, uStart: %f, vStart: %f, uEnd: %f, vEnd: %f.\n",
		left, top, right, bottom, (double)cmd.uStart, (double)cmd.vStart, (double)cmd.uEnd, (double)cmd.vEnd));*/

	if (left > right) std::swap(left, right);
	if (top > bottom) std::swap(top, bottom);

	if (textureVertexBufferLength && !drawIfOutOfSpace(0, 6)) {
		*textureVertexIt = *(textureVertexIt - 1);
		++textureVertexIt;
		const TextureVertex firstVertex{ (float)left, (float)top, 0.F, 1.F, cmd.uStart, cmd.vStart };
		*textureVertexIt = firstVertex;
		++textureVertexIt;
		*textureVertexIt = firstVertex;
		++textureVertexIt;
		textureVertexBufferLength += 6;
		textureVertexBufferRemainingSize -= 6;
	}
	else {
		drawIfOutOfSpace(0, 4);
		textureVertexBufferLength += 4;
		textureVertexBufferRemainingSize -= 4;
		*textureVertexIt = TextureVertex{ (float)left, (float)top, 0.F, 1.F, cmd.uStart, cmd.vStart };
		++textureVertexIt;
	}
	*textureVertexIt = TextureVertex{ (float)left, float(bottom), 0.F, 1.F, cmd.uStart, cmd.vEnd };
	++textureVertexIt;
	*textureVertexIt = TextureVertex{ (float)right, (float)top, 0.F, 1.F, cmd.uEnd, cmd.vStart };
	++textureVertexIt;
	*textureVertexIt = TextureVertex{ (float)right, (float)bottom, 0.F, 1.F, cmd.uEnd, cmd.vEnd };
	++textureVertexIt;
}

void Graphics::drawAll() {
	
	initializeVertexBuffers();
	resetVertexBuffer();
	resetTextureVertexBuffer();
	preparedArrayboxIdCounter = 0;
	needClearStencil = false;

	drawingWhat = RENDER_STATE_DRAWING_NOTHING_YET;
	device->SetRenderState(D3DRS_STENCILENABLE, TRUE);
	device->SetRenderState(D3DRS_STENCILFUNC, D3DCMP_EQUAL);
	device->SetRenderState(D3DRS_STENCILPASS, D3DSTENCILOP_INCRSAT);
	device->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
	device->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
	device->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
	device->SetVertexShader(nullptr);
	device->SetPixelShader(nullptr);
	device->SetFVF(D3DFVF_XYZRHW | D3DFVF_DIFFUSE);
	device->SetTexture(0, nullptr);
	device->SetStreamSource(0, vertexBuffer, 0, sizeof(Vertex));

	if (!loggedDrawingOperationsOnce) {
		logwrap(fprintf(logfile, "graphics.drawDataUse.hitboxDisplayDisabled: %u, graphics.drawDataUse.empty: %u\n",
			(unsigned int)graphics.drawDataUse.hitboxDisplayDisabled, (unsigned int)graphics.drawDataUse.empty));
	}
	if (!graphics.drawDataUse.hitboxDisplayDisabled && !graphics.drawDataUse.empty) {
		for (const ComplicatedHurtbox& params : drawDataUse.hurtboxes) {
			prepareComplicatedHurtbox(params);
		}
		
		for (auto it = drawDataUse.hitboxes.cbegin(); it != drawDataUse.hitboxes.cend(); ++it) {
			const DrawHitboxArrayCallParams& params = *it;
			bool found = false;
			for (auto itScan = it + 1; itScan != drawDataUse.hitboxes.cend(); ++itScan) {
				if (params == *itScan) {
					found = true;
					break;
				}
			}
			if (!found) prepareArraybox(params, false);
		}
		for (const DrawBoxCallParams& params : drawDataUse.pushboxes) {
			prepareBox(params);
		}
		for (const DrawBoxCallParams& params : drawDataUse.throwBoxes) {
			prepareBox(params);
		}
		for (const DrawOutlineCallParams& params : outlines) {
			prepareOutline(params);
		}
		for (const DrawPointCallParams& params : drawDataUse.points) {
			preparePoint(params);
		}
	}
	if (!graphics.drawDataUse.inputDisplayDisabled && graphics.drawDataUse.hasInputs) {
		getInputsAndDrawThem();
	}
	drawAllPrepared();

	outlines.clear();
	stencil.onEndSceneEnd(device);

	loggedDrawingOperationsOnce = true;

}

void Graphics::prepareArraybox(const DrawHitboxArrayCallParams& params, bool isComplicatedHurtbox,
								BoundingRect* boundingRect, std::vector<DrawOutlineCallParams>* outlinesOverride) {
	if (!params.hitboxCount) return;
	/*logOnce2(fputs("drawHitboxArray called with parameters:\n", logfile));
	logOnce2(fprintf(logfile, "hitboxData: %p\n", params.hitboxData));
	logOnce2(fprintf(logfile, "hitboxCount: %d\n", params.hitboxCount));
	logOnce2(fputs("fillColor: ", logfile));*/
	//logColor(params.fillColor);
	//logOnce2(fputs("\noutlineColor: ", logfile));
	//logColor(params.outlineColor);
	/*logOnce2(fprintf(logfile, "\nposX: %d\nposY: %d\nflip: %hhd\nscaleX: %d\nscaleY: %d\nangle: %d\nhitboxOffsetX: %d\nhitboxOffsetY: %d\n",
		params.params.posX, params.params.posY, params.params.flip, params.params.scaleX, params.params.scaleY,
		params.params.angle, params.params.hitboxOffsetX, params.params.hitboxOffsetY));*/
	rectCombinerInputBoxes.reserve(params.hitboxCount);
	BoundingRect localBoundingRect;
	if (!boundingRect) {
		boundingRect = &localBoundingRect;
	}
	if (!isComplicatedHurtbox) {
		preparedArrayboxes.emplace_back();
		preparedArrayboxes.back().id = preparedArrayboxIdCounter++;
	}

	float angleRads;
	int cos;
	int sin;
	int angleCapped;
	const bool isRotated = (params.params.angle != 0);
	if (isRotated) {
		angleRads = -(float)params.params.angle / 1000.F / 180.F * PI;
		angleCapped = params.params.angle % 360000;
		if (angleCapped < 0) angleCapped += 360000;
		cos = (int)(::cos(angleRads) * 1000.F);
		sin = (int)(::sin(angleRads) * 1000.F);
	}
	
	DrawBoxCallParams drawBoxCall;
	drawBoxCall.fillColor = params.fillColor;

	const Hitbox* hitboxData = params.hitboxData;
	for (int i = 0; i < params.hitboxCount; ++i) {
		//logOnce2(fprintf(logfile, "drawing box %d\n", i));

		int offX = params.params.scaleX * ((int)hitboxData->offX + params.params.hitboxOffsetX / 1000 * params.params.flip);
		int offY = params.params.scaleY * (-(int)hitboxData->offY + params.params.hitboxOffsetY / 1000);
		int sizeX = (int)hitboxData->sizeX * params.params.scaleX;
		int sizeY = -(int)hitboxData->sizeY * params.params.scaleY;

		if (isRotated) {
			int centerX = offX + sizeX / 2;
			int centerY = offY + sizeY / 2;
			if (angleCapped >= 45000 && (angleCapped < 135000 || angleCapped >= 225000)) {
				std::swap(sizeX, sizeY);
			}
			offX = (cos * centerX - sin * centerY) / 1000 - sizeX / 2;
			offY = (cos * centerY + sin * centerX) / 1000 - sizeY / 2;
		}

		offX -= params.params.hitboxOffsetX;
		offX = params.params.posX + offX * params.params.flip;
		sizeX *= params.params.flip;

		offY += params.params.posY + params.params.hitboxOffsetY;

		rectCombinerInputBoxes.emplace_back(offX, offX + sizeX, offY, offY + sizeY);

		drawBoxCall.left = offX;
		drawBoxCall.right = offX + sizeX;
		drawBoxCall.top = offY;
		drawBoxCall.bottom = offY + sizeY;

		if (prepareBox(drawBoxCall, boundingRect, false, true)) {
			++preparedArrayboxes.back().boxesPreparedSoFar;
		}

		++hitboxData;
	}
	if (!isComplicatedHurtbox) {
		PreparedArraybox& preparedArraybox = preparedArrayboxes.back();
		preparedArraybox.isComplete = true;
		preparedArraybox.boundingRect = *boundingRect;
		lastThingInVertexBuffer = LAST_THING_IN_VERTEX_BUFFER_END_OF_ARRAYBOX;
	}

	RectCombiner::getOutlines(rectCombinerInputBoxes, rectCombinerOutlines);
	rectCombinerInputBoxes.clear();
	for (const std::vector<RectCombiner::PathElement>& outline : rectCombinerOutlines) {
		std::vector<DrawOutlineCallParams>* outlinesDest = outlinesOverride ? outlinesOverride : &outlines;
		outlinesDest->emplace_back();
		DrawOutlineCallParams& drawOutlineCallParams = outlinesDest->back();
		drawOutlineCallParams.outlineColor = params.outlineColor;
		drawOutlineCallParams.thickness = params.thickness;
		drawOutlineCallParams.reserveSize(outline.size());
		for (const RectCombiner::PathElement& path : outline) {
			drawOutlineCallParams.addPathElem(path.x, path.y, path.xDir(), path.yDir());
		}
	}
}

Graphics::Vertex::Vertex(float x, float y, float z, float rhw, D3DCOLOR color)
	: x(x), y(y), z(z), rhw(rhw), color(color) { }

Graphics::TextureVertex::TextureVertex(float x, float y, float z, float rhw, float u, float v)
	: x(x), y(y), z(z), rhw(rhw), u(u), v(v) { }

void Graphics::prepareOutline(const DrawOutlineCallParams& params) {
	//logOnce2(fprintf(logfile, "Called drawOutlines with an outline with %d elements\n", params.count()));
	
	D3DXVECTOR3 conv;

	if (params.thickness == 1) {
		
		if (params.empty()) return;

		preparedOutlines.emplace_back();
		preparedOutlines.back().isOnePixelThick = true;

		Vertex firstVertex;

		const bool alreadyProjected = params.getPathElem(0).hasProjectionAlready;

		for (int outlineIndex = 0; outlineIndex < params.count(); ++outlineIndex) {
			const PathElement& elem = params.getPathElem(outlineIndex);

			drawIfOutOfSpace(1, 0);
			if (outlineIndex != 0) {
				++preparedOutlines.back().linesSoFar;
			}

			if (!alreadyProjected) {
				worldToScreen(D3DXVECTOR3{ (float)elem.x, 0.F, (float)elem.y }, &conv);
			} else {
				conv.x = elem.xProjected;
				conv.y = elem.yProjected;
			}

			//logOnce2(fprintf(logfile, "x: %f; y: %f;\n", (double)conv.x, (double)conv.y));
			*vertexIt = Vertex{ conv.x, conv.y, 0.F, 1.F, params.outlineColor };
			if (outlineIndex == 0) firstVertex = *vertexIt;
			++vertexIt;
			++vertexBufferLength;
			--vertexBufferRemainingSize;
		}
		drawIfOutOfSpace(1, 0);
		PreparedOutline& preparedOutline = preparedOutlines.back();
		++preparedOutline.linesSoFar;
		preparedOutline.isComplete = true;
		*vertexIt = firstVertex;
		++vertexIt;
		++vertexBufferLength;
		--vertexBufferRemainingSize;
		lastThingInVertexBuffer = LAST_THING_IN_VERTEX_BUFFER_END_OF_THINLINE;

	} else if (!params.empty()) {

		preparedOutlines.emplace_back();

		Vertex firstVertex;
		Vertex secondVertex;

		bool padTheFirst = (lastThingInVertexBuffer == LAST_THING_IN_VERTEX_BUFFER_END_OF_THICKLINE);
		const bool alreadyProjected = params.getPathElem(0).hasProjectionAlready;

		for (int outlineIndex = 0; outlineIndex < params.count(); ++outlineIndex) {
			const PathElement& elem = params.getPathElem(outlineIndex);

			if (!alreadyProjected) {
				worldToScreen(D3DXVECTOR3{ (float)elem.x, 0.F, (float)elem.y }, &conv);
			} else {
				conv.x = elem.xProjected;
				conv.y = elem.yProjected;
			}

			//logOnce2(fprintf(logfile, "x: %f; y: %f;\n", (double)conv.x, (double)conv.y));
			if (padTheFirst && !drawIfOutOfSpace(4, 0)) {
				firstVertex = Vertex{ conv.x, conv.y, 0.F, 1.F, params.outlineColor };
				*vertexIt = *(vertexIt - 1);
				++vertexIt;
				*vertexIt = firstVertex;
				++vertexIt;
				*vertexIt = firstVertex;
				++vertexIt;
				vertexBufferLength += 4;
				vertexBufferRemainingSize -= 4;
				preparedOutlines.back().hasPadding = true;
			} else {
				drawIfOutOfSpace(2, 0);
				if (outlineIndex == 0) {
					firstVertex = Vertex{ conv.x, conv.y, 0.F, 1.F, params.outlineColor };
					*vertexIt = firstVertex;
					++vertexIt;
				} else {
					++preparedOutlines.back().linesSoFar;
					*vertexIt = Vertex{ conv.x, conv.y, 0.F, 1.F, params.outlineColor };
					++vertexIt;
				}
				vertexBufferLength += 2;
				vertexBufferRemainingSize -= 2;
			}
			padTheFirst = false;
			worldToScreen(D3DXVECTOR3{ (float)elem.x + params.thickness * elem.inX, 0.F, (float)elem.y + params.thickness * elem.inY }, &conv);

			if (outlineIndex == 0) {
				secondVertex = Vertex{ conv.x, conv.y, 0.F, 1.F, params.outlineColor };
				*vertexIt = secondVertex;
				++vertexIt;
			} else {
				*vertexIt = Vertex{ conv.x, conv.y, 0.F, 1.F, params.outlineColor };
				++vertexIt;
			}
		}
		drawIfOutOfSpace(2, 0);
		PreparedOutline& preparedOutline = preparedOutlines.back();
		++preparedOutline.linesSoFar;
		preparedOutline.isComplete = true;
		*vertexIt = firstVertex;
		++vertexIt;
		*vertexIt = secondVertex;
		++vertexIt;
		vertexBufferLength += 2;
		vertexBufferRemainingSize -= 2;
		lastThingInVertexBuffer = LAST_THING_IN_VERTEX_BUFFER_END_OF_THICKLINE;
	}
}

void Graphics::worldToScreen(const D3DXVECTOR3& vec, D3DXVECTOR3* out) {
	camera.worldToScreen(device, vec, out);
}

void Graphics::preparePoint(const DrawPointCallParams& params) {
	D3DXVECTOR3 p{ (float)params.posX, 0.F, (float)params.posY };
	//logOnce2(fprintf(logfile, "drawPoint called x: %f; y: %f; z: %f\n", (double)p.x, (double)p.y, (double)p.z));

	D3DXVECTOR3 sp;
	worldToScreen(p, &sp);

	/*  54321012345 (+)
	*  +-----------+
	*  |           | 5 (+)
	*  |           | 4
	*  |           | 3
	*  | 2        4| 2
	*  |           | 1
	*  |     x     | 0
	*  | 1        3| 1
	*  |           | 2
	*  |           | 3
	*  |           | 4
	*  |           | 5 (-)
	   +-----------+*/

	drawIfOutOfSpace(14, 0);
	vertexBufferLength += 14;
	vertexBufferRemainingSize -= 14;

	const D3DCOLOR fillColor = params.fillColor;

	*vertexIt = Vertex{ sp.x - 4, sp.y - 1, 0.F, 1.F, fillColor };
	++vertexIt;
	*vertexIt = Vertex{ sp.x - 4, sp.y + 2, 0.F, 1.F, fillColor };
	++vertexIt;
	*vertexIt = Vertex{ sp.x + 5, sp.y - 1, 0.F, 1.F, fillColor };
	++vertexIt;
	*vertexIt = Vertex{ sp.x + 5, sp.y + 2, 0.F, 1.F, fillColor };
	++vertexIt;

	/*  54321012345 (+)
	*  +-----------+
	*  |    2  4   | 5 (+)
	*  |           | 4
	*  |           | 3
	*  |           | 2
	*  |           | 1
	*  |     x     | 0
	*  |           | 1
	*  |           | 2
	*  |           | 3
	*  |    1  3   | 4
	*  |           | 5 (-)
	   +-----------+*/

	// PADDING
	*vertexIt = Vertex{ sp.x + 5, sp.y + 2, 0.F, 1.F, fillColor };
	++vertexIt;
	*vertexIt = Vertex{ sp.x - 1, sp.y - 4, 0.F, 1.F, fillColor };
	++vertexIt;

	*vertexIt = Vertex{ sp.x - 1, sp.y - 4, 0.F, 1.F, fillColor };
	++vertexIt;
	*vertexIt = Vertex{ sp.x - 1, sp.y + 5, 0.F, 1.F, fillColor };
	++vertexIt;
	*vertexIt = Vertex{ sp.x + 2, sp.y - 4, 0.F, 1.F, fillColor };
	++vertexIt;
	*vertexIt = Vertex{ sp.x + 2, sp.y + 5, 0.F, 1.F, fillColor };
	++vertexIt;

	/*  54321012345 (+)
	*  +-----------+
	*  |           | 5 (+)
	*  |     4     | 4
	*  |           | 3
	*  |           | 2
	*  |           | 1
	*  |  1  x   2 | 0
	*  |           | 1
	*  |           | 2
	*  |     3     | 3
	*  |           | 4
	*  |           | 5 (-)
	   +-----------+*/

	const D3DCOLOR outlineColor = params.outlineColor;

	*vertexIt = Vertex{ sp.x - 3, sp.y, 0.F, 1.F, outlineColor };
	++vertexIt;
	*vertexIt = Vertex{ sp.x + 4, sp.y, 0.F, 1.F, outlineColor };
	++vertexIt;
	*vertexIt = Vertex{ sp.x, sp.y - 3, 0.F, 1.F, outlineColor };
	++vertexIt;
	*vertexIt = Vertex{ sp.x, sp.y + 4, 0.F, 1.F, outlineColor };
	++vertexIt;
	++numberOfPointsPrepared;
}

std::pair<Graphics::ScreenshotSurface*, volatile LogicSlot*> Graphics::getOffscreenSurface(
			D3DSURFACE_DESC* renderTargetDescPtr,
			bool recordThisFrame,
			bool hasInputs,
			bool isEof) {
	if (isEof) { logwrap(fputs("getOffscreenSurface saw eof\n", logfile)); }
	logOnce2(fputs("getOffscreenSurface called\n", logfile));
	D3DSURFACE_DESC renderTargetDesc;
	if (!renderTargetDescPtr && recordThisFrame) {
		logOnce2(fputs("No render target desc provided, obtaining render target\n", logfile));
		CComPtr<IDirect3DSurface9> renderTarget;
		if (FAILED(device->GetRenderTarget(0, &renderTarget.p))) {
			logwrap(fputs("GetRenderTarget failed\n", logfile));
			return {nullptr, nullptr};
		}
		logOnce2(fputs("Obtained render target\n", logfile));
		logOnce2(fputs("Obtaining desc of render target\n", logfile));
		SecureZeroMemory(&renderTargetDesc, sizeof(renderTargetDesc));
		if (FAILED(renderTarget->GetDesc(&renderTargetDesc))) {
			logwrap(fputs("GetDesc failed\n", logfile));
			return {nullptr, nullptr};
		}
		logOnce2(fputs("Obtained desc of render target\n", logfile));
		renderTargetDescPtr = &renderTargetDesc;
	}
	unsigned int foundSurface = SCREENSHOT_SURFACE_COUNT;
	if (recordThisFrame || isEof) {
		logOnce2(fputs("Locking screenshotSurfacesMutex\n", logfile));
		std::unique_lock<std::mutex> guard(screenshotSurfacesMutex);
		logOnce2(fputs("Locked screenshotSurfacesMutex\n", logfile));
		screenshotSurfacesRecording = true;
		for (unsigned int i = 0; i < SCREENSHOT_SURFACE_COUNT; ++i) {
			volatile ScreenshotSurface& screenshotSurface = screenshotSurfaces[i];
			if (screenshotSurface.consumed && !screenshotSurface.occupied) {
				foundSurface = i;
				logOnce2(fprintf(logfile, "Found surface %u\n", i));
				break;
			}
		}
		if (foundSurface == SCREENSHOT_SURFACE_COUNT) {
			logOnce2(fputs("Surface not found, taking oldest one\n", logfile));
			unsigned int maxMatchCounter = 0;
			for (unsigned int i = 0; i < SCREENSHOT_SURFACE_COUNT; ++i) {
				volatile ScreenshotSurface& screenshotSurface = screenshotSurfaces[i];
				if (screenshotSurface.matchCounter >= maxMatchCounter && !screenshotSurface.occupied) {
					maxMatchCounter = screenshotSurface.matchCounter;
					foundSurface = i;
				}
			}
		}
		if (foundSurface == SCREENSHOT_SURFACE_COUNT) {
			logwrap(fputs("getOffscreenSurface error: foundSurface == SCREENSHOT_SURFACE_COUNT\n", logfile));
		}
		logOnce2(fprintf(logfile, "Final found surface %u\n", foundSurface));
		screenshotSurfaces[foundSurface].occupied = true;
	}

	unsigned int pickedLogicSlot = SCREENSHOT_SURFACE_COUNT;
	{
		logOnce2(fputs("Locking logicBufferMutex\n", logfile));
		std::unique_lock<std::mutex> guard(logicBufferMutex);
		logOnce2(fputs("Locked logicBufferMutex\n", logfile));
		while (true) {
			for (unsigned int i = 0; i < SCREENSHOT_SURFACE_COUNT; ++i) {
				volatile LogicSlot& logicSlot = logicBuffer[i];
				if (logicSlot.consumed && !logicSlot.occupied) {
					logicSlot.occupied = true;
					pickedLogicSlot = i;
					logOnce2(fprintf(logfile, "Picked logic slot %u\n", pickedLogicSlot));
					break;
				}
			}
			if (pickedLogicSlot != SCREENSHOT_SURFACE_COUNT) {
				logOnce2(fputs("Logic slot already picked, exiting infinite loop\n", logfile));
				break;
			}
			logOnce2(fputs("Logic slot not picked, need to wait\n", logfile));
			HANDLE objects[2] = { fileMappingManager.eventExiting, eventLogicBufferFreed };
			needToFreeLogicBuffer = true;
			ResetEvent(eventLogicBufferFreed);
			guard.unlock();
			logOnce2(fputs("Released logicBufferMutex\n", logfile));
			logwrap(fputs("getOffscreenSurface: Calling WaitForMultipleObjects\n", logfile));
			DWORD waitResult = WaitForMultipleObjects(2, objects, FALSE, INFINITE);
			logwrap(fputs("Locking logicBufferMutex\n", logfile));
			guard.lock();
			logwrap(fputs("Locked logicBufferMutex\n", logfile));
			if (waitResult == WAIT_OBJECT_0) {
				logwrap(fputs("WaitForMultipleObjects returned application exit, returning nulls\n", logfile));
				return { nullptr, nullptr };
			} else if (waitResult != WAIT_OBJECT_0 + 1) {
				logwrap(fprintf(logfile, "Abnormal wait result in getOffscreenSurface: %.8x\n", (unsigned int)waitResult));
			}
			logwrap(fprintf(logfile, "Going to mark logic slot %u occupied\n", pickedLogicSlot));
		}
		logOnce2(fprintf(logfile, "Marked logic slot %u occupied\n", pickedLogicSlot));
		logicBuffer[pickedLogicSlot].occupied = true;
	}
	logOnce2(fputs("Released logicBufferMutex\n", logfile));
	volatile LogicSlot& logicSlot = logicBuffer[pickedLogicSlot];
	ScreenshotSurface* screenshotSurface = nullptr;
	if (recordThisFrame || isEof) {
		screenshotSurface = &screenshotSurfaces[foundSurface];
	}
	if (recordThisFrame) {
		if (screenshotSurface->offscreenSurfaceWidth != renderTargetDescPtr->Width || screenshotSurface->offscreenSurfaceHeight != renderTargetDescPtr->Height) {
			logwrap(fprintf(logfile, "getOffscreenSurface: width/height of offscreen surface are obsolete/not set, need new surface: %u, %u, %u, %u, %u\n",
				screenshotSurface->offscreenSurfaceWidth, screenshotSurface->offscreenSurfaceHeight, renderTargetDescPtr->Width, renderTargetDescPtr->Height,
				foundSurface));
			if (screenshotSurface->locked) {
				logwrap(fputs("getOffscreenSurface: surface is locked, need to unlock before freeing\n", logfile));
				if (!screenshotSurface->offscreenSurface) {
					logwrap(fputs("Error: offscreenSurface is null in getOffscreenSurface\n", logfile));
				}
				if (FAILED(screenshotSurface->offscreenSurface->UnlockRect())) {
					logwrap(fputs("UnlockRect failed\n", logfile));
					return {nullptr, nullptr};
				}
				screenshotSurface->locked = false;
			}
			logwrap(fputs("getOffscreenSurface: surface set to null\n", logfile));
			screenshotSurface->offscreenSurface = nullptr;
			logwrap(fputs("getOffscreenSurface: calling CreateOffscreenPlainSurface\n", logfile));
			if (FAILED(device->CreateOffscreenPlainSurface(renderTargetDescPtr->Width,
				renderTargetDescPtr->Height,
				renderTargetDescPtr->Format,
				D3DPOOL_SYSTEMMEM,
				&screenshotSurface->offscreenSurface,
				NULL))) {
				logwrap(fputs("CreateOffscreenPlainSurface failed\n", logfile));
				return {nullptr, nullptr};
			}
			logwrap(fputs("getOffscreenSurface: called CreateOffscreenPlainSurface\n", logfile));
			screenshotSurface->offscreenSurfaceWidth = renderTargetDescPtr->Width;
			screenshotSurface->offscreenSurfaceHeight = renderTargetDescPtr->Height;
		} else if (screenshotSurface->locked) {
			logOnce2(fputs("getOffscreenSurface: surface does not require updating, but is locked, need to unlock before returning\n", logfile));
			if (!screenshotSurface->offscreenSurface) {
				logwrap(fputs("Error: offscreenSurface is null in getOffscreenSurface (2)\n", logfile));
			}
			if (FAILED(screenshotSurface->offscreenSurface->UnlockRect())) {
				logwrap(fputs("UnlockRect failed\n", logfile));
				return {nullptr, nullptr};
			}
			screenshotSurface->locked = false;
		}
	}
	return { screenshotSurface, &logicSlot };
}

bool Graphics::getFramebufferData(IDirect3DDevice9* device,
		IDirect3DSurface9* renderTarget,
		D3DSURFACE_DESC* renderTargetDescPtr,
		bool recordThisFrame,
		bool hasInputs,
		bool isEof) {
	logOnce2(fputs("getFramebufferData called\n", logfile));
	CComPtr<IDirect3DSurface9> renderTargetComPtr;
	if (!renderTarget && recordThisFrame) {
		logOnce2(fputs("No render target provided, obtaining render target\n", logfile));
		if (FAILED(device->GetRenderTarget(0, &renderTargetComPtr.p))) {
			logwrap(fputs("GetRenderTarget failed\n", logfile));
			return false;
		}
		logOnce2(fputs("Obtained render target\n", logfile));
		renderTarget = renderTargetComPtr;
	}
	D3DSURFACE_DESC renderTargetDesc;
	if (!renderTargetDescPtr && recordThisFrame) {
		logOnce2(fputs("No desc provided, obtaining desc of render target\n", logfile));
		SecureZeroMemory(&renderTargetDesc, sizeof(renderTargetDesc));
		if (FAILED(renderTarget->GetDesc(&renderTargetDesc))) {
			logwrap(fputs("GetDesc failed\n", logfile));
			return false;
		}
		logOnce2(fputs("Obtained desc of render target\n", logfile));
		renderTargetDescPtr = &renderTargetDesc;
	}
	logOnce2(fprintf(logfile, "Width: %u, height: %u\n", renderTargetDescPtr->Width, renderTargetDescPtr->Height));

	logOnce2(fputs("Calling getOffscreenSurface\n", logfile));
	std::pair<ScreenshotSurface*, volatile LogicSlot*> getOffscreenSurfaceResult = getOffscreenSurface(
			renderTargetDescPtr,
			recordThisFrame,
			hasInputs,
			isEof);
	logOnce2(fputs("Finished calling getOffscreenSurface\n", logfile));
	ScreenshotSurface* screenshotSurface = getOffscreenSurfaceResult.first;
	volatile LogicSlot* logicSlot = getOffscreenSurfaceResult.second;
	logOnce2(fprintf(logfile, "screenshotSurface: %p, logicSlot: %p, screenshotSurface index: %d, logicSlot index: %d\n",
		screenshotSurface, logicSlot, screenshotSurface == nullptr ? 0 : screenshotSurface - screenshotSurfaces, logicSlot - logicBuffer));
	if (!screenshotSurface && (recordThisFrame || isEof) || !logicSlot) {
		if (isEof) {
			logwrap(fprintf(logfile, "getFramebufferData while under isEof is exiting %p, %u, %u, %p\n",
				screenshotSurface, (unsigned int)recordThisFrame, (unsigned int)isEof, logicSlot));
		}
		return false;
	}
	if (recordThisFrame || isEof) {
		screenshotSurface->empty = true;
		if (recordThisFrame) {
			if (!screenshotSurface->offscreenSurface) {
				logwrap(fputs("Error: offscreenSurface is null in getFramebufferData (2)\n", logfile));
			}
			logOnce2(fputs("Calling GetRenderTargetData\n", logfile));
			if (FAILED(device->GetRenderTargetData(renderTarget, screenshotSurface->offscreenSurface))) {
				logwrap(fprintf(logfile, "GetRenderTargetData failed. renderTarget is: %p. offscreenSurface is %p\n", renderTarget, screenshotSurface->offscreenSurface.p));
				return false;
			}
			logOnce2(fputs("Called GetRenderTargetData\n", logfile));

			D3DLOCKED_RECT lockedRect;
			RECT rect;
			rect.left = 0;
			rect.right = renderTargetDescPtr->Width;
			rect.top = 0;
			rect.bottom = renderTargetDescPtr->Height;
			if (!screenshotSurface->offscreenSurface) {
				logwrap(fputs("Error: offscreenSurface is null in getFramebufferData\n", logfile));
			}
			logOnce2(fputs("Calling LockRect\n", logfile));
			if (FAILED(screenshotSurface->offscreenSurface->LockRect(&lockedRect, &rect, D3DLOCK_READONLY))) {
				logwrap(fputs("LockRect failed\n", logfile));
				return false;
			}
			logOnce2(fputs("Called LockRect\n", logfile));
			screenshotSurface->lockedRectPtr = (unsigned int*)lockedRect.pBits;
			screenshotSurface->pitch = lockedRect.Pitch;
			screenshotSurface->locked = true;
			screenshotSurface->isEof = isEof;
			screenshotSurface->empty = false;
		}
		if (isEof) {
			screenshotSurface->isEof = true;
		}
		{
			logOnce2(fputs("Locking screenshotSurfacesMutex\n", logfile));
			std::unique_lock<std::mutex> guard(screenshotSurfacesMutex);
			logOnce2(fputs("Locked screenshotSurfacesMutex\n", logfile));
			screenshotSurface->consumed = false;
			screenshotSurface->occupied = false;
			screenshotSurface->matchCounter = drawDataUse.matchCounter;
			SetEvent(eventScreenshotSurfaceSet);
			logOnce2(fprintf(logfile, "Set eventScreenshotSurfaceSet into slot %d\n", screenshotSurface - screenshotSurfaces));
		}
	}

	if (!drawDataUse.empty) {
		logOnce2(fputs("Calling drawDataUse.copyTo\n", logfile));
		drawDataUse.copyTo((DrawData*)&logicSlot->drawData);
		logOnce2(fputs("Calling camera.valuesUse.copyTo\n", logfile));
		camera.valuesUse.copyTo((CameraValues&)logicSlot->camera);
		logicSlot->drawData.wontHaveFrame = !recordThisFrame;
	} else if (drawDataUse.hasInputs) {
		logOnce2(fputs("Calling drawDataUse.copyInputsTo\n", logfile));
		drawDataUse.copyInputsTo((DrawData*)&logicSlot->drawData);
		logicSlot->drawData.wontHaveFrame = true;
	}

	logicSlot->drawData.isEof = isEof;
	if (logicSlot->drawData.isEof) { logwrap(fputs("getFramebufferData sends eof\n", logfile)); }
	
	{
		logOnce2(fputs("Locking logicBufferMutex\n", logfile));
		std::unique_lock<std::mutex> guard(logicBufferMutex);
		logOnce2(fputs("Locked logicBufferMutex\n", logfile));
		logicSlot->consumed = false;
		logicSlot->occupied = false;
		SetEvent(eventLogicBufferSet);
		logOnce2(fprintf(logfile, "Set eventLogicBufferSet into slot %d\n", logicSlot - logicBuffer));
	}

	return true;
}

void Graphics::takeScreenshotSimple(IDirect3DDevice9* device, bool recordThisFrame, bool hasInputs, bool isEof) {
	logOnce2(fputs("takeScreenshotSimple called\n", logfile));
	IDirect3DSurface9* renderTargetPtr = nullptr;
	CComPtr<IDirect3DSurface9> renderTarget;
	D3DSURFACE_DESC renderTargetDesc;
	if (recordThisFrame) {
		if (!rememberedRenderTarget) {
			logOnce2(fputs("No rememberedRenderTarget\n", logfile));
			logOnce2(fputs("Getting render target\n", logfile));
			if (FAILED(device->GetRenderTarget(0, &renderTarget.p))) {
				logwrap(fputs("GetRenderTarget failed\n", logfile));
				return;
			}
			logOnce2(fputs("Got render target\n", logfile));
			renderTargetPtr = renderTarget;
		} else {
			logOnce2(fprintf(logfile, "Have remembered target: %p\n", rememberedRenderTarget.p));
			renderTargetPtr = rememberedRenderTarget;
		}
		SecureZeroMemory(&renderTargetDesc, sizeof(renderTargetDesc));
		logOnce2(fputs("Calling GetDesc on render target\n", logfile));
		if (FAILED(renderTargetPtr->GetDesc(&renderTargetDesc))) {
			logwrap(fputs("GetDesc failed\n", logfile));
			return;
		}
	}
	logOnce2(fputs("Got desc\n", logfile));
	logOnce2(fputs("Calling getFramebufferData\n", logfile));
	getFramebufferData(device, renderTargetPtr, &renderTargetDesc, recordThisFrame, hasInputs, isEof);
	logOnce2(fputs("Finished calling getFramebufferData\n", logfile));

}

void Graphics::rememberRenderTarget() {
	if (rememberedRenderTarget) {
		logwrap(fputs("Error: rememberedRenderTarget is not null\n", logfile));
	}
	if (FAILED(device->GetRenderTarget(0, &rememberedRenderTarget.p))) {
		logwrap(fputs("GetRenderTarget failed\n", logfile));
		return;
	}
}

void Graphics::stopRecording() {
	if (!screenshotSurfacesRecording) return;
	logwrap(fputs("Graphics stopped recording\n", logfile));
	std::unique_lock<std::mutex> guard(screenshotSurfacesMutex);
	if (!screenshotSurfacesRecording) return;
	screenshotSurfacesRecording = false;
	for (unsigned int i = 0; i < SCREENSHOT_SURFACE_COUNT; ++i) {
		ScreenshotSurface& screenshotSurface = screenshotSurfaces[i];
		if (!screenshotSurface.occupied && screenshotSurface.offscreenSurface) {
			if (screenshotSurface.locked) {
				if (!screenshotSurface.offscreenSurface) {
					logwrap(fputs("Error: offscreenSurface is null in stopRecording\n", logfile));
				}
				screenshotSurface.offscreenSurface->UnlockRect();
				screenshotSurface.locked = false;
			}
			screenshotSurface.offscreenSurface = nullptr;
			screenshotSurface.offscreenSurfaceHeight = 0;
			screenshotSurface.offscreenSurfaceWidth = 0;
		}
	}
}

bool Graphics::initializePackedTexture() {
	if (failedToCreatePackedTexture) return false;
	if (packedTexture) return true;
	CComPtr<IDirect3DTexture9> systemTexture;
	if (FAILED(device->CreateTexture(inputsDrawing.packedTexture.width, inputsDrawing.packedTexture.height, 1, NULL, D3DFMT_A8R8G8B8, D3DPOOL_SYSTEMMEM, &systemTexture, NULL))) {
		logwrap(fputs("CreateTexture failed\n", logfile));
		failedToCreatePackedTexture = true;
		return false;
	}
	if (FAILED(device->CreateTexture(inputsDrawing.packedTexture.width, inputsDrawing.packedTexture.height, 1, NULL, D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT, &packedTexture, NULL))) {
		logwrap(fputs("CreateTexture (2) failed\n", logfile));
		failedToCreatePackedTexture = true;
		return false;
	}
	D3DLOCKED_RECT lockedRect;
	if (FAILED(systemTexture->LockRect(0, &lockedRect, NULL, NULL))) {
		logwrap(fputs("texture->LockRect failed\n", logfile));
		failedToCreatePackedTexture = true;
		return false;
	}
	inputsDrawing.packedTexture.bitBlt(lockedRect.pBits, lockedRect.Pitch, 0, 0, 0, 0, inputsDrawing.packedTexture.width, inputsDrawing.packedTexture.height);
	if (FAILED(systemTexture->UnlockRect(0))) {
		logwrap(fputs("texture->UnlockRect failed\n", logfile));
		failedToCreatePackedTexture = true;
		return false;
	}
	if (FAILED(device->UpdateTexture(systemTexture, packedTexture))) {
		logwrap(fputs("UpdateTexture failed\n", logfile));
		failedToCreatePackedTexture = true;
		return false;
	}
	logwrap(fprintf(logfile, "Initialized packed texture successfully. Width: %u; Height: %u.\n", inputsDrawing.packedTexture.width, inputsDrawing.packedTexture.height));
	return true;
}

bool loggedDrawingInputsOnce = false;
void Graphics::getInputsAndDrawThem() {
	if (!initializePackedTexture()) return;
	D3DVIEWPORT9 viewport;
	device->GetViewport(&viewport);
	for (int i = 0; i < 2; ++i) {
		if (!loggedDrawingInputsOnce) {
			logwrap(fprintf(logfile, "Input ring buffer contents for player side %u:\n", (unsigned int)i));
			logwrap(fprintf(logfile, "currentInput: %.4hX\n", drawDataUse.inputRingBuffers[i].currentInput));
			logwrap(fprintf(logfile, "index: %hu\n", drawDataUse.inputRingBuffers[i].index));
			logwrap(fprintf(logfile, "lastInput: %.4hX\n", drawDataUse.inputRingBuffers[i].lastInput));
			logwrap(fprintf(logfile, "inputs: "));
			for (int i = 0; i < 30; ++i) {
				logwrap(fprintf(logfile, "%.4hX ", drawDataUse.inputRingBuffers[i].inputs[i]));
			}
			logwrap(fprintf(logfile, "\n"));
			logwrap(fprintf(logfile, "framesHeld: "));
			for (int i = 0; i < 30; ++i) {
				logwrap(fprintf(logfile, "%.4hX ", drawDataUse.inputRingBuffers[i].framesHeld[i]));
			}
			logwrap(fprintf(logfile, "\n"));
		}
		inputRingBuffersStored[i].update(graphics.drawDataUse.inputRingBuffers[i], prevInputRingBuffers[i]);
		prevInputRingBuffers[i] = graphics.drawDataUse.inputRingBuffers[i];
		InputsDrawingCommandRow result[100];
		memset(result, 0, sizeof(result));
		unsigned int resultSize = 0;
		inputsDrawing.produceData(inputRingBuffersStored[i], result, &resultSize);

		if (!loggedDrawingInputsOnce) {
			logwrap(fprintf(logfile, "Number of lines of inputs on player %u side: %u\n", (unsigned int)i, resultSize));
		}

		int screenBorder = 2;
		int fullWidthStep = 20;
		int spacingStep = 2;
		if (i == 1) {
			screenBorder = viewport.Width - 2 - fullWidthStep;
			fullWidthStep = -fullWidthStep;
			spacingStep = -spacingStep;
		}

		int currentY = 143;
		const unsigned int viewportHeight = viewport.Height;

		for (const InputsDrawingCommandRow* ptr = result + resultSize - 1; ptr >= result && currentY <= (int)viewportHeight; --ptr) {
			const InputsDrawingCommandRow& row = *ptr;
			int currentX = screenBorder;
			if (!loggedDrawingInputsOnce) {
				logwrap(fprintf(logfile, "Number of inputs on line %u on player %u side: %u\n", (unsigned int)(ptr - result), (unsigned int)i, row.count));
			}
			for (unsigned char rowIterator = 0; rowIterator < row.count; ++rowIterator) {
				prepareTexturedBox(currentX, currentY, currentX + 20, currentY + 20, row.cmds[rowIterator]);
				currentX += fullWidthStep + spacingStep;
			}
			currentY += 20 + 2;
		}

	}
	loggedDrawingInputsOnce = true;
}

void Graphics::clearRememberedRenderTarget() {
	rememberedRenderTarget = nullptr;
}

void Graphics::clearCollectedInputs() {
	for (unsigned int i = 0; i < _countof(inputRingBuffersStored); ++i) {
		inputRingBuffersStored[i].clear();
	}
}

bool Graphics::initializeVertexBuffers() {
	if (failedToCreateVertexBuffers) return false;
	if (vertexBuffer) return true;
	if (FAILED(device->CreateVertexBuffer(sizeof(Vertex) * vertexBufferSize, D3DUSAGE_DYNAMIC, D3DFVF_XYZRHW | D3DFVF_DIFFUSE, D3DPOOL_DEFAULT, &vertexBuffer, NULL))) {
		logwrap(fputs("CreateVertexBuffer failed\n", logfile));
		failedToCreateVertexBuffers = false;
		return false;
	}
	vertexArena.resize(vertexBufferSize);

	if (FAILED(device->CreateVertexBuffer(sizeof(TextureVertex) * textureVertexBufferSize, D3DUSAGE_DYNAMIC, D3DFVF_XYZRHW | D3DFVF_TEX1, D3DPOOL_DEFAULT, &textureVertexBuffer, NULL))) {
		logwrap(fputs("CreateVertexBuffer (2) failed\n", logfile));
		failedToCreateVertexBuffers = false;
		return false;
	}
	textureVertexArena.resize(textureVertexBufferSize);

	return true;
}

void Graphics::resetVertexBuffer() {
	vertexBufferRemainingSize = vertexBufferSize;
	vertexBufferLength = 0;
	vertexIt = vertexArena.begin();
	lastThingInVertexBuffer = LAST_THING_IN_VERTEX_BUFFER_NOTHING;
	vertexBufferPosition = 0;
}

void Graphics::resetTextureVertexBuffer() {
	textureVertexBufferRemainingSize = textureVertexBufferSize;
	textureVertexBufferLength = 0;
	textureVertexIt = textureVertexArena.begin();
}

bool Graphics::drawIfOutOfSpace(unsigned int verticesCountRequired, unsigned int texturedVerticesCountRequired) {
	if (vertexBufferRemainingSize < verticesCountRequired || textureVertexBufferRemainingSize < texturedVerticesCountRequired) {
		drawAllPrepared();
		return true;
	}
	return false;
}

void Graphics::prepareComplicatedHurtbox(const ComplicatedHurtbox& pairOfBoxesOrOneBox) {
	if (pairOfBoxesOrOneBox.hasTwo) {
		preparedArrayboxes.emplace_back();
		preparedArrayboxes.back().id = preparedArrayboxIdCounter++;
		BoundingRect boundingRect;
		prepareArraybox(pairOfBoxesOrOneBox.param1, true, &boundingRect, &outlinesOverrideArena);
		prepareArraybox(pairOfBoxesOrOneBox.param2, true, &boundingRect);
		PreparedArraybox& preparedArraybox = preparedArrayboxes.back();
		preparedArraybox.isComplete = true;
		preparedArraybox.boundingRect = boundingRect;
		lastThingInVertexBuffer = LAST_THING_IN_VERTEX_BUFFER_END_OF_COMPLICATED_HURTBOX;
		outlines.insert(outlines.end(), outlinesOverrideArena.begin(), outlinesOverrideArena.end());
		outlinesOverrideArena.clear();
	} else {
		prepareArraybox(pairOfBoxesOrOneBox.param1, false);
	}
}

void Graphics::advanceRenderState(RenderStateDrawingWhat newState) {
	if (drawingWhat == RENDER_STATE_DRAWING_NOTHING_YET
			|| drawingWhat == RENDER_STATE_DRAWING_COMPLICATED_HURTBOXES
			|| drawingWhat == RENDER_STATE_DRAWING_ARRAYBOXES) {
		if (newState == RENDER_STATE_DRAWING_COMPLICATED_HURTBOXES
				|| newState == RENDER_STATE_DRAWING_ARRAYBOXES) {
			drawingWhat = newState;
			return;
		}
		device->SetRenderState(D3DRS_STENCILENABLE, FALSE);
	}
	if (drawingWhat != RENDER_STATE_DRAWING_OUTLINES
			&& drawingWhat != RENDER_STATE_DRAWING_POINTS
			&& (newState == RENDER_STATE_DRAWING_OUTLINES
			|| newState == RENDER_STATE_DRAWING_POINTS)) {
		device->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);

	} else if ((drawingWhat == RENDER_STATE_DRAWING_OUTLINES
			|| drawingWhat == RENDER_STATE_DRAWING_POINTS)
			&& newState != RENDER_STATE_DRAWING_OUTLINES
			&& newState != RENDER_STATE_DRAWING_POINTS) {
		device->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
	}
	if (drawingWhat != RENDER_STATE_DRAWING_TEXTURES
			&& newState == RENDER_STATE_DRAWING_TEXTURES) {
		device->SetFVF(D3DFVF_XYZRHW | D3DFVF_TEX1);
		device->SetTexture(0, packedTexture);
		device->SetStreamSource(0, textureVertexBuffer, 0, sizeof(TextureVertex));
	}
	drawingWhat = newState;
}

void Graphics::drawAllPrepared() {
	if (!loggedDrawingOperationsOnce) {
		logwrap(fprintf(logfile, "Arrayboxes count: %u\nboxes count (including those in arrayboxes): %u\noutlines count: %u, points count: %u, textured boxes count: %u\n",
			preparedArrayboxes.size(), preparedBoxesCount, preparedOutlines.size(), numberOfPointsPrepared, textureVertexBufferLength > 4 ? (textureVertexBufferLength - 2) / 6 : textureVertexBufferLength / 4));
	}
	switch (1) {
	case 1:
		if (!drawAllArrayboxes()) break;
		drawAllBoxes();
		if (!drawAllOutlines()) break;
		drawAllPoints();
		drawAllTexturedBoxes();
	}
	if (vertexBufferPosition != 0) {
		if (vertexBufferPosition > vertexBufferLength) {
			logwrap(fprintf(logfile, "vertexBufferPosition > vertexBufferLength: %u, %u\n", vertexBufferPosition, vertexBufferLength));
		}
		if (vertexBufferPosition != vertexBufferLength) {
			auto destinationIt = vertexArena.begin();
			const auto itEnd = vertexArena.begin() + vertexBufferLength;
			for (auto it = vertexArena.begin() + vertexBufferPosition; it != itEnd; ++it) {
				*destinationIt = *it;
				++destinationIt;
			}
		}
		vertexBufferLength -= vertexBufferPosition;
		vertexBufferPosition = 0;
		vertexIt = vertexArena.begin() + vertexBufferLength;
		vertexBufferRemainingSize = vertexBufferSize - vertexBufferLength;
		if (!loggedDrawingOperationsOnce) {
			logwrap(fprintf(logfile, "drawAllPrepared: resetting vertex buffer: vertexBufferLength: %u, vertexBufferPosition: 0, vertexIt: %u, vertexBufferRemainingSize: %u\n",
				vertexBufferLength, vertexIt - vertexArena.begin(), vertexBufferRemainingSize));
		}
		lastThingInVertexBuffer = LAST_THING_IN_VERTEX_BUFFER_NOTHING;
	}
	vertexBufferSent = false;
}

void Graphics::sendAllPreparedVertices() {
	if (vertexBufferSent) return;
	vertexBufferSent = true;
	Vertex* buffer = nullptr;
	if (FAILED(vertexBuffer->Lock(0, 0, (void**)&buffer, D3DLOCK_DISCARD))) return;
	memcpy(buffer, &vertexArena.front(), sizeof(Vertex) * vertexBufferLength);
	if (FAILED(vertexBuffer->Unlock())) return;
}

void Graphics::sendAllPreparedTextureVertices() {
	TextureVertex* buffer = nullptr;
	if (FAILED(textureVertexBuffer->Lock(0, 0, (void**)&buffer, D3DLOCK_DISCARD))) return;
	memcpy(buffer, &textureVertexArena.front(), sizeof(TextureVertex) * textureVertexBufferLength);
	if (FAILED(textureVertexBuffer->Unlock())) return;
}

bool Graphics::drawAllArrayboxes() {
	if (preparedArrayboxes.empty()) return true;
	sendAllPreparedVertices();
	for (auto it = preparedArrayboxes.begin(); it != preparedArrayboxes.end(); ++it) {
		if (it->boxesPreparedSoFar) {

			if (needClearStencil && lastArrayboxId != it->id) {
				stencil.clearRegion(device, lastBoundingRect);
				needClearStencil = false;
			}

			stencil.initialize(device);
			device->DrawPrimitive(D3DPT_TRIANGLESTRIP, vertexBufferPosition, 2 + (it->boxesPreparedSoFar - 1) * 6);
			vertexBufferPosition += 4 + (it->boxesPreparedSoFar - 1) * 6;
			if (vertexBufferPosition > vertexBufferLength) {
				logwrap(fprintf(logfile, "drawAllArrayboxes made vertex buffer position go out of bounds: %u, %u. Bounding rect: %f, %f, %f, %f\n",
					vertexBufferPosition, vertexBufferLength, (double)it->boundingRect.left, (double)it->boundingRect.top, (double)it->boundingRect.bottom,
					(double)it->boundingRect.right));
			}
			preparedBoxesCount -= it->boxesPreparedSoFar;
			it->boxesPreparedSoFar = 0;
			
			needClearStencil = true;
			lastArrayboxId = it->id;
			lastBoundingRect = it->boundingRect;

			if (!it->isComplete) {
				preparedArrayboxes.erase(preparedArrayboxes.begin(), it);
				return false;
			}

		} else if (!it->isComplete) {
			preparedArrayboxes.erase(preparedArrayboxes.begin(), it);
			return false;
		}
	}
	preparedArrayboxes.clear();
	return true;
}

void Graphics::drawAllBoxes() {
	if (!preparedBoxesCount) return;
	sendAllPreparedVertices();
	advanceRenderState(RENDER_STATE_DRAWING_BOXES);
	device->DrawPrimitive(D3DPT_TRIANGLESTRIP, vertexBufferPosition, 2 + (preparedBoxesCount - 1) * 6);
	vertexBufferPosition += 4 + (preparedBoxesCount - 1) * 6;
	preparedBoxesCount = 0;
}

void Graphics::drawOutlinesSection(bool preserveLastTwoVertices) {
	if (!outlinesSectionEmpty) {
		if (!loggedDrawingOperationsOnce) {
			logwrap(fprintf(logfile, "drawOutlinesSection: preserveLastTwoVertices: %u; vertexBufferPosition: %u, outlinesSectionTotalLineCount: %u, outlinesSectionOutlineCount: %u\n",
				(unsigned int)preserveLastTwoVertices, vertexBufferPosition, outlinesSectionTotalLineCount, outlinesSectionOutlineCount));
		}
		device->DrawPrimitive(D3DPT_TRIANGLESTRIP, vertexBufferPosition, 2 * outlinesSectionTotalLineCount + 4 * (outlinesSectionOutlineCount - 1));
		vertexBufferPosition += (preserveLastTwoVertices ? 0 : 2) + 2 * outlinesSectionTotalLineCount + 4 * (outlinesSectionOutlineCount - 1);
		outlinesSectionEmpty = true;
	}
}

bool Graphics::drawAllOutlines() {
	if (preparedOutlines.empty()) return true;
	sendAllPreparedVertices();
	advanceRenderState(RENDER_STATE_DRAWING_OUTLINES);

	if (!loggedDrawingOperationsOnce) {
		logwrap(fprintf(logfile, "drawAllOutlines: vertexBufferPosition: %u\n", vertexBufferPosition));
	}

	for (auto it = preparedOutlines.begin(); it != preparedOutlines.end(); ++it) {
		if (!loggedDrawingOperationsOnce) {
			logwrap(fprintf(logfile, "drawAllOutlines: outline index: %u; it->isOnePixelThick: %u; it->linesSoFar: %u; it->isComplete: %u\n",
				it - preparedOutlines.begin(), (unsigned int)it->isOnePixelThick, it->linesSoFar, (unsigned int)it->isComplete));
		}
		if (it->isOnePixelThick) {
			if (it->linesSoFar) {
				drawOutlinesSection(false);
				device->DrawPrimitive(D3DPT_LINESTRIP, vertexBufferPosition, it->linesSoFar);
				if (it->isComplete) {
					vertexBufferPosition += 1 + it->linesSoFar;
					it->linesSoFar = 0;
				} else {
					// we'll duplicate the last one vertex into the new buffer
					vertexBufferPosition += it->linesSoFar;
					it->linesSoFar = 0;
					if (!loggedDrawingOperationsOnce) {
						logwrap(fprintf(logfile, "drawAllOutlines: erasing %u lines\n", it - preparedOutlines.begin()));
					}
					preparedOutlines.erase(preparedOutlines.begin(), it);
					return false;
				}
			} else if (!it->isComplete) {
				drawOutlinesSection(false);
				if (!loggedDrawingOperationsOnce) {
					logwrap(fprintf(logfile, "drawAllOutlines: erasing %u lines\n", it - preparedOutlines.begin()));
				}
				preparedOutlines.erase(preparedOutlines.begin(), it);
				return false;
			} else {
				logwrap(fputs("Suspicious outline behavior in drawAllOutlines\n", logfile));
			}
		} else {
			if (it->linesSoFar) {
				if (outlinesSectionEmpty) {
					if (!loggedDrawingOperationsOnce) {
						logwrap(fprintf(logfile, "drawAllOutlines: starting new outlines section\n"));
					}
					outlinesSectionEmpty = false;
					outlinesSectionOutlineCount = 1;
					outlinesSectionTotalLineCount = it->linesSoFar;
				} else {
					++outlinesSectionOutlineCount;
					outlinesSectionTotalLineCount += it->linesSoFar;
				}
				it->linesSoFar = 0;
				if (!it->isComplete) {
					drawOutlinesSection(true);
					if (!loggedDrawingOperationsOnce) {
						logwrap(fprintf(logfile, "drawAllOutlines: erasing %u lines\n", it - preparedOutlines.begin()));
					}
					preparedOutlines.erase(preparedOutlines.begin(), it);
					return false;
				}
			} else if (!it->isComplete) {
				drawOutlinesSection(false);
				if (it->hasPadding) {
					it->hasPadding = false;
					vertexBufferPosition += 2;
				}
				if (!loggedDrawingOperationsOnce) {
					logwrap(fprintf(logfile, "drawAllOutlines: erasing %u lines\n", it - preparedOutlines.begin()));
				}
				preparedOutlines.erase(preparedOutlines.begin(), it);
				return false;
			}
		}
	}
	drawOutlinesSection(false);

	preparedOutlines.clear();
	return true;
}

void Graphics::drawAllPoints() {
	if (!numberOfPointsPrepared) return;
	sendAllPreparedVertices();
	advanceRenderState(RENDER_STATE_DRAWING_POINTS);

	for (unsigned int i = numberOfPointsPrepared; i != 0; --i) {
		device->DrawPrimitive(D3DPT_TRIANGLESTRIP, vertexBufferPosition, 8);
		vertexBufferPosition += 10;
		device->DrawPrimitive(D3DPT_LINELIST, vertexBufferPosition, 2);
		vertexBufferPosition += 4;
	}

	numberOfPointsPrepared = 0;

}

void Graphics::drawAllTexturedBoxes() {
	if (!textureVertexBufferLength) return;
	sendAllPreparedTextureVertices();
	advanceRenderState(RENDER_STATE_DRAWING_TEXTURES);

	device->DrawPrimitive(D3DPT_TRIANGLESTRIP, 0, textureVertexBufferLength - 2);

	resetTextureVertexBuffer();

}
