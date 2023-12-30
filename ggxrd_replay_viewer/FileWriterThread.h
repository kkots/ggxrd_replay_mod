#pragma once
#include "framework.h"
#include "FileWriterInput.h"
#include "ReplayFileStructs.h"
#include "StoredInput.h"
#include <vector>

class FileWriterThread
{
public:
	void threadLoop();
	HANDLE eventResponse = NULL;
private:
	void updateStoredInputs(const InputRingBuffer& inputRingBuffer, const InputRingBuffer& prevInputRingBuffer, std::vector<StoredInput>& replayInputs, unsigned int matchCounter);
	std::vector<FileWriterInputEntry> inputs;
	InputRingBuffer prevInputRingBuffer[2];
	void writeChar(char value);
	void writeInt(int value);
	void writeUnsignedInt(unsigned int value);
	void writeFloat(float value);
	void writeUINT64(unsigned long long value);
	void translateCamera(char*& dataPtr);
	void translateHurtboxes(char*& dataPtr);
	void translateHitboxes(char*& dataPtr);
	void translatePushboxes(char*& dataPtr);
	void translatePoints(char*& dataPtr);
	void translateThrowInfos(char*& dataPtr);
	void translateArraybox(char*& dataPtr);
	std::vector<HitboxData> hitboxDataArena;
	bool needNotifyEof = false;
	bool encounteredEof = false;
};
