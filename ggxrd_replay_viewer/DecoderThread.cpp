#include "framework.h"
#include "DecoderThread.h"
#include "debugMsg.h"
#include "DecoderInput.h"
#include "Codec.h"
#include <vector>
#include "MemoryProvisioner.h"
#include "WinError.h"

extern HANDLE eventApplicationIsClosing;

void DecoderThread::threadLoop() {

    HRESULT errorCode;

    drainingSample.resize(codec.decoderOutputStreamInfo.cbAlignment, codec.decoderOutputStreamInfo.cbSize);
    std::vector<SampleWithBufferRef> outputSamples;

	while (true) {
        HANDLE events[2] = { eventApplicationIsClosing, decoderInput.event };
        DWORD waitResult = WaitForMultipleObjects(2, events, FALSE, INFINITE);
        if (waitResult == WAIT_OBJECT_0) {
            return;
        }
        else if (waitResult == WAIT_OBJECT_0 + 1) {
            bool needContinue = false;
            do {
                needContinue = false;
                bool needStartScrubbing = false;
                bool needStopScrubbing = false;
                {
                    std::unique_lock<std::mutex> queueGuard(decoderInput.mutex);
                    if (!decoderInput.scrubbing && scrubbing) {
                        needStopScrubbing = true;
                        scrubbing = false;
                    } else {
                        needStartScrubbing = decoderInput.scrubbing && !scrubbing
                            || decoderInput.scrubbing && scrubbing && scrubbingMatchCounter != decoderInput.scrubbingMatchCounter;
                        scrubbing = decoderInput.scrubbing;
                        scrubbingMatchCounter = decoderInput.scrubbingMatchCounter;
                    }
                    if (needSwitchFiles) {
                        scrubbing = false;
                        decoderInput.clear();
                        needSwitchFiles = false;
                        SetEvent(eventResponse);
                        break;
                    }
                    decoderInput.getFirstEntry(entry);
                }

                if (!entry.samples.empty() || !entry.logics.empty()) {
                    needContinue = true;
                    if (!entry.samples.empty()) {
                        for (SampleWithBufferRef& ref : entry.samples) {
                            if (!codec.decodeInput(ref->sample)) {
                                crashed = true;
                                return;
                            }
                        }

                        for (SampleWithBufferRef& ref : entry.samples) {
                            SampleWithBufferRef newRef = memoryProvisioner.createDecoderOutputSample();
                            errorCode = newRef->buffer->SetCurrentLength(0);
                            if (FAILED(errorCode)) {
                                debugMsg("newRef->buffer->SetCurrentLength(0) failed: %.8x\n", errorCode);
                                return;
                            }
                            //debugMsg("Decoder got matchCounter: %u\n", ref->matchCounter);
                            if (!codec.decoderOutput(newRef->sample)) {
                                crashed = true;
                                return;
                            }
                            newRef->matchCounter = ref->matchCounter;
                            newEntry.samples.push_back(std::move(newRef));
                        }
                        if (!codec.decoderOutput(drainingSample.sample)) {
                            crashed = true;
                            return;
                        }
                    }
                    if (!entry.logics.empty()) {
                        newEntry.logicInfo.insert(newEntry.logicInfo.begin(), entry.logics.begin(), entry.logics.end());
                    }

                    {
                        std::unique_lock<std::mutex> presentationGuard(presentationInput.mutex);
                        if (!newEntry.logicInfo.empty() || !newEntry.samples.empty()) {
                            if (!presentationInput.scrubbing && scrubbing
                                    || presentationInput.scrubbing && scrubbing && presentationInput.scrubbingMatchCounter != scrubbingMatchCounter) {
                                presentationInput.clear();
                            }
                            presentationInput.addEntry(newEntry);
                        }
                        presentationInput.scrubbing = scrubbing;
                        presentationInput.scrubbingMatchCounter = scrubbingMatchCounter;
                        SetEvent(presentationInput.event);
                    }
                    newEntry.logicInfo.clear();
                    newEntry.samples.clear();
                } else if (needStopScrubbing || needStartScrubbing) {
                    std::unique_lock<std::mutex> presentationGuard(presentationInput.mutex);
                    presentationInput.scrubbing = scrubbing;
                    presentationInput.scrubbingMatchCounter = scrubbingMatchCounter;
                    SetEvent(presentationInput.event);
                }
                needStopScrubbing = false;
                needStartScrubbing = false;
                entry.logics.clear();
                entry.samples.clear();

            } while (needContinue);
        } else {
            debugMsg("DecoderThread::threadLoop abnormal result (in decimal): %u\n", waitResult);
        }
	}
}
