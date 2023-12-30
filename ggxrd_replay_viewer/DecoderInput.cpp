#include "DecoderInput.h"

DecoderInput decoderInput;

void DecoderInput::destroy() {
	entries.clear();
}

void DecoderInput::addEntry(DecoderInputEntry& newEntry) {
	if (entries.empty()) {
		entries.push_back(newEntry);
	} else if (entriesCount == entries.size()) {
		entries.insert(entries.begin() + (entriesIndex + entriesCount - 1) % entries.size() + 1, newEntry);
	}
	else {
		DecoderInputEntry& reusedEntry = entries[(entriesIndex + entriesCount) % entries.size()];
		reusedEntry.logics.insert(reusedEntry.logics.begin(), newEntry.logics.begin(), newEntry.logics.end());
		reusedEntry.samples.insert(reusedEntry.samples.begin(), newEntry.samples.begin(), newEntry.samples.end());
	}
	++entriesCount;
}

void DecoderInput::getFirstEntry(DecoderInputEntry& destination) {
	if (!entriesCount) return;
	--entriesCount;
	DecoderInputEntry& firstEntry = entries[entriesIndex];
	destination.logics.insert(destination.logics.begin(), firstEntry.logics.begin(), firstEntry.logics.end());
	destination.samples.insert(destination.samples.begin(), firstEntry.samples.begin(), firstEntry.samples.end());
	firstEntry.logics.clear();
	firstEntry.samples.clear();
	++entriesIndex;
	if (entriesIndex == entries.size()) entriesIndex = 0;
}

void DecoderInput::clear() {
	while (entriesCount) {
		--entriesCount;
		DecoderInputEntry& firstEntry = entries[entriesIndex];
		firstEntry.logics.clear();
		firstEntry.samples.clear();
		++entriesIndex;
		if (entriesIndex == entries.size()) entriesIndex = 0;
	}
}
