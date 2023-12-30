#include "framework.h"
#include "FileReaderThread.h"
#include "PresentationTimer.h"
#include "debugMsg.h"
#include "ReplayFileData.h"
#include "MemoryProvisioner.h"
#include <mfapi.h>
#include "WinError.h"

extern HANDLE eventApplicationIsClosing;

void FileReaderThread::threadLoop() {
    
    HRESULT errorCode;
    while (true) {
        HANDLE events[2] = { eventApplicationIsClosing, presentationTimer.eventForFileReader };
        DWORD waitResult = WaitForMultipleObjects(2, events, FALSE, INFINITE);
        if (waitResult == WAIT_OBJECT_0) {
            return;
        }
        else if (waitResult == WAIT_OBJECT_0 + 1) {
            while (true) {
                unsigned int desiredMatchCounter = 0;
                bool needReadFurther = false;
                bool needStartScrubbing = false;
                bool needStopScrubbing = false;
                {
                    std::unique_lock<std::mutex> presentationGuard(presentationTimer.mutex);
                    if (presentationTimer.scrubbing) {
                        desiredMatchCounter = presentationTimer.scrubbingMatchCounter;
                        if (!scrubbing || scrubbingMatchCounter != desiredMatchCounter) {
                            needStartScrubbing = true;
                        }
                    } else {
                        if (scrubbing) {
                            scrubbing = false;
                            needStopScrubbing = true;
                        }
                        if (presentationTimer.isFileOpen && (presentationTimer.isPlaying || matchCounterEmpty)) {
                            scrubbingIndexEntryInvalidated = true;
                            if (
                                    matchCounterEmpty
                                    || matchCounter <= presentationTimer.matchCounter
                                    && presentationTimer.matchCounter - matchCounter < 32
                                    || matchCounter > presentationTimer.matchCounter
                                ) {
                                needReadFurther = true;
                            }
                        }
                    }
                }
                if (needSwitchFiles) {
                    scrubbing = false;
                    needSwitchFiles = false;
                    matchCounterEmpty = true;
                    matchCounter = 0;
                    SetEvent(eventResponse);
                    break;
                }
                if (needStartScrubbing) {
                    needReadFurther = true;
                    std::unique_lock<std::mutex> indexGuard(replayFileData.indexMutex);
                    ReplayIndexEntry newScrubbingIndexEntry = replayFileData.index[0xFFFFFFFF - desiredMatchCounter];
                    bool justNotify = !scrubbingIndexEntryInvalidated && newScrubbingIndexEntry.fileOffset == scrubbingIndexEntry.fileOffset;
                    scrubbingIndexEntryInvalidated = false;
                    scrubbingIndexEntry = newScrubbingIndexEntry;
                    scrubbing = true;
                    scrubbingMatchCounter = desiredMatchCounter;
                    if (justNotify) {
                        std::unique_lock<std::mutex> queueGuard(decoderInput.mutex);
                        decoderInput.scrubbing = scrubbing;
                        decoderInput.scrubbingMatchCounter = scrubbingMatchCounter;
                        SetEvent(decoderInput.event);
                        needReadFurther = false;
                    }
                }
                if (needStopScrubbing && !needReadFurther) {
                    std::unique_lock<std::mutex> queueGuard(decoderInput.mutex);
                    decoderInput.scrubbing = false;
                    SetEvent(decoderInput.event);
                }
                needStopScrubbing = false;
                if (!needReadFurther) break;
                std::unique_lock<std::mutex> fileGuard(replayFileData.mutex);
                if (needStartScrubbing) {
                    matchCounterEmpty = true;
                    _fseeki64(replayFileData.file, scrubbingIndexEntry.fileOffset, SEEK_SET);
                }
                
                const bool isOutOfBounds = _ftelli64(replayFileData.file) >= (long long)replayFileData.header.indexOffset;
                if (!isOutOfBounds) {
                    unsigned int fileMatchCounter = readUnsignedInt();
                    unsigned int logicsCount = readUnsignedInt();
                    char sampleCount = readChar();
                    unsigned int totalLogicsSize = readUnsignedInt();
                    for (unsigned int logicCounter = 0; logicCounter < logicsCount; ++logicCounter) {
                        LogicInfoRef newLogic = memoryProvisioner.createLogicInfo();
                        matchCounter = readUnsignedInt();
                        newLogic->matchCounter = matchCounter;
                        matchCounterEmpty = false;
                        unsigned int thisOneLogicSize = readUnsignedInt();
                        unsigned int consumedSizeSoFar = 0;
                        for (unsigned int i = 0; i < 2; ++i) {
                            newLogic->facings[i] = readChar();
                            ++consumedSizeSoFar;
                            newLogic->inputRingBuffers[i].index = readChar();
                            ++consumedSizeSoFar;
                            fread(newLogic->inputRingBuffers[i].inputs, sizeof(Input), 30, replayFileData.file);
                            consumedSizeSoFar += sizeof(Input) * 30;
                            fread(newLogic->inputRingBuffers[i].framesHeld, sizeof(unsigned short), 30, replayFileData.file);
                            consumedSizeSoFar += sizeof(unsigned short) * 30;
                            newLogic->replayStates[i].index = readInt();
                            consumedSizeSoFar += 4;
                            newLogic->replayStates[i].repeatIndex = readInt();
                            consumedSizeSoFar += 4;
                        }
                        fseek(replayFileData.file, thisOneLogicSize - consumedSizeSoFar, SEEK_CUR);
                        newEntry.logics.push_back(std::move(newLogic));
                    }
                    while (sampleCount) {
                        --sampleCount;
                        unsigned int sampleMatchCounter = readUnsignedInt();
                        //debugMsg("Read frame matchCounter: %u\n", sampleMatchCounter);
                        unsigned int sampleSize = readUnsignedInt();  // does not include the sample attributes
                        UINT64 decodeTimestamp = readUINT64();
                        LONGLONG sampleTime = readUINT64();
                        UINT32 pictureType = readUnsignedInt();
                        char isCleanPoint = readChar();
                        UINT64 quantizationParameter = readUINT64();
                        SampleWithBufferRef newSample = memoryProvisioner.createDecoderInputSample();
                        newSample->matchCounter = sampleMatchCounter;
                        CComPtr<IMFAttributes> attribs;
                        errorCode = newSample->sample->QueryInterface(IID_IMFAttributes, (LPVOID*)&attribs);
                        if (FAILED(errorCode)) {
                            debugMsg("newSample->sample->QueryInterface(IID_IMFAttributes, (LPVOID*)&attribs) failed: %.8x\n", errorCode);
                            return;
                        }

                        errorCode = attribs->SetUINT32(MFSampleExtension_LastSlice, 1);
                        if (FAILED(errorCode)) {
                            debugMsg("attribs->SetUINT32(MFSampleExtension_LastSlice, 1) failed: %.8x\n", errorCode);
                            return;
                        }

                        errorCode = attribs->SetUINT64(MFSampleExtension_DecodeTimestamp, decodeTimestamp);
                        if (FAILED(errorCode)) {
                            debugMsg("attribs->SetUINT64(MFSampleExtension_DecodeTimestamp, decodeTimestamp) failed: %.8x\n", errorCode);
                            return;
                        }

                        errorCode = newSample->sample->SetSampleTime(sampleTime);
                        if (FAILED(errorCode)) {
                            debugMsg("newSample->sample->SetSampleTime(sampleTime) failed: %.8x\n", errorCode);
                            return;
                        }

                        errorCode = newSample->sample->SetSampleDuration(166666);
                        if (FAILED(errorCode)) {
                            debugMsg("newSample->sample->SetSampleDuration(166666) failed: %.8x\n", errorCode);
                            return;
                        }

                        errorCode = attribs->SetUINT32(MFSampleExtension_LongTermReferenceFrameInfo, 65535);
                        if (FAILED(errorCode)) {
                            debugMsg("attribs->SetUINT32(MFSampleExtension_LongTermReferenceFrameInfo, 65535) failed: %.8x\n", errorCode);
                            return;
                        }

                        errorCode = attribs->SetUINT32(MFSampleExtension_VideoEncodePictureType, pictureType);
                        if (FAILED(errorCode)) {
                            debugMsg("attribs->SetUINT32(MFSampleExtension_VideoEncodePictureType, pictureType) failed: %.8x\n", errorCode);
                            return;
                        }

                        errorCode = attribs->SetUINT32(MFSampleExtension_CleanPoint, isCleanPoint);
                        if (FAILED(errorCode)) {
                            debugMsg("attribs->SetUINT32(MFSampleExtension_CleanPoint, isCleanPoint) failed: %.8x\n", errorCode);
                            return;
                        }

                        errorCode = attribs->SetUINT64(MFSampleExtension_VideoEncodeQP, quantizationParameter);
                        if (FAILED(errorCode)) {
                            debugMsg("attribs->SetUINT64(MFSampleExtension_VideoEncodeQP, quantizationParameter) failed: %.8x\n", errorCode);
                            return;
                        }

                        {
                            BYTE* bufPtr = nullptr;
                            DWORD bufMaxLength = 0;
                            DWORD bufCurrentLength = 0;
                            errorCode = newSample->buffer->Lock(&bufPtr, &bufMaxLength, &bufCurrentLength);
                            if (FAILED(errorCode)) {
                                debugMsg("newSample->buffer->Lock(&bufPtr, &bufMaxLength, &bufCurrentLength) failed: %.8x\n", errorCode);
                                return;
                            }

                            if (sampleSize > bufMaxLength) return;
                            fread(bufPtr, 1, sampleSize, replayFileData.file);

                            errorCode = newSample->buffer->SetCurrentLength(sampleSize);
                            if (FAILED(errorCode)) {
                                debugMsg("newSample->buffer->SetCurrentLength(sampleSize) failed: %.8x\n", errorCode);
                                return;
                            }

                            errorCode = newSample->buffer->Unlock();
                            if (FAILED(errorCode)) {
                                debugMsg("newSample->buffer->Unlock() failed: %.8x\n", errorCode);
                                return;
                            }

                        }
                        newEntry.samples.push_back(std::move(newSample));
                    }
                }
                {
                    std::unique_lock<std::mutex> queueGuard(decoderInput.mutex);
                    if (!newEntry.logics.empty() || !newEntry.samples.empty()) {
                        if (!decoderInput.scrubbing && scrubbing 
                                || decoderInput.scrubbing && scrubbing && decoderInput.scrubbingMatchCounter != scrubbingMatchCounter) {
                            decoderInput.clear();
                        }
                        decoderInput.addEntry(newEntry);
                    }
                    decoderInput.scrubbing = scrubbing;
                    decoderInput.scrubbingMatchCounter = scrubbingMatchCounter;
                    unsigned int entriesIndexTest = decoderInput.entriesIndex;
                    for (unsigned int counterTest = decoderInput.entriesCount; counterTest != 0; --counterTest) {
                        if (decoderInput.entries[entriesIndexTest].samples.size() > 8) {
                            debugMsg("Error\n");
                        }
                        ++entriesIndexTest;
                        if (entriesIndexTest == decoderInput.entries.size()) entriesIndexTest = 0;
                    }
                    SetEvent(decoderInput.event);
                }
                if (isOutOfBounds) break;
                newEntry.logics.clear();
                newEntry.samples.clear();
            }
        }
        else {
            debugMsg("FileReaderThread::threadLoop abnormal result (in decimal): %u\n", waitResult);
        }
    }
}

char FileReaderThread::readChar() {
    return fgetc(replayFileData.file);
}

int FileReaderThread::readInt() {
    int value;
    fread(&value, 4, 1, replayFileData.file);
    return value;
}

unsigned int FileReaderThread::readUnsignedInt() {
    unsigned int value;
    fread(&value, 4, 1, replayFileData.file);
    return value;
}

float FileReaderThread::readFloat() {
    float value;
    fread(&value, 4, 1, replayFileData.file);
    return value;
}

UINT64 FileReaderThread::readUINT64() {
    UINT64 value;
    fread(&value, 8, 1, replayFileData.file);
    return value;
}
