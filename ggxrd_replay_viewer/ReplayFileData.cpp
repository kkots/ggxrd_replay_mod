#include "ReplayFileData.h"
#include <Shlobj.h>
#include "WinError.h"
#include "debugMsg.h"

extern void showOrLogErrorWide(debugStrArgType msg, bool showInsteadOfLog);
ReplayFileData replayFileData;

wchar_t errorString[1024];

bool ReplayFileData::startTempFile() {
	if (file) return true;
	if (path.empty()) {
		PWSTR videosFolder = nullptr;
		if (FAILED(SHGetKnownFolderPath(FOLDERID_Videos, NULL, NULL, &videosFolder))) {  // won't have backslash. Free memory with CoTaskMemFree(PWSTR*)
			WinError winErr;
			debugMsg("SHGetKnownFolderPath failed: %ls\n", winErr.getMessage());
			return false;
		}
		path = videosFolder;
		if (path.empty() || path.back() != L'\\') path += L'\\';
		path += L"ggxrd_replay.rpl";
		CoTaskMemFree(videosFolder);
	}

	if (!startFile(path.c_str(), false)) return false;

	fseek(file, sizeof(ReplayFileHeader), SEEK_SET);
	
	return true;
}

bool ReplayFileData::startFile(const wchar_t* path, bool forReading) {

	errno_t errCode = _wfopen_s(&file, path, !forReading ? L"w+b" : L"rb");
	if (errCode || !file) {
		_wcserror_s(errorString, errCode);
		std::wstring errorMessage = L"Failed to open file ";
		errorMessage += path;
		errorMessage += L": ";
		errorMessage += errorString;
		errorMessage += L"\n";
		showOrLogErrorWide(errorMessage.c_str(), true);
		return false;
	}

	debugMsg("temp file: %ls\n", path);

	return true;
}

void ReplayFileData::closeFile() {
	if (file) {
		for (int i = 0; i < 2; ++i) {
			replayInputs[i].clear();
		}
		index.clear();
		hitboxData.clear();
		fflush(file);
		fclose(file);
		file = nullptr;
	}
}
