#include "framework.h"
#include "EncoderThread.h"
#include "EncoderInput.h"
#include "debugMsg.h"
#include "Codec.h"
#include "FileWriterInput.h"
#include "MemoryProvisioner.h"
#include <mutex>
#include "EncodedImageSize.h"

extern HANDLE eventApplicationIsClosing;

void EncoderThread::threadLoop() {
    
    class DestructionCode {
    public:
        DestructionCode(EncoderThread* thisArg) : thisArg(thisArg) { }
        ~DestructionCode() {
            while (thisArg->frameCount--) {
                thisArg->frames[thisArg->frameCount].clear();
            }
            thisArg->inputSamples.clear();
            thisArg->outputSamples.clear();
            thisArg->logics.clear();
            thisArg->drainingSample.buffer = nullptr;
            thisArg->drainingSample.sample = nullptr;
        }
        EncoderThread* thisArg = nullptr;
    } destructionCode(this);

    drainingSample.resize(codec.encoderOutputStreamInfo.cbAlignment, codec.encoderOutputStreamInfo.cbSize);

    HRESULT errorCode;
    for (SampleWithBuffer& swb : inputSamples) {
        yuvSize = encodedImageHeight * encodedImageWidth + (encodedImageHeight * encodedImageWidth / 4) * 2;
        swb.resize(0, yuvSize);
        errorCode = swb.sample->SetSampleDuration(166666);
        if (FAILED(errorCode)) {
            debugMsg("swb.sample->SetSampleDuration failed: %.8x\n", errorCode);
            return;
        }
    }
    for (unsigned int i = 0; i < 8; ++i) {
        outputSamples.push_back(memoryProvisioner.createEncoderOutputSample());
    }

    while (true) {
        HANDLE events[2] = { eventApplicationIsClosing, encoderInput.event };
        DWORD waitResult = WaitForMultipleObjects(2, events, FALSE, INFINITE);
        if (waitResult == WAIT_OBJECT_0) {
            return;
        }
        else if (waitResult == WAIT_OBJECT_0 + 1) {
            bool processExited = false;
            {
                std::unique_lock<std::mutex> guard(encoderInput.mutex);
                encoderInput.retrieveLogics(logics);
                encoderInput.retrieveFrames(frames, frameCount);
                if (encoderInput.processExited) {
                    processExited = true;
                    encoderInput.processExited = false;
                }
            }
            if (processExited) {
                reset();
                std::unique_lock<std::mutex> fileInputGuard(fileWriterInput.mutex);
                fileWriterInput.processExited = true;
                SetEvent(fileWriterInput.event);
                continue;
            }
            HRESULT errorCode;
            EncoderInputFrameRef* frame = frames;
            for (unsigned int frameCounter = frameCount; frameCounter != 0 && inputSamplesCount < 8; --frameCounter) {
                SampleWithBuffer& swb = inputSamples[inputSamplesCount++];
                const EncoderInputFrame& fr = *frame->ptr;
                swb.matchCounter = fr.matchCounter;
                swb.empty = fr.empty;
                swb.isEof = fr.isEof;
                if (fr.isEof) {
                    eofEncountered = true;
                    debugMsg("Encoder thread sees eof image, its match counter is: %u\n", fr.matchCounter);
                }
                if (!fr.empty) {
                    ++nonEmptyInputSamplesCount;
                    BYTE* bufPtr = nullptr;
                    DWORD bufMaxLen = 0;
                    DWORD bufCurrentLen = 0;
                    errorCode = swb.buffer->Lock(&bufPtr, &bufMaxLen, &bufCurrentLen);
                    if (FAILED(errorCode)) {
                        debugMsg("swb.buffer->Lock failed: %.8x\n", errorCode);
                        return;
                    }
                    scaler.resize(fr.pitch, fr.width, fr.height);
                    scaler.process(bufPtr, fr.data);
                    errorCode = swb.buffer->SetCurrentLength(yuvSize);
                    if (FAILED(errorCode)) {
                        debugMsg("swb.buffer->SetCurrentLength failed: %.8x\n", errorCode);
                        return;
                    }
                    swb.currentSize = yuvSize;
                    errorCode = swb.buffer->Unlock();
                    if (FAILED(errorCode)) {
                        debugMsg("swb.buffer->Unlock failed: %.8x\n", errorCode);
                        return;
                    }
                    errorCode = swb.sample->SetSampleTime((UINT64)(0xFFFFFFFF - fr.matchCounter) * (UINT64)166666);
                    if (FAILED(errorCode)) {
                        debugMsg("swb.sample->SetSampleTime failed: %.8x\n", errorCode);
                        return;
                    }
                    if (!codec.encodeInput(swb.sample)) {
                        crashed = true;
                        return;
                    }
                } else {
                    debugMsg("EncoderThread sees empty frame\n");
                }
                ++frame;
            }
            clearFrames();
            if (eofEncountered && !outputSamplesContainsEof) {
                outputSamplesContainsEof = true;
                debugMsg("Encoder thread transferred eof into output samples\n");
            }
            if (!outputSamplesCount) {
                if (inputSamplesCount == 8 || eofEncountered) {
                    auto swbInput = inputSamples.begin();
                    while (inputSamplesCount) {
                        --inputSamplesCount;
                        if (!swbInput->empty) {
                            SampleWithBufferRef& swb = outputSamples[outputSamplesCount++];
                            swb->matchCounter = swbInput->matchCounter;
                            errorCode = swb->buffer->SetCurrentLength(0);
                            if (FAILED(errorCode)) {
                                debugMsg("swb.buffer->SetCurrentLength(0) failed: %.8x\n", errorCode);
                                return;
                            }
                            if (!codec.encoderOutput(swb->sample)) {
                                crashed = true;
                                return;
                            }
                            errorCode = swb->buffer->GetCurrentLength(&swb->currentSize);
                            if (FAILED(errorCode)) {
                                debugMsg("swb.buffer->GetCurrentLength(...) failed: %.8x\n", errorCode);
                                return;
                            }
                            if (!swb->currentSize) {
                                debugMsg("Error: sample has 0 size\n");
                                return;
                            }
                        }
                        ++swbInput;
                    }
                    if (nonEmptyInputSamplesCount) {
                        if (!codec.encoderOutput(drainingSample.sample)) {
                            crashed = true;
                            return;
                        }
                        nonEmptyInputSamplesCount = 0;
                    }
                }
            }
            eofEncountered = false;
            if ((outputSamplesCount == 8 && inputSamplesCount >= 1 || outputSamplesContainsEof) && !logics.empty()) {
                if (outputSamplesContainsEof) {
                    debugMsg("Encoder thread tries to find the eof logic\n");
                }
                unsigned int firstInputSampleMatchCounter = 0;
                if (inputSamplesCount >= 1 && !outputSamplesContainsEof) firstInputSampleMatchCounter = inputSamples[0].matchCounter;
                bool readyToSend = false;
                auto it = logics.end();
                while (it != logics.begin()) {
                    --it;
                    if (outputSamplesContainsEof) {
                        if (it->ptr->isEof) {
                            readyToSend = true;
                            ++it;
                            break;
                        }
                    } else {
                        if (it->ptr->matchCounter == firstInputSampleMatchCounter) {
                            readyToSend = true;
                            break;
                        } else if (it->ptr->matchCounter > firstInputSampleMatchCounter) {
                            break;
                        }
                    }
                }
                if (readyToSend) {
                    FileWriterInputEntry fileEntry;
                    fileEntry.logics.insert(fileEntry.logics.begin(), logics.begin(), it);
                    logics.erase(logics.begin(), it);
                    fileEntry.sampleCount = outputSamplesCount;
                    SampleWithBufferRef* ref = fileEntry.samples;
                    auto sourceRef = outputSamples.begin();
                    while (outputSamplesCount) {
                        --outputSamplesCount;
                        *ref = *sourceRef;
                        *sourceRef = memoryProvisioner.createEncoderOutputSample();

                        if (!ref->ptr->currentSize) {
                            debugMsg("Error: sample has 0 size\n");
                            return;
                        }

                        if (!ref->ptr->matchCounter) {
                            debugMsg("Error: sample has 0 matchCounter\n");
                            return;
                        }

                        ++sourceRef;
                        ++ref;
                    }
                    outputSamplesContainsEof = false;

                    ref = fileEntry.samples;
                    auto endIt = fileEntry.logics.end();
                    unsigned int sampleCountTemp = fileEntry.sampleCount;
                    for (auto it = fileEntry.logics.begin(); it != endIt; ++it) {
                        const unsigned int itMatchCounter = it->ptr->matchCounter;
                        SampleWithBufferRef* matchingSample = nullptr;
                        while (sampleCountTemp) {
                            const unsigned int refMatchCounter = ref->ptr->matchCounter;
                            if (refMatchCounter == itMatchCounter) {
                                matchingSample = ref;
                                break;
                            } else if (refMatchCounter > itMatchCounter) {
                                ++ref;
                                --sampleCountTemp;
                            } else {
                                break;
                            }
                        }
                        it->ptr->matchCounter = ownMatchCounter;
                        if (matchingSample) {
                            matchingSample->ptr->matchCounter = ownMatchCounter;
                        }
                        if (ownMatchCounter != 0) {
                            --ownMatchCounter;
                        }
                    }

                    std::unique_lock<std::mutex> fileInputGuard(fileWriterInput.mutex);
                    fileWriterInput.addInput(fileEntry);
                    SetEvent(fileWriterInput.event);
                }
            }
        }
        else {
            debugMsg("EncoderThread::threadLoop abnormal result (in decimal): %u\n", waitResult);
            return;
        }
    }
}

void EncoderThread::clearFrames() {
    while (frameCount) {
        frames[--frameCount].clear();
    }
}

void EncoderThread::reset() {
    clearFrames();
    logics.clear();
    ownMatchCounter = 0xFFFFFFFF;
    inputSamplesCount = 0;
    nonEmptyInputSamplesCount = 0;
    outputSamplesCount = 0;
    outputSamplesContainsEof = false;
    eofEncountered = false;
}
