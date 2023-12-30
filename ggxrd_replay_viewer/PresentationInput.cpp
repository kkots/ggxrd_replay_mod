#include "PresentationInput.h"

PresentationInput presentationInput;

void PresentationInput::destroy() {
	entries.clear();
}

void PresentationInput::addEntry(PresentationInputEntry& newEntry) {
	if (entriesCount == entries.size()) {
		entries.push_back(newEntry);
	} else {
		PresentationInputEntry& reusedEntry = entries[entriesCount];
		reusedEntry.logicInfo.insert(reusedEntry.logicInfo.begin(), newEntry.logicInfo.begin(), newEntry.logicInfo.end());
		reusedEntry.samples.insert(reusedEntry.samples.begin(), newEntry.samples.begin(), newEntry.samples.end());
	}
	++entriesCount;
}

void PresentationInput::clear() {
	while (entriesCount) {
		--entriesCount;
		PresentationInputEntry& entry = entries[entriesCount];
		entry.logicInfo.clear();
		entry.samples.clear();
	}
}
