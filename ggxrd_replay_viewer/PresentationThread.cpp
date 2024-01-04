#include "framework.h"
#include "PresentationThread.h"
#include "PresentationInput.h"
#include "debugMsg.h"
#include <vector>
#include "EncodedImageSize.h"
#include "PresentationTimer.h"
#include "WinError.h"
#include <timeapi.h>
#include "ReplayFileData.h"
#include <algorithm>
#include "Layout.h"
#include "InputsDrawing.h"

using namespace std::literals;

extern HANDLE eventApplicationIsClosing;
extern HWND mainWindowHandle;
extern void onReachPlaybackEnd();
extern bool rectsIntersect(const RECT* a, const RECT* b);
extern HDC hdcMem;
extern std::mutex hdcMemMutex;

void PresentationThread::threadLoop() {
    DWORD nextWaitTime = INFINITE;

    if (!initializeDirect3D()) {
        return;
    }

    while (true) {
        HANDLE events[3] = { eventApplicationIsClosing, presentationInput.event, presentationTimer.eventForPresentationThread };
        DWORD waitResult = WaitForMultipleObjects(3, events, FALSE, nextWaitTime);
        nextWaitTime = INFINITE;
        if (waitResult == WAIT_OBJECT_0) {
            return;
        }
        else if (waitResult == WAIT_OBJECT_0 + 1 || waitResult == WAIT_OBJECT_0 + 2 || waitResult == WAIT_TIMEOUT) {
            bool startedScrubbing = false;
            bool stoppedScrubbing = false;
            bool gotNewSamples = false;
            bool redrawFrameRequest = false;
            bool redrawTextRequest = false;
            if (waitResult != WAIT_TIMEOUT) {
                std::unique_lock<std::mutex> presentationGuard(presentationInput.mutex);
                if (!scrubbing && presentationInput.scrubbing
                        || scrubbing && presentationInput.scrubbing && scrubbingMatchCounter != presentationInput.scrubbingMatchCounter) {
                    startedScrubbing = true;
                    requestPendingToEraseObsoleteFrames = false;
                    isFirstPresentation = true;
                }
                if (!presentationInput.scrubbing && scrubbing) {
                    stoppedScrubbing = true;
                }
                if (presentationInput.needReportScrubbing) {
                    needReportScrubbing = true;
                    presentationInput.needReportScrubbing = false;
                }
                if (presentationInput.needReportNotScrubbing) {
                    needReportNotScrubbing = true;
                    presentationInput.needReportNotScrubbing = false;
                }
                if (presentationInput.needRedrawFrame) {
                    redrawFrameRequest = true;
                    presentationInput.needRedrawFrame = false;
                }
                if (presentationInput.needRedrawText) {
                    redrawTextRequest = true;
                    presentationInput.needRedrawText = false;
                }
                scrubbing = presentationInput.scrubbing;
                scrubbingMatchCounter = presentationInput.scrubbingMatchCounter;
                unsigned int entriesCounter = presentationInput.entriesCount;
                if (entriesCounter) {
                    if (startedScrubbing) {
                        requestPendingToEraseObsoleteFrames = false;
                        samples.clear();
                        logics.clear();
                    }
                    gotNewSamples = true;
                    auto entriesIt = presentationInput.entries.begin();
                    while (entriesCounter--) {
                        PresentationInputEntry& entry = *entriesIt;
                        samples.insert(samples.end(), entry.samples.begin(), entry.samples.end());
                        logics.insert(logics.end(), entry.logicInfo.begin(), entry.logicInfo.end());
                        ++entriesIt;
                    }
                    presentationInput.clear();
                }
            }
            if (gotNewSamples) {
                std::sort(samples.begin(), samples.end(), [](SampleWithBufferRef& a, SampleWithBufferRef& b) {
                    return a->matchCounter > b->matchCounter;
                });
                std::sort(logics.begin(), logics.end(), [](LogicInfoRef& a, LogicInfoRef& b) {
                    return a->matchCounter > b->matchCounter;
                });
                removeDuplicateSamples();
                removeDuplicateLogics();
            }
            if (waitResult != WAIT_TIMEOUT) {
                std::unique_lock<std::mutex> presentationTimerGuard(presentationTimer.mutex);
                isPlaying = presentationTimer.isPlaying;
                if (presentationTimer.scrubbing && !scrubbing) {
                    continue;
                }
                if (startedScrubbing) {
                    presentationTimer.reachedEnd = false;
                }
            }
            if (stoppedScrubbing) {
                requestPendingToEraseObsoleteFrames = true;
            }
            if (requestPendingToEraseObsoleteFrames && isPlaying) {
                requestPendingToEraseObsoleteFrames = false;
                {
                    auto it = samples.begin();
                    for (; it != samples.end(); ++it) {
                        if (it->ptr->matchCounter <= samplesMatchCounter) break;
                    }
                    samples.erase(samples.begin(), it);
                }

                {
                    auto it = logics.begin();
                    for (; it != logics.end(); ++it) {
                        if (it->ptr->matchCounter <= matchCounter) break;
                    }
                    logics.erase(logics.begin(), it);
                }
            }
            if (needSwitchFiles) {
                logics.clear();
                samples.clear();
                matchCounter = 0;
                samplesMatchCounter = 0;
                needSwitchFiles = false;
                isFirstPresentation = true;
                requestPendingToEraseObsoleteFrames = false;
                SetEvent(eventResponse);
                continue;
            }
            if (needReportNotScrubbing && !scrubbing) {
                needReportNotScrubbing = false;
                SetEvent(eventResponse);
            }
            if (!isFirstPresentation && (!isPlaying || scrubbing)) {
                if (redrawFrameRequest || redrawTextRequest) {
                    if (!drawFrame(redrawFrameRequest, false, nullptr, nullptr, false, nullptr, redrawTextRequest)) return;
                }
                continue;
            }
            redrawFrameRequest = false;
            redrawTextRequest = false;
            resizeSurfaces();
            bool needAbruptStop = false;
            auto logicIt = logics.begin();
            if (!logics.empty() && scrubbing) {
                LogicInfoRef& logicRef = *logicIt;
                unsigned int firstMatchCounter = logicRef->matchCounter;
                unsigned int diff = 0;
                if (scrubbingMatchCounter <= firstMatchCounter) {
                    diff = firstMatchCounter - scrubbingMatchCounter;
                    if (diff < logics.size()) {
                        logicIt += diff;
                    }
                }
            }
            LogicInfoRef logicRefToDraw;
            bool logicRefIsNull = true;
            if (logicIt != logics.end()) {
                logicRefIsNull = false;
                logicRefToDraw = *logicIt;
                matchCounter = logicRefToDraw->matchCounter;
                /*if (logicRefToDraw->isEof) {
                    matchCounter = (0xFFFFFFFF - replayFileData.header.logicsCount) + 1;
                }*/
                if (!scrubbing) {
                    logics.erase(logicIt);
                }
            }
            auto samplesIt = samples.begin();
            auto maxSamplesIt = samples.end();
            if (scrubbing) {
                while (samplesIt != samples.end()) {
                    SampleWithBufferRef& swbRef = *samplesIt;
                    if (swbRef->matchCounter < scrubbingMatchCounter) {
                        break;
                    }
                    maxSamplesIt = samplesIt;
                    ++samplesIt;
                }
                if (maxSamplesIt == samples.end() && !samples.empty()) {
                    maxSamplesIt = samples.begin();
                }
            } else {
                while (samplesIt != samples.end()) {
                    SampleWithBufferRef& swbRef = *samplesIt;
                    if (swbRef->matchCounter < matchCounter) {
                        break;
                    }
                    maxSamplesIt = samplesIt;
                    ++samplesIt;
                }
                if (maxSamplesIt == samples.end() && !samples.empty()) {
                    maxSamplesIt = samples.begin();
                }
            }
            bool needDrawFrame = false;
            if (maxSamplesIt != samples.end()) {
                SampleWithBufferRef& swbRef = *maxSamplesIt;
                if (isFirstPresentation || swbRef->matchCounter != samplesMatchCounter) {
                    samplesMatchCounter = swbRef->matchCounter;
                    needDrawFrame = true;
                }
            }
            if (needDrawFrame) {
                lastDrawnFrame = *maxSamplesIt;
                if (!updateSurface()) {
                    return;
                }
            }
            if (!drawFrame(true, !scrubbing, nullptr, nullptr, !logicRefIsNull, logicRefToDraw, true)) return;
            if (needDrawFrame) {
                isFirstPresentation = false;
            }
            if (!logicRefIsNull || needDrawFrame) {
                {
                    std::unique_lock<std::mutex> presentationTimeGuard(presentationTimer.mutex);
                    if (!scrubbing && presentationTimer.scrubbing) {
                        needAbruptStop = true;
                        continue;
                    }
                    presentationTimer.matchCounter = matchCounter;
                    /*if (swbRef->matchCounter == replayFileData.header.logicsCount && isPlaying && !scrubbing && !presentationTimer.reachedEnd) {
                        presentationTimer.reachedEnd = true;
                        presentationTimer.isPlaying = false;
                        onReachPlaybackEnd();
                    }*/
                    SetEvent(presentationTimer.eventForFileReader);
                }

                if (!scrubbing && needDrawFrame && !isFirstPresentation) {
                    auto erasingSampleIt = samples.begin();
                    for (; erasingSampleIt != samples.end(); ++erasingSampleIt) {
                        if ((*erasingSampleIt)->matchCounter <= samplesMatchCounter) break;
                    }
                    samples.erase(samples.begin(), erasingSampleIt);
                }
                if (isPlaying) {
                    nextWaitTime = 0;
                }
            }
            if (startedScrubbing && !needDrawFrame) {
                debugMsg("oops\n");
            }

            /*if (!needAbruptStop
                    && !playedSample
                    && isPlaying
                    && !scrubbing) {
                std::unique_lock<std::mutex> presentationTimeGuard(presentationTimer.mutex);
                if (presentationTimer.matchCounter == replayFileData.header.lastFrameMatchCounter && !presentationTimer.reachedEnd) {
                    presentationTimer.reachedEnd = true;
                    presentationTimer.isPlaying = false;
                    onReachPlaybackEnd();
                }
            }*/
            if (needReportScrubbing && scrubbing) {
                needReportScrubbing = false;
                SetEvent(eventResponse);
            }
        }
        else {
            debugMsg("PresentationThread::threadLoop abnormal result (in decimal): %u\n", waitResult);
        }
    }
}

bool PresentationThread::initializeDirect3D() {
    if (d3dDevice) return true;
    CComPtr<IDirect3D9> d3d = Direct3DCreate9(D3D_SDK_VERSION);
    if (!d3d) return false;

    d3dPresentParameters.Windowed = TRUE;
    d3dPresentParameters.SwapEffect = D3DSWAPEFFECT_COPY;
    d3dPresentParameters.Flags = D3DPRESENTFLAG_LOCKABLE_BACKBUFFER;
    d3dPresentParameters.hDeviceWindow = mainWindowHandle;

    HRESULT errorCode = d3d->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, mainWindowHandle,
        D3DCREATE_HARDWARE_VERTEXPROCESSING,
        &d3dPresentParameters, &d3dDevice);
    if (FAILED(errorCode)) {
        debugMsg("IDirect3D9->CreateDevice failed: %.8x\n", errorCode);
        return false;
    }

    errorCode = d3dDevice->GetRenderTarget(0, &renderTarget);
    if (FAILED(errorCode)) {
        debugMsg("IDirect3DDevice9->GetRenderTarget failed: %.8x\n", errorCode);
        return false;
    }

    errorCode = renderTarget->GetDesc(&renderTargetDesc);
    if (FAILED(errorCode)) {
        debugMsg("IDirect3DSurface9->GetDesc failed: %.8x\n", errorCode);
        return false;
    }

    errorCode = d3d->CheckDeviceFormatConversion(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, D3DFMT_YUY2, renderTargetDesc.Format);
    if (errorCode == D3DERR_NOTAVAILABLE) {
        debugMsg("We don't have the color-space conversion support.\n");
        return false;
    }
    else if (FAILED(errorCode)) {
        debugMsg("IDirect3D9->CheckDeviceFormatConversion failed: %.8x\n", errorCode);
        return false;
    }

    return true;
}

bool PresentationThread::resizeSurfaces() {
    HRESULT errorCode;
    if (videoWidth == encodedImageWidth && videoHeight == encodedImageHeight) return true;
    if (videoSurfaceSystemMemory) {
        D3DSURFACE_DESC desc;
        videoSurfaceSystemMemory->GetDesc(&desc);
        if (desc.Width != encodedImageWidth || desc.Height != encodedImageHeight) {
            videoSurfaceSystemMemory = nullptr;
        }
    }
    if (!videoSurfaceSystemMemory) {

        errorCode = d3dDevice->CreateOffscreenPlainSurface(encodedImageWidth, encodedImageHeight, D3DFMT_YUY2, D3DPOOL_SYSTEMMEM, &videoSurfaceSystemMemory, NULL);
        if (FAILED(errorCode)) {
            debugMsg("IDirect3DDevice9->CreateOffscreenPlainSurface (videoSurfaceSystemMemory) failed: %.8x\n", errorCode);
            return false;
        }

    }

    if (videoSurfaceDefaultMemory) {
        D3DSURFACE_DESC desc;
        videoSurfaceDefaultMemory->GetDesc(&desc);
        if (desc.Width != encodedImageWidth || desc.Height != encodedImageHeight) {
            videoSurfaceDefaultMemory = nullptr;
        }
    }
    if (!videoSurfaceDefaultMemory) {

        errorCode = d3dDevice->CreateOffscreenPlainSurface(encodedImageWidth, encodedImageHeight, D3DFMT_YUY2, D3DPOOL_DEFAULT, &videoSurfaceDefaultMemory, NULL);
        if (FAILED(errorCode)) {
            debugMsg("IDirect3DDevice9->CreateOffscreenPlainSurface (videoSurfaceDefaultMemory) failed: %.8x\n", errorCode);
            return false;
        }

    }

    return true;
}

bool PresentationThread::updateSurface() {
    D3DLOCKED_RECT lockedRect;
    HRESULT errorCode = videoSurfaceSystemMemory->LockRect(&lockedRect, NULL, NULL);
    if (FAILED(errorCode)) {
        debugMsg("videoSurfaceSystemMemory->LockRect failed: %.8x\n", errorCode);
        return false;
    }

    BYTE* bufRect = nullptr;
    DWORD bufMaxLength = 0;
    DWORD bufCurrentLength = 0;
    errorCode = lastDrawnFrame->buffer->Lock(&bufRect, &bufMaxLength, &bufCurrentLength);
    if (FAILED(errorCode)) {
        debugMsg("lastDrawnFrame->buffer->Lock(&bufRect, &bufMaxLength, &bufCurrentLength) failed: %.8x\n", errorCode);
        return false;
    }

    if (bufCurrentLength < encodedImageHeight * 2 * encodedImageWidth) return false;

    const unsigned int pitch = lockedRect.Pitch;
    DWORD* src = (DWORD*)bufRect;
    DWORD* dest = (DWORD*)lockedRect.pBits;
    for (unsigned int heightCounter = encodedImageHeight; heightCounter != 0; --heightCounter) {
        memcpy(dest, src, 2 * encodedImageWidth);
        src += (encodedImageWidth >> 1);
        dest = (DWORD*)((char*)dest + pitch);
    }

    errorCode = lastDrawnFrame->buffer->Unlock();
    if (FAILED(errorCode)) {
        debugMsg("buffer->Unlock() failed: %.8x\n", errorCode);
        return false;
    }

    errorCode = videoSurfaceSystemMemory->UnlockRect();
    if (FAILED(errorCode)) {
        debugMsg("videoSurfaceSystemMemory->UnlockRect failed: %.8x\n", errorCode);
        return false;
    }

    errorCode = d3dDevice->UpdateSurface(videoSurfaceSystemMemory, NULL, videoSurfaceDefaultMemory, NULL);
    if (FAILED(errorCode)) {
        debugMsg("d3dDevice->UpdateSurface (videoSurfaceSystemMemory to videoSurfaceDefaultMemory) failed: %.8x\n", errorCode);
        return false;
    }
    return true;
}

void PresentationThread::onWmPaint(const PAINTSTRUCT& paintStruct, HDC hdc) {
    if (!videoSurfaceDefaultMemory) return;
    RECT videoRectLocal;
    RECT seekbarRectLocal;
    layout.getVideoAndSeekbar(&videoRectLocal, &seekbarRectLocal);
    RECT textRect;
    layout.getAnything(&textRect, &layout.text);
    const bool willNeedRedrawFrame = rectsIntersect(&paintStruct.rcPaint, &videoRectLocal);
    const bool willNeedRedrawText = rectsIntersect(&paintStruct.rcPaint, &textRect);
    if (willNeedRedrawFrame || willNeedRedrawText) {
        std::unique_lock<std::mutex> presentationInputGuard(presentationInput.mutex);
        presentationInput.needRedrawFrame = presentationInput.needRedrawFrame || willNeedRedrawFrame;
        presentationInput.needRedrawText = presentationInput.needRedrawText || willNeedRedrawText;
        SetEvent(presentationInput.event);
    }
}

bool PresentationThread::drawFrame(bool needDrawFrame, bool allowSleep, const RECT* videoRect, const RECT* seekbarRect, bool drawLogic, LogicInfoRef logicInfo, bool drawText) {
    bool thisIsTheSecondIteration = false;
    while (true) {
        RECT videoRectLocal;
        RECT seekbarRectLocal;
        if (!videoRect || !seekbarRect) {
            layout.getVideoAndSeekbar(&videoRectLocal, &seekbarRectLocal);
            videoRect = &videoRectLocal;
            seekbarRect = &seekbarRectLocal;
        }

        if (needDrawFrame) {
            HRESULT errorCode = d3dDevice->BeginScene();
            if (FAILED(errorCode)) {
                debugMsg("d3dDevice->BeginScene failed: %.8x\n", errorCode);
                return false;
            }

            RECT destRect;
            destRect.left = 0;
            destRect.top = 0;
            destRect.right = renderTargetDesc.Width;
            destRect.bottom = renderTargetDesc.Height - seekbarHeight(videoRect, seekbarRect);
            errorCode = d3dDevice->StretchRect(videoSurfaceDefaultMemory, NULL, renderTarget, NULL, D3DTEXF_LINEAR);
            if (FAILED(errorCode)) {
                debugMsg("d3dDevice->StretchRect (videoSurfaceDefaultMemory) failed: %.8x\n", errorCode);
                return false;
            }

            drawSeekbar(videoRect, seekbarRect);

            errorCode = d3dDevice->EndScene();
            if (FAILED(errorCode)) {
                debugMsg("d3dDevice->EndScene failed: %.8x\n", errorCode);
                return false;
            }

            std::chrono::steady_clock::time_point currentTime = std::chrono::steady_clock::now();

            if (allowSleep && !isFirstPresentation && currentTime - lastPresentationTime < 16ms) {
                bool needToCancelTimePeriod = (timeBeginPeriod(3) == TIMERR_NOERROR);
                Sleep((DWORD)(std::chrono::duration_cast<std::chrono::milliseconds>(16ms - (currentTime - lastPresentationTime))).count());
                if (needToCancelTimePeriod) {
                    timeEndPeriod(3);
                }
            }

            if (allowSleep) {
                lastPresentationTime = std::chrono::steady_clock::now();
            }

            RECT totalRect;
            totalRect.top = videoRect->top;
            totalRect.left = videoRect->left;
            totalRect.right = videoRect->right;
            totalRect.bottom = seekbarRect->bottom;

            errorCode = d3dDevice->Present(NULL, &totalRect, NULL, NULL);
            if (errorCode == D3DERR_DEVICELOST) {
                if (thisIsTheSecondIteration) {
                    debugMsg("d3dDevice->Present returned D3DERR_DEVICELOST a second time\n");
                    return false;
                }

                videoSurfaceSystemMemory = nullptr;
                videoSurfaceDefaultMemory = nullptr;
                renderTarget = nullptr;

                errorCode = d3dDevice->Reset(&d3dPresentParameters);
                if (FAILED(errorCode)) {
                    debugMsg("d3dDevice->Reset failed: %.8x\n", errorCode);
                    return false;
                }

                errorCode = d3dDevice->GetRenderTarget(0, &renderTarget);
                if (FAILED(errorCode)) {
                    debugMsg("IDirect3DDevice9->GetRenderTarget failed: %.8x\n", errorCode);
                    return false;
                }

                errorCode = renderTarget->GetDesc(&renderTargetDesc);
                if (FAILED(errorCode)) {
                    debugMsg("IDirect3DSurface9->GetDesc failed: %.8x\n", errorCode);
                    return false;
                }

                resizeSurfaces();

                if (!updateSurface()) return false;

                thisIsTheSecondIteration = true;
                continue;
            }
            if (FAILED(errorCode)) {
                debugMsg("d3dDevice->Present failed: %.8x\n", errorCode);
                return false;
            }
        }

        if (drawLogic && logicInfo.ptr) {
            for (int i = 0; i < 2; ++i) {
                inputsDrawing.panels[i].followToInputBufferIndexCrossthread(logicInfo->replayStates[i].index);
            }
            lastTextMatchCounter = logicInfo->matchCounter;
            {
                std::unique_lock<std::mutex> indexGuard(replayFileData.indexMutex);
                lastTextLastMatchCounter = 0xFFFFFFFF - (unsigned int)(replayFileData.index.size() - 1);
            }
            textArena.clear();
            textArena += std::to_wstring(0xFFFFFFFF - lastTextMatchCounter + 1);
            textArena += L" / ";
            textArena += std::to_wstring(0xFFFFFFFF - lastTextLastMatchCounter + 1);
        }

        if (drawText) {
            RECT textRect;
            layout.getAnything(&textRect, &layout.text);
            HDC hdc = GetDC(mainWindowHandle);
            DrawText(hdc, textArena.c_str(), (int)textArena.size(), &textRect, DT_SINGLELINE);
            ReleaseDC(mainWindowHandle, hdc);
        }


        return true;
    }
    return false;
}

void PresentationThread::drawSeekbar(const RECT* videoRect, const RECT* seekbarRect) {
    d3dDevice->SetRenderState(D3DRS_STENCILENABLE, FALSE);
    d3dDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
    d3dDevice->SetVertexShader(nullptr);
    d3dDevice->SetPixelShader(nullptr);
    d3dDevice->SetFVF(D3DFVF_XYZRHW | D3DFVF_DIFFUSE);
    d3dDevice->SetTexture(0, nullptr);

    const unsigned char height = seekbarHeight(videoRect, seekbarRect);
    unsigned int seekbarWidth;
    if (matchCounter == 0) {
        seekbarWidth = renderTargetDesc.Width;
    } else {
        seekbarWidth = (unsigned int)(
            (unsigned long long)renderTargetDesc.Width * (unsigned long long)(0xFFFFFFFF - matchCounter + 1) / replayFileData.header.logicsCount
        );
    }

    const float seekbarTopFloat = (float)(renderTargetDesc.Height - height);
    const D3DCOLOR seekbarBgClr = D3DCOLOR_XRGB(0x99, 0x99, 0x99);
    const D3DCOLOR seekbarFillClr = D3DCOLOR_XRGB(0xFF, 0x00, 0x00);
    const float renderTargetHeightFloat = (float)(renderTargetDesc.Height);
    const float renderTargetWidthFloat = (float)(renderTargetDesc.Width);
    const float seekbarWidthFloat = (float)seekbarWidth;

    Vertex buffer[] {
        Vertex { 0.F,                    seekbarTopFloat,         0.F, 1.F, seekbarBgClr },
        Vertex { renderTargetWidthFloat, seekbarTopFloat,         0.F, 1.F, seekbarBgClr },
        Vertex { 0.F,                    renderTargetHeightFloat, 0.F, 1.F, seekbarBgClr },
        Vertex { renderTargetWidthFloat, renderTargetHeightFloat, 0.F, 1.F, seekbarBgClr },
        Vertex { renderTargetWidthFloat, renderTargetHeightFloat, 0.F, 1.F, seekbarBgClr },
        Vertex { 0.F,                    seekbarTopFloat,         0.F, 1.F, seekbarFillClr },
        Vertex { 0.F,                    seekbarTopFloat,         0.F, 1.F, seekbarFillClr },
        Vertex { seekbarWidthFloat,      seekbarTopFloat,         0.F, 1.F, seekbarFillClr },
        Vertex { 0.F,                    renderTargetHeightFloat, 0.F, 1.F, seekbarFillClr },
        Vertex { seekbarWidthFloat,      renderTargetHeightFloat, 0.F, 1.F, seekbarFillClr }
    };

    d3dDevice->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 8, buffer, sizeof(Vertex));
}

PresentationThread::Vertex::Vertex(float x, float y, float z, float rhw, D3DCOLOR color)
    : x(x), y(y), z(z), rhw(rhw), color(color) { }

void PresentationThread::removeDuplicateSamples() {
    unsigned int prevMatchCounter = 0;
    bool isFirst = true;
    auto it = samples.begin();
    while (it != samples.end()) {
        if (isFirst) {
            prevMatchCounter = it->ptr->matchCounter;
            ++it;
            isFirst = false;
        }
        else if (prevMatchCounter == it->ptr->matchCounter) {
            it = samples.erase(it);
        }
        else {
            prevMatchCounter = it->ptr->matchCounter;
            ++it;
        }
    }
}

void PresentationThread::removeDuplicateLogics() {
    unsigned int prevMatchCounter = 0;
    bool isFirst = true;
    auto it = logics.begin();
    while (it != logics.end()) {
        if (isFirst) {
            prevMatchCounter = it->ptr->matchCounter;
            ++it;
            isFirst = false;
        }
        else if (prevMatchCounter == it->ptr->matchCounter) {
            it = logics.erase(it);
        }
        else {
            prevMatchCounter = it->ptr->matchCounter;
            ++it;
        }
    }
}

void PresentationThread::destroy() {
    samples.clear();
    logics.clear();
}

unsigned char PresentationThread::seekbarHeight(const RECT* videoRect, const RECT* seekbarRect) const {
    unsigned int totalVideoHeight = seekbarRect->bottom - videoRect->top;
    unsigned int renderTargetHeight = renderTargetDesc.Height;
    return (unsigned char)((seekbarRect->bottom - seekbarRect->top) * renderTargetHeight / totalVideoHeight);
}
