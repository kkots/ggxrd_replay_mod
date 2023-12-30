#include "framework.h"
#include "FileWriterThread.h"
#include "debugMsg.h"
#include "ReplayFileData.h"
#include <mfapi.h>

extern HANDLE eventApplicationIsClosing;

void FileWriterThread::threadLoop() {

    class DestructionCode {
    public:
        DestructionCode(FileWriterThread* thisArg) : thisArg(thisArg) { }
        ~DestructionCode() {
            thisArg->inputs.clear();
        }
        FileWriterThread* thisArg = nullptr;
    } destructionCode(this);

    memset(prevInputRingBuffer, 0, sizeof(InputRingBuffer) * 2);

    while (true) {
        HANDLE events[2] = { eventApplicationIsClosing, fileWriterInput.event };
        DWORD waitResult = WaitForMultipleObjects(2, events, FALSE, INFINITE);
        if (waitResult == WAIT_OBJECT_0) {
            return;
        }
        else if (waitResult == WAIT_OBJECT_0 + 1) {
            bool processExited = false;
            {
                std::unique_lock<std::mutex> guard(fileWriterInput.mutex);
                if (fileWriterInput.needNotifyEof) {
                    needNotifyEof = true;
                }
                if (fileWriterInput.processExited) {
                    processExited = true;
                    fileWriterInput.processExited = false;
                }
                fileWriterInput.needNotifyEof = false;
                fileWriterInput.retrieve(inputs);
            }
            if (processExited) {
                encounteredEof = true;
            }
            if (needNotifyEof && inputs.empty() && encounteredEof) {
                needNotifyEof = false;
                SetEvent(eventResponse);
            }
            if (inputs.empty()) continue;
            std::unique_lock<std::mutex> fileGuard(replayFileData.mutex);
            std::unique_lock<std::mutex> indexGuard(replayFileData.indexMutex);
            std::unique_lock<std::mutex> replayInputsGuard(replayFileData.replayInputsMutex);
            for (auto& it : inputs) {
                __int64 filePos;
                filePos = _ftelli64(replayFileData.file);
                ReplayIndexEntry sameEntryEveryTime;
                sameEntryEveryTime.fileOffset = (unsigned long long)filePos;
                
                unsigned int lastMatchCounter;
                {
                    const LogicInfoRef& lastLogic = it.logics.front();
                    if (lastLogic->isEof && lastLogic->matchCounter == 0) {
                        if (replayFileData.index.size() + 1 > 0xFFFFFFFF) {
                            lastMatchCounter = 0;
                        } else {
                            lastMatchCounter = (0xFFFFFFFF - (unsigned int)(replayFileData.index.size() + 1)) + 1;
                        }
                    } else {
                        lastMatchCounter = lastLogic->matchCounter;
                    }
                }
                writeUnsignedInt(lastMatchCounter);
                writeUnsignedInt((unsigned int)it.logics.size());
                writeChar(it.sampleCount);
                writeUnsignedInt(0);  // all logics size (see writeUnsignedInt(totalLogicSize);)
                unsigned int matchCounterPrev = 0;
                bool hasMatchCounterPrev = false;
                unsigned int totalLogicSize = 0;
                encounteredEof = processExited;
                for (auto& jt : it.logics) {
                    if (replayFileData.index.empty()) {
                        replayFileData.index.reserve(3600);
                    } else if (replayFileData.index.capacity() == replayFileData.index.size()) {
                        replayFileData.index.reserve(replayFileData.index.capacity() * 2);
                    }
                    replayFileData.index.push_back(sameEntryEveryTime);
                    matchCounterPrev = jt->matchCounter;
                    hasMatchCounterPrev = true;
                    if (jt->isEof && matchCounterPrev == 0) {
                        if (replayFileData.index.size() > 0xFFFFFFFF) {
                            matchCounterPrev = 0;
                        } else {
                            matchCounterPrev = (0xFFFFFFFF - (unsigned int)replayFileData.index.size()) + 1;
                        }
                    }
                    debugMsg("Wrote frame matchCounter: %u\n", matchCounterPrev);
                    for (unsigned int i = 0; i < 2; ++i) {
                        updateStoredInputs(jt->inputRingBuffers[i], prevInputRingBuffer[i], replayFileData.replayInputs[i], matchCounterPrev);
                    }
                    memcpy(prevInputRingBuffer, jt->inputRingBuffers, sizeof(InputRingBuffer) * 2);
                    const bool encounteredEofBefore = encounteredEof;
                    encounteredEof = jt->isEof || processExited;
                    if (encounteredEof) {
                        debugMsg("File writer thread encountered eof\n");
                    }
                    if (!encounteredEof && encounteredEofBefore) {
                        debugMsg("Somehow set encounteredEof from true to false\n");
                    }
                    writeUnsignedInt(matchCounterPrev);
                    writeUnsignedInt(0); // logic size (see writeUnsignedInt(logicSize);)
                    filePos = _ftelli64(replayFileData.file);
                    for (unsigned int i = 0; i < 2; ++i) {
                        writeChar(jt->facings[i]);
                        writeChar((char)jt->inputRingBuffers[i].index);
                        fwrite(jt->inputRingBuffers[i].inputs, sizeof(Input), 30, replayFileData.file);
                        fwrite(jt->inputRingBuffers[i].framesHeld, sizeof(unsigned short), 30, replayFileData.file);
                        if (replayFileData.replayInputs[i].empty()) {
                            writeInt(0);
                            writeInt(0);
                        } else {
                            writeInt((int)replayFileData.replayInputs[i].size() - 1);
                            writeInt(replayFileData.replayInputs[i].back().framesHeld);
                        }
                    }

                    char* dataPtr = jt->data;
                    translateCamera(dataPtr);
                    translateHurtboxes(dataPtr);
                    translateHitboxes(dataPtr);
                    translatePushboxes(dataPtr);
                    translatePoints(dataPtr);
                    translateThrowInfos(dataPtr);
                    unsigned int logicSize = (unsigned int)(_ftelli64(replayFileData.file) - filePos);
                    fflush(replayFileData.file);
                    fseek(replayFileData.file, -(int)logicSize - 4, SEEK_CUR);
                    writeUnsignedInt(logicSize);
                    fflush(replayFileData.file);
                    fseek(replayFileData.file, logicSize, SEEK_CUR);
                    totalLogicSize += logicSize + 8;  // include two fields: the match counter and the size of this particular logic element that are at the start
                }
                fflush(replayFileData.file);
                fseek(replayFileData.file, -(int)totalLogicSize - 4, SEEK_CUR);
                writeUnsignedInt(totalLogicSize);
                fflush(replayFileData.file);
                fseek(replayFileData.file, totalLogicSize, SEEK_CUR);
                unsigned int sampleCount = it.sampleCount;
                SampleWithBufferRef* sample = it.samples;
                while (sampleCount) {
                    --sampleCount;
                    const unsigned int currentMatchCounter = sample->ptr->matchCounter;
                    writeUnsignedInt(currentMatchCounter);
                    writeUnsignedInt(sample->ptr->currentSize);

                    IMFSample* samplePtr = sample->ptr->sample;
                    CComPtr<IMFAttributes> attribs;
                    HRESULT errorCode = samplePtr->QueryInterface(IID_IMFAttributes, (LPVOID*)&attribs);
                    if (FAILED(errorCode)) {
                        debugMsg("samplePtr->QueryInterface(IID_IMFAttributes, (LPVOID*)&attribs) failed: %.8x\n", errorCode);
                        return;
                    }

                    UINT64 decodeTimestamp = 0;
                    errorCode = attribs->GetUINT64(MFSampleExtension_DecodeTimestamp, &decodeTimestamp);
                    if (FAILED(errorCode)) {
                        debugMsg("attribs->GetUINT64(MFSampleExtension_DecodeTimestamp, &decodeTimestamp) failed: %.8x\n", errorCode);
                        return;
                    }
                    writeUINT64(decodeTimestamp);

                    LONGLONG sampleTime = 0;
                    errorCode = samplePtr->GetSampleTime(&sampleTime);
                    if (FAILED(errorCode)) {
                        debugMsg("samplePtr->GetSampleTime(&sampleTime) failed: %.8x\n", errorCode);
                        return;
                    }
                    writeUINT64(sampleTime);

                    UINT32 pictureType = 0;
                    errorCode = attribs->GetUINT32(MFSampleExtension_VideoEncodePictureType, &pictureType);
                    if (FAILED(errorCode)) {
                        debugMsg("samplePtr->GetUINT32(MFSampleExtension_VideoEncodePictureType, &pictureType) failed: %.8x\n", errorCode);
                        return;
                    }
                    writeUnsignedInt(pictureType);

                    UINT32 isCleanPoint = 0;
                    errorCode = attribs->GetUINT32(MFSampleExtension_CleanPoint, &isCleanPoint);
                    if (FAILED(errorCode)) {
                        debugMsg("samplePtr->GetUINT32(MFSampleExtension_CleanPoint, &isCleanPoint) failed: %.8x\n", errorCode);
                        return;
                    }
                    writeChar((char)isCleanPoint);

                    UINT64 quantizationParameter = 0;
                    errorCode = attribs->GetUINT64(MFSampleExtension_VideoEncodeQP, &quantizationParameter);
                    if (FAILED(errorCode)) {
                        debugMsg("attribs->GetUINT64(MFSampleExtension_VideoEncodeQP, &quantizationParameter) failed: %.8x\n", errorCode);
                        return;
                    }
                    writeUINT64(quantizationParameter);

                    {
                        BYTE* bufPtr = nullptr;
                        DWORD bufMaxLength = 0;
                        DWORD bufCurrentLength = 0;
                        errorCode = sample->ptr->buffer->Lock(&bufPtr, &bufMaxLength, &bufCurrentLength);
                        if (FAILED(errorCode)) {
                            debugMsg("sample->ptr->buffer->Lock(...) failed: %.8x\n", errorCode);
                            return;
                        }
                        fwrite(bufPtr, 1, bufCurrentLength, replayFileData.file);
                        errorCode = sample->ptr->buffer->Unlock();
                        if (FAILED(errorCode)) {
                            debugMsg("sample->ptr->buffer->Unlock() failed: %.8x\n", errorCode);
                            return;
                        }
                    }
                    ++sample;
                }
            }
            if (needNotifyEof && encounteredEof) {
                needNotifyEof = false;
                encounteredEof = false;
                SetEvent(eventResponse);
            }
            inputs.clear();
        }
        else {
            debugMsg("FileWriterThread::threadLoop abnormal result (in decimal): %u\n", waitResult);
            return;
        }
    }
}

void FileWriterThread::updateStoredInputs(const InputRingBuffer& inputRingBuffer, const InputRingBuffer& prevInputRingBuffer, std::vector<StoredInput>& replayInputs, unsigned int matchCounter) {
    unsigned short indexDiff = 0;
    const bool currentEmpty = inputRingBuffer.empty();
    const bool prevEmpty = prevInputRingBuffer.empty();
    if (currentEmpty) return;
    if (prevEmpty && !currentEmpty) {
        indexDiff = inputRingBuffer.index + 1;
    }
    else {
        if (inputRingBuffer.index < prevInputRingBuffer.index) {
            indexDiff = inputRingBuffer.index + 30 - prevInputRingBuffer.index;
        }
        else {
            indexDiff = inputRingBuffer.index - prevInputRingBuffer.index;
        }
    }
    if (replayInputs.empty()) {
        replayInputs.reserve(3600);
        replayInputs.resize(30);
    } else {
        if (replayInputs.capacity() - replayInputs.size() < indexDiff) {
            replayInputs.reserve(replayInputs.capacity() * 2);
        }
        if (indexDiff) {
            replayInputs.resize(replayInputs.size() + indexDiff);
        }
    }
    auto destinationIt = replayInputs.begin() + (replayInputs.size() - 1);
    unsigned short sourceIndex = inputRingBuffer.index;
    unsigned int framesHeldInRecording = 0;
    bool weWentOutOfBounds = false;
    for (int i = 30; i != 0; --i) {
        const Input& input = inputRingBuffer.inputs[sourceIndex];
        const unsigned short framesHeld = inputRingBuffer.framesHeld[sourceIndex];
        if (i == 30) {
            debugMsg("updateStoredInputs called with matchCounter %u, first input framesHeld is %hu\n", matchCounter, framesHeld);
        }
        if (0xFFFFFFFF - matchCounter < (unsigned int)(framesHeld - 1)) {
            if (weWentOutOfBounds) {
                framesHeldInRecording = 0;
            } else {
                framesHeldInRecording = (0xFFFFFFFF - matchCounter) + 1;
                weWentOutOfBounds = true;
            }
            matchCounter = 0xFFFFFFFF;
        }
        else {
            framesHeldInRecording = weWentOutOfBounds ? 0 : framesHeld;
            matchCounter += framesHeld - 1;
        }
        if (framesHeld == 0) {
            break;
        }
        (Input&)(*destinationIt) = input;
        destinationIt->framesHeld = framesHeld;
        destinationIt->matchCounter = matchCounter;
        destinationIt->framesHeldInRecording = framesHeldInRecording;
        --destinationIt;

        if (matchCounter == 0xFFFFFFFF) {
            weWentOutOfBounds = true;
        } else {
            ++matchCounter;
        }
        if (sourceIndex == 0) sourceIndex = 29;
        else --sourceIndex;
    }
}

void FileWriterThread::writeChar(char value) {
    fputc(value, replayFileData.file);
}

void FileWriterThread::writeInt(int value) {
    fwrite(&value, 4, 1, replayFileData.file);
}

void FileWriterThread::writeUnsignedInt(unsigned int value) {
    fwrite(&value, 4, 1, replayFileData.file);
}

void FileWriterThread::writeFloat(float value) {
    fwrite(&value, 4, 1, replayFileData.file);
}

void FileWriterThread::writeUINT64(unsigned long long value) {
    fwrite(&value, 8, 1, replayFileData.file);
}

void FileWriterThread::translateArraybox(char*& dataPtr) {
    unsigned char hitboxesCount = *(unsigned char*)dataPtr;
    dataPtr += 4;
    unsigned int hitboxPtr = *(unsigned int*)dataPtr;
    dataPtr += 4;
    {
        std::unique_lock<std::mutex> hitboxMutex(replayFileData.hitboxDataMutex);
        auto found = replayFileData.hitboxData.find(hitboxPtr);
        if (found == replayFileData.hitboxData.end()) {
            while (hitboxesCount) {
                --hitboxesCount;
                hitboxDataArena.emplace_back(*(float*)dataPtr, *(float*)(dataPtr + 4), *(float*)(dataPtr + 8), *(float*)(dataPtr + 12));
                dataPtr += 16;
            }
            replayFileData.hitboxData[hitboxPtr] = hitboxDataArena;
            hitboxDataArena.clear();
        } else {
            dataPtr += hitboxesCount * 16;
        }
    }
    writeUnsignedInt(hitboxPtr);
    fwrite(dataPtr, 4, 9, replayFileData.file);
    dataPtr += 36;
}

void FileWriterThread::translateCamera(char*& dataPtr) {
    fwrite(dataPtr, 4, 8, replayFileData.file);
    dataPtr += 32;
}

void FileWriterThread::translateHurtboxes(char*& dataPtr) {
    while (*dataPtr != '\0') {
        writeChar(*dataPtr);
        if (*dataPtr == 1) {
            dataPtr += 4;
            translateArraybox(dataPtr);
        }
        else {
            dataPtr += 4;
            translateArraybox(dataPtr);
            translateArraybox(dataPtr);
        }
    }
    writeChar('\0');
    dataPtr += 4;
}

void FileWriterThread::translateHitboxes(char*& dataPtr) {
    while (*dataPtr != '\0') {
        translateArraybox(dataPtr);
    }
    writeChar('\0');
    dataPtr += 4;
}

void FileWriterThread::translatePushboxes(char*& dataPtr) {
    unsigned char numPushboxes = *(unsigned char*)dataPtr;
    fwrite(dataPtr, 4, 1 + 5 * numPushboxes, replayFileData.file);
    dataPtr += (1 + 5 * numPushboxes) * 4;
}

void FileWriterThread::translatePoints(char*& dataPtr) {
    unsigned char numPoints = *(unsigned char*)dataPtr;
    fwrite(dataPtr, 4, 1 + 2 * numPoints, replayFileData.file);
    dataPtr += (1 + 2 * numPoints) * 4;
}

void FileWriterThread::translateThrowInfos(char*& dataPtr) {
    unsigned char throwInfosCount = *(unsigned char*)dataPtr;
    writeChar(throwInfosCount);
    while (throwInfosCount) {
        unsigned char totalSize = *(unsigned char*)dataPtr;
        writeChar(totalSize);  // size
        dataPtr += 4;
        writeChar(*dataPtr);  // flags
        dataPtr += 4;
        fwrite(dataPtr, 1, totalSize, replayFileData.file);
        dataPtr += totalSize;
        --throwInfosCount;
    }
}
