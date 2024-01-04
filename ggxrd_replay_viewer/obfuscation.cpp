#include "obfuscation.h"
std::vector<char> obfuscateOrDeobfuscate(const char* str, int length, const char* key) {
	const char* strPtr = str;
	const char* keyPtr = key;
	std::vector<char> result;
	result.reserve(length + 1);
	while (length != 0) {
		result.push_back((*strPtr) ^ (*keyPtr));

		++keyPtr;
		if (*keyPtr == '\0') {
			keyPtr = key;
		}
		++strPtr;
		--length;
	}
	result.push_back('\0');
	return result;
}