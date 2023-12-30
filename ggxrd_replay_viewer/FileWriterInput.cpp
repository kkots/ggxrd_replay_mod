#include "FileWriterInput.h"

FileWriterInput fileWriterInput;

void FileWriterInput::addInput(FileWriterInputEntry& input) {
	if (entries.size() < 2) {
		entries.push_back(std::move(input));
	} else {
		while (input.sampleCount) {
			input.samples[--input.sampleCount].clear();
		}
		entries.push_back(std::move(input));
	}
}

void FileWriterInput::retrieve(std::vector<FileWriterInputEntry>& destination) {
	destination.insert(destination.begin(), entries.begin(), entries.end());
	entries.clear();
}

void FileWriterInput::destroy() {
	entries.clear();
}
