// ggxrd_replay_viewer.cpp : Defines the entry point for the application.
//

#include "framework.h"

#pragma comment(linker,"\"/manifestdependency:type='win32' \
name='Microsoft.Windows.Common-Controls' version='6.0.0.0' \
processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")  // this is needed to get unicode tooltips working (non-unicode works without this)
// thanks to https://learn.microsoft.com/en-us/windows/win32/controls/cookbook-overview

#include "ggxrd_replay_viewer.h"
#include "MemoryFunctions.h"
#include "PngResource.h"
#include <Windowsx.h>
#include <commdlg.h>  // for GetOpenFileNameW

// the Rpcrt4.lib is needed for UuidCompare
#pragma comment(lib, "mfplat")  // for Media Foundation
#pragma comment(lib, "mf")  // for Media Foundation
#pragma comment(lib, "mfuuid")  // for Media Foundation
#pragma comment(lib, "Shlwapi")  // for Media Foundation
// the Strmiids.lib is needed for IID_ICodecAPI definition

#pragma comment(lib, "Winmm")  // for timeBeginPeriod

// libpng16.lib is needed to decode PNGs
// zlib.lib is needed for libpng16.lib

// d3d9.lib/dll is needed for Direct3D (can get from Direct3D 9 SDK, dll should be installed on Windows systems or install with DirectX 9 installer)

#include <commctrl.h>  // for tooltips

#include <string>
#include "WinError.h"
#include "debugMsg.h"
#include "PngButtonManager.h"
#include <thread>
#include <mutex>
#include "FileMapping.h"
#include "MemoryProvisioner.h"
#include "EncoderThread.h"
#include "EncoderInput.h"
#include "Codec.h"
#include "FileWriterInput.h"
#include "FileWriterThread.h"
#include "ReplayFileData.h"
#include "PresentationTimer.h"
#include "DecoderThread.h"
#include "FileReaderThread.h"
#include "DecoderInput.h"
#include "PresentationInput.h"
#include "PresentationThread.h"
#include "InputsDrawing.h"
#include "Layout.h"

#define DLL_NAME TEXT("ggxrd_replay_hook.dll")
#define PROCESS_NAME TEXT("GuiltyGearXrd.exe")
#define FILTER_STRING TEXT("GGXrd replay mod replay file (*.rpl)\0*.RPL\0")
#define MAX_LOADSTRING 100

// Global Variables:
HINSTANCE hInst = NULL;                         // current instance
HWND mainWindowHandle = NULL;                   // a handle to the main window of the application
WCHAR szTitle[MAX_LOADSTRING];                  // The title bar text
WCHAR szWindowClass[MAX_LOADSTRING];            // the main window class name
bool doingRealtimeStuff = false;
HWND globalTooltipHandle = NULL;
PngButtonManager pngButtonManager;
HDC hdcMem = NULL;
std::mutex hdcMemMutex;
bool mouseIsDown = false;
MSG msg;
DWORD ggProcessId = 0;
HANDLE ggProcessHandle = NULL;
std::mutex ggProcessHandleMutex;
std::thread ggProcessWatcher;
HANDLE eventApplicationIsClosing = NULL;
HANDLE eventGgProcessHandleObtained = NULL;
HANDLE eventGgProcessExited = NULL;
bool ggProcessWatcherIsRunning = false;
HANDLE fileMappingHandle = NULL;
HANDLE eventChangesMade = NULL;
HANDLE eventChangesReceived = NULL;
HANDLE fileMapMutex = NULL;
FileMappingManager fileMappingManager;
bool isRecordingGlobal = false;
HCURSOR handCursor = NULL;
bool seekbarIsPressed = false;
bool wasPlayingBeforeSeekbarPressed = false;
HWND leftScrollbar = NULL;
HWND rightScrollbar = NULL;
int scrollbarWidth = 0;
HBRUSH grayColor = NULL;
// TODO:
// 1) Are we storing the input buffer indices and framesHeld in the file during recording (yes)?
// 2) Create index table for input buffer index into input drawing command row, store input buffer index in the input drawing command row
// 3) In presentation thread read the input buffer index, find the input drawing command row and draw them on the screen
// 4) NOW we can fix the frame-skip and play logic data elements instead of samples. Remove frame-skip
// 5) While scrubbing scrub to a logic. As for sample - find the oldest (leftmost) one
// 9) Implement scrollbar, a toggle to make the inputs window follow the video timer or stay independently, locked to the position it's currently scrolled to
// 10) Implement row selection cursor (yellow, black over blue, but do blue later)
// 11) Implement row selection by a click (highlight with blue finally)
// 11) Implement drag-selection into auto scroll near the edge
// 12) Implemenet shift-keyboard selection: (ugh, I can't list these right now)
int ctxStandardFlags = 0;
HMENU ctxMenu = NULL;

HWND subwindowHandle = NULL;
bool mouseLeft = true;
debugStrStrType ctxDialogText;
std::vector<debugStrStrType> comboBoxItemNames;
int frameskip = 3;
bool displayBoxes = false;
bool displayInputs = true;

TCHAR dllPath[MAX_PATH] = { TEXT('\0') };
TCHAR settingsPath[MAX_PATH] = { TEXT('\0') };

TEXTMETRIC textMetric;

bool encoderThreadStarted = false;
std::thread encoderThreadThread;
EncoderThread encoderThread;
bool fileWriterThreadStarted = false;
std::thread fileWriterThreadThread;
FileWriterThread fileWriterThread;
bool decoderThreadStarted = false;
std::thread decoderThreadThread;
DecoderThread decoderThread;
bool fileReaderThreadStarted = false;
std::thread fileReaderThreadThread;
FileReaderThread fileReaderThread;
bool presentationThreadStarted = false;
std::thread presentationThreadThread;
PresentationThread presentationThread;

const unsigned int MAX_FILEMAP_SIZE = 256 * 1024 + 2 * (1280 * 720 * 4 + 256);

PngResource recordBtn;
PngResource recordBtnHover;
PngResource recordBtnPressed;
PngResource stopBtn;
PngResource stopBtnHover;
PngResource stopBtnPressed;
PngResource playBtn;
PngResource playBtnHover;
PngResource playBtnPressed;
PngResource playBtnDisabled;
PngResource pauseBtn;
PngResource pauseBtnHover;
PngResource pauseBtnPressed;
PngResource followBtn;
PngResource followBtnHover;
PngResource followBtnToggled;
PngResource followBtnToggledHover;
PngResource followBtnPressed;
PngResource stepLeftBtn;
PngResource stepLeftBtnHover;
PngResource stepLeftBtnPressed;
PngResource stepLeftBtnDisabled;
PngResource stepRightBtn;
PngResource stepRightBtnHover;
PngResource stepRightBtnPressed;
PngResource stepRightBtnDisabled;
const unsigned int RECORD_BUTTON = 1;
const unsigned int PLAY_BUTTON = 2;
const unsigned int LEFT_FOLLOW_BUTTON = 3;
const unsigned int RIGHT_FOLLOW_BUTTON = 4;
const unsigned int STEP_LEFT_BUTTON = 5;
const unsigned int STEP_RIGHT_BUTTON = 6;
unsigned int BUTTONBAR_BUTTON_COUNT = 4;

// Forward declarations of functions included in this code module:
ATOM                MyRegisterClass(HINSTANCE hInstance);
BOOL                InitInstance(HINSTANCE, int);
LRESULT CALLBACK    WndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK    About(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK    CtxDialogMsgHandler(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK    settingsHandler(HWND, UINT, WPARAM, LPARAM);
void buttonClicked(unsigned int id);
void ggProcessWatcherProc();
bool initializeInterprocess(bool needShowErrorMessageBoxes);
bool initializePlayback();
void onCtrlO();
void showLastWinError(const wchar_t* msg, bool showInsteadOfLog);
void showOrLogError(debugStrArgType msg, bool showInsteadOfLog);
void showOrLogErrorWide(const wchar_t* msg, bool showInsteadOfLog);
bool initializeTooltip();
bool stopPlayback();
bool writeFileSupplementaries();
bool readSupplementaries();
bool seekbarMouseHittest(float* ratio = nullptr, POINT* cursor = nullptr, bool forceTrue = false);
void onSeekbarMousePress(float ratio, bool pressedJustNow);
void onSeekbarMousePress(unsigned int targetMatchCounter, bool pressedJustNow);
void onSeekbarMouseMove();
void onSeekbarMouseUp();
void onReachPlaybackEnd();
void updateScrollbarInfo();
void startWaiting();
void finishWaiting();
bool rectHitTest(const RECT* rect, int x, int y);
void onSelectAll();
void onCopy();
void openSettings();
void readSettings();
void writeSettings();
int findChar(const char* buf, char c);
void trim(std::string& str);
std::vector<std::string> split(const std::string& str, char c);
bool endsWithCaseInsensitive(std::wstring str, const wchar_t* endingPart);
bool parseInteger(const std::string& keyValue, int& integer);
bool parseBoolean(const std::string& keyValue, bool& aBooleanValue);
void onArrowLeftRight(bool isRight);
void makeAtLeastOnePanelMarkedAsLastClicked();
bool rectsIntersect(const RECT* a, const RECT* b);
bool selectRplFile(bool forWriting, std::wstring& path);

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
                     _In_opt_ HINSTANCE hPrevInstance,
                     _In_ LPWSTR    lpCmdLine,
                     _In_ int       nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

    GetCurrentDirectory(MAX_PATH, dllPath);
    memcpy(settingsPath, dllPath, sizeof(dllPath));
    _tcscat_s(dllPath, MAX_PATH, TEXT("\\"));
    _tcscat_s(dllPath, MAX_PATH, DLL_NAME);
    debugMsg("Dll path: " PRINTF_STRING_ARGA "\n", dllPath);

    eventApplicationIsClosing = CreateEvent(NULL, TRUE, FALSE, NULL);
    eventGgProcessHandleObtained = CreateEvent(NULL, FALSE, FALSE, NULL);
    eventGgProcessExited = CreateEvent(NULL, FALSE, FALSE, NULL);
    
    if (!codec.initializePreWindow()) return FALSE;

    scrollbarWidth = GetSystemMetrics(SM_CXHSCROLL);

    // Initialize global strings
    LoadStringW(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
    LoadStringW(hInstance, IDC_GGXRDREPLAYVIEWER, szWindowClass, MAX_LOADSTRING);
    MyRegisterClass(hInstance);

    handCursor = (HCURSOR)LoadImage(NULL, IDC_HAND, IMAGE_CURSOR, 0, 0, LR_DEFAULTSIZE | LR_SHARED);
    if (!handCursor) {
        WinError winErr;
        debugMsg("Failed to load hand cursor: %ls\n", winErr.getMessage());
        return FALSE;
    }

    // Perform application initialization:
    if (!InitInstance (hInstance, nCmdShow))
    {
        return FALSE;
    }

    HACCEL hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_GGXRDREPLAYVIEWER));

    // Main message loop:
    while (true)
    {
        if (doingRealtimeStuff) {
            bool encounteredQuit = false;
            while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
                encounteredQuit = encounteredQuit || msg.message == WM_QUIT;
                if (!TranslateAccelerator(msg.hwnd, hAccelTable, &msg))
                {
                    TranslateMessage(&msg);
                    DispatchMessage(&msg);
                }
            }
            if (msg.message == WM_QUIT || encounteredQuit) {
                break;
            }
        } else {
            if (GetMessage(&msg, nullptr, 0, 0)) {
                if (!TranslateAccelerator(msg.hwnd, hAccelTable, &msg))
                {
                    TranslateMessage(&msg);
                    DispatchMessage(&msg);
                }
            } else {
                break;
            }
        }
    }

    codec.shutdown();
    return (int) msg.wParam;
}



//
//  FUNCTION: MyRegisterClass()
//
//  PURPOSE: Registers the window class.
//
ATOM MyRegisterClass(HINSTANCE hInstance)
{
    WNDCLASSEXW wcex;

    wcex.cbSize = sizeof(WNDCLASSEX);

    wcex.style          = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc    = WndProc;
    wcex.cbClsExtra     = 0;
    wcex.cbWndExtra     = 0;
    wcex.hInstance      = hInstance;
    wcex.hIcon          = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_GGXRDREPLAYVIEWER));
    wcex.hCursor        = LoadCursor(nullptr, IDC_ARROW);
    wcex.hbrBackground  = (HBRUSH)(COLOR_WINDOW+1);
    wcex.lpszMenuName   = MAKEINTRESOURCEW(IDC_GGXRDREPLAYVIEWER);
    wcex.lpszClassName  = szWindowClass;
    wcex.hIconSm        = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));

    return RegisterClassExW(&wcex);
}

//
//   FUNCTION: InitInstance(HINSTANCE, int)
//
//   PURPOSE: Saves instance handle and creates main window
//
//   COMMENTS:
//
//        In this function, we save the instance handle in a global variable and
//        create and display the main program window.
//
BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
{
    hInst = hInstance; // Store instance handle in our global variable

    mainWindowHandle = CreateWindowW(szWindowClass, szTitle, WS_OVERLAPPEDWINDOW,
    CW_USEDEFAULT, 0, CW_USEDEFAULT, 0, nullptr, nullptr, hInstance, nullptr);

    if (!mainWindowHandle)
    {
        return FALSE;
    }

    if (!initializeTooltip()) return FALSE;
    
    HDC hdc = GetDC(mainWindowHandle);
    GetTextMetrics(hdc, &textMetric);
    hdcMem = CreateCompatibleDC(hdc);
    loadPngResource(hInstance, IDB_RECORDBTN, recordBtn, hdc, "Record");
    loadPngResource(hInstance, IDB_RECORDBTNHOVER, recordBtnHover, hdc, "RecordHover");
    loadPngResource(hInstance, IDB_RECORDBTNPRESSED, recordBtnPressed, hdc, "RecordPressed");
    loadPngResource(hInstance, IDB_STOPBTN, stopBtn, hdc, "Stop");
    loadPngResource(hInstance, IDB_STOPBTNHOVER, stopBtnHover, hdc, "StopHover");
    loadPngResource(hInstance, IDB_STOPBTNPRESSED, stopBtnPressed, hdc, "StopPressed");
    loadPngResource(hInstance, IDB_PLAYBTN, playBtn, hdc, "Play");
    loadPngResource(hInstance, IDB_PLAYBTNHOVER, playBtnHover, hdc, "PlayHover");
    loadPngResource(hInstance, IDB_PLAYBTNPRESSED, playBtnPressed, hdc, "PlayPressed");
    loadPngResource(hInstance, IDB_PLAYBTNDISABLED, playBtnDisabled, hdc, "PlayDisabled");
    loadPngResource(hInstance, IDB_PAUSEBTN, pauseBtn, hdc, "Pause");
    loadPngResource(hInstance, IDB_PAUSEBTNHOVER, pauseBtnHover, hdc, "PauseHover");
    loadPngResource(hInstance, IDB_PAUSEBTNPRESSED, pauseBtnPressed, hdc, "PausePressed");
    loadPngResource(hInstance, IDB_FOLLOWBTN, followBtn, hdc, "Follow");
    loadPngResource(hInstance, IDB_FOLLOWBTNHOVER, followBtnHover, hdc, "FollowHover");
    loadPngResource(hInstance, IDB_FOLLOWBTNTOGGLED, followBtnToggled, hdc, "FollowToggled");
    loadPngResource(hInstance, IDB_FOLLOWBTNTOGGLEDHOVER, followBtnToggledHover, hdc, "FollowToggledHover");
    loadPngResource(hInstance, IDB_FOLLOWBTNPRESSED, followBtnPressed, hdc, "FollowPressed");
    loadPngResource(hInstance, IDB_STEPLEFTBTN, stepLeftBtn, hdc, "StepLeft");
    loadPngResource(hInstance, IDB_STEPLEFTBTNHOVER, stepLeftBtnHover, hdc, "StepLeftHover");
    loadPngResource(hInstance, IDB_STEPLEFTBTNPRESSED, stepLeftBtnPressed, hdc, "StepLeftPressed");
    loadPngResource(hInstance, IDB_STEPLEFTBTNDISABLED, stepLeftBtnDisabled, hdc, "StepLeftDisabled");
    loadPngResource(hInstance, IDB_STEPRIGHTBTN, stepRightBtn, hdc, "StepRight");
    loadPngResource(hInstance, IDB_STEPRIGHTBTNHOVER, stepRightBtnHover, hdc, "StepRightHover");
    loadPngResource(hInstance, IDB_STEPRIGHTBTNPRESSED, stepRightBtnPressed, hdc, "StepRightPressed");
    loadPngResource(hInstance, IDB_STEPRIGHTBTNDISABLED, stepRightBtnDisabled, hdc, "StepRightDisabled");
    if (!inputsDrawing.load(hdc, hInstance)) return FALSE;
    grayColor = CreateSolidBrush(RGB(0x55, 0x55, 0x55));
    if (!grayColor) {
        WinError winErr;
        debugMsg("CreateSolidBrush failed: %.8x\n", winErr.getMessage());
        return FALSE;
    }
    ReleaseDC(mainWindowHandle, hdc);
    pngButtonManager.callback = buttonClicked;


    layout.update();

    pngButtonManager.addCircularButton(RECORD_BUTTON, layout.buttonbar.left + 5, 5, 25, recordBtn, recordBtnHover, recordBtnPressed);
    pngButtonManager.addTooltip(RECORD_BUTTON, TEXT("Record (R)"));
    pngButtonManager.addResources(RECORD_BUTTON, stopBtn, stopBtnHover, stopBtnPressed);
    pngButtonManager.addTooltip(RECORD_BUTTON, TEXT("Stop recording (R)"));

    pngButtonManager.addCircularButton(PLAY_BUTTON, layout.buttonbar.left + 60, 5, 25, playBtn, playBtnHover, playBtnPressed);
    pngButtonManager.addTooltip(PLAY_BUTTON, TEXT("Play (Space)"));
    pngButtonManager.addResources(PLAY_BUTTON, pauseBtn, pauseBtnHover, pauseBtnPressed);
    pngButtonManager.addTooltip(PLAY_BUTTON, TEXT("Pause (Space)"));
    pngButtonManager.addResources(PLAY_BUTTON, playBtnDisabled, playBtnDisabled, playBtnDisabled);
    pngButtonManager.addTooltip(PLAY_BUTTON, TEXT(""));
    pngButtonManager.setResourceIndex(PLAY_BUTTON, 2);

    pngButtonManager.addRectangularButton(LEFT_FOLLOW_BUTTON, layout.leftPanel.left + 5, 5, (int)followBtn.width, (int)followBtn.height, followBtn, followBtnHover, followBtnPressed);
    pngButtonManager.addTooltip(LEFT_FOLLOW_BUTTON, TEXT("Follow playback"));
    pngButtonManager.addResources(LEFT_FOLLOW_BUTTON, followBtnToggled, followBtnToggledHover, followBtnPressed);
    pngButtonManager.addTooltip(LEFT_FOLLOW_BUTTON, TEXT("Stop following playback"));
    pngButtonManager.setResourceIndex(LEFT_FOLLOW_BUTTON, 1);

    pngButtonManager.addRectangularButton(RIGHT_FOLLOW_BUTTON, layout.rightPanel.left + 5, 5, (int)followBtn.width, (int)followBtn.height, followBtn, followBtnHover, followBtnPressed);
    pngButtonManager.addTooltip(RIGHT_FOLLOW_BUTTON, TEXT("Follow playback"));
    pngButtonManager.addResources(RIGHT_FOLLOW_BUTTON, followBtnToggled, followBtnToggledHover, followBtnPressed);
    pngButtonManager.addTooltip(RIGHT_FOLLOW_BUTTON, TEXT("Stop following playback"));
    pngButtonManager.setResourceIndex(RIGHT_FOLLOW_BUTTON, 1);

    pngButtonManager.addCircularButton(STEP_LEFT_BUTTON, layout.buttonbar.left + 115, 5, 25, stepLeftBtn, stepLeftBtnHover, stepLeftBtnPressed);
    pngButtonManager.addTooltip(STEP_LEFT_BUTTON, TEXT("Step one frame backwards (< or ,)"));
    pngButtonManager.addResources(STEP_LEFT_BUTTON, stepLeftBtnDisabled, stepLeftBtnDisabled, stepLeftBtnDisabled);
    pngButtonManager.addTooltip(STEP_LEFT_BUTTON, TEXT(""));
    pngButtonManager.setResourceIndex(STEP_LEFT_BUTTON, 1);

    pngButtonManager.addCircularButton(STEP_RIGHT_BUTTON, layout.buttonbar.left + 170, 5, 25, stepRightBtn, stepRightBtnHover, stepRightBtnPressed);
    pngButtonManager.addTooltip(STEP_RIGHT_BUTTON, TEXT("Step one frame forward (> or .)"));
    pngButtonManager.addResources(STEP_RIGHT_BUTTON, stepRightBtnDisabled, stepRightBtnDisabled, stepRightBtnDisabled);
    pngButtonManager.addTooltip(STEP_RIGHT_BUTTON, TEXT(""));
    pngButtonManager.setResourceIndex(STEP_RIGHT_BUTTON, 1);

    leftScrollbar = CreateWindowEx(
        0,
        L"SCROLLBAR",
        NULL,
        WS_CHILD | WS_VISIBLE | SBS_VERT | SBS_RIGHTALIGN,
        layout.leftPanelData.left,
        layout.leftPanelData.top,
        layout.leftPanelData.right - layout.leftPanelData.left,
        layout.leftPanelData.bottom - layout.leftPanelData.top,
        mainWindowHandle,
        NULL,
        hInstance,
        NULL
    );
    if (!leftScrollbar) {
        WinError winErr;
        debugMsg("Failed to create left scrollbar: %ls\n", winErr.getMessage());
        return FALSE;
    }

    rightScrollbar = CreateWindowEx(
        0,
        L"SCROLLBAR",
        NULL,
        WS_CHILD | WS_VISIBLE | SBS_VERT | SBS_RIGHTALIGN,
        layout.rightPanelData.left,
        layout.rightPanelData.top,
        layout.rightPanelData.right - layout.rightPanelData.left,
        layout.rightPanelData.bottom - layout.rightPanelData.top,
        mainWindowHandle,
        NULL,
        hInstance,
        NULL
    );
    if (!rightScrollbar) {
        WinError winErr;
        debugMsg("Failed to create right scrollbar: %ls\n", winErr.getMessage());
        return FALSE;
    }


    inputsDrawing.panels[0].thisPanelIndex = 0;
    inputsDrawing.panels[0].panelRect = &layout.leftPanel;
    inputsDrawing.panels[0].panelRectData = &layout.leftPanelData;
    inputsDrawing.panels[0].buttonId = LEFT_FOLLOW_BUTTON;
    inputsDrawing.panels[0].scrollbarHandle = leftScrollbar;
    inputsDrawing.panels[1].thisPanelIndex = 1;
    inputsDrawing.panels[1].panelRect = &layout.rightPanel;
    inputsDrawing.panels[1].panelRectData = &layout.rightPanelData;
    inputsDrawing.panels[1].buttonId = RIGHT_FOLLOW_BUTTON;
    inputsDrawing.panels[1].scrollbarHandle = rightScrollbar;
    for (int i = 0; i < 2; ++i) {
        inputsDrawing.panels[i].updateScrollbarInfo();
    }

    ctxStandardFlags = GetSystemMetrics(SM_MENUDROPALIGNMENT);
    ctxMenu = LoadMenu(hInstance, MAKEINTRESOURCE(IDR_CTXMENU));

    comboBoxItemNames.push_back(TEXT("...every frame."));
    comboBoxItemNames.push_back(TEXT("...every second frame."));
    comboBoxItemNames.push_back(TEXT("...every third frame."));
    comboBoxItemNames.push_back(TEXT("...every fourth frame."));

    readSettings();
    if (displayBoxes || displayInputs) {
        std::unique_lock<std::mutex> guard(ggProcessHandleMutex);
        if (initializeInterprocess(false)) {
            fileMappingManager.pushSettingsUpdate(displayBoxes, displayInputs);
        }
    }

    ShowWindow(mainWindowHandle, nCmdShow);
    UpdateWindow(mainWindowHandle);


    return TRUE;
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{

    switch (message)
    {
    case WM_COMMAND:
        {
            int wmId = LOWORD(wParam);
            // Parse the menu selections:
            switch (wmId)
            {
            case IDM_ABOUT:
                DialogBox(hInst, MAKEINTRESOURCE(IDD_ABOUTBOX), hWnd, About);
                break;
            case ID_FILE_OPEN:
                onCtrlO();
                break;
            case ID_EDIT_SELECTALL:
                makeAtLeastOnePanelMarkedAsLastClicked();
                onSelectAll();
                break;
            case ID_EDIT_COPY:
                makeAtLeastOnePanelMarkedAsLastClicked();
                onCopy();
                break;
            case ID_EDIT_SETTINGS:
                openSettings();
                break;
            case IDM_EXIT:
                DestroyWindow(hWnd);
                break;
            default:
                return DefWindowProc(hWnd, message, wParam, lParam);
            }
        }
        break;
    case WM_VSCROLL: {
        // lParam - HANDLE to the scrollbar control
        WORD scrollingRequest = LOWORD(wParam);
        WORD scrollPosition = HIWORD(wParam);
        if ((HWND)lParam == leftScrollbar) {
            inputsDrawing.panels[0].onScrollEvent(scrollingRequest, scrollPosition);
        } else if ((HWND)lParam == rightScrollbar) {
            inputsDrawing.panels[1].onScrollEvent(scrollingRequest, scrollPosition);
        }
        // SB_BOTTOM - Scrolls to the lower right.
        // SB_LINEDOWN - Scrolls one line down.
        // SB_LINEUP - Scrolls one line up.
        // SB_PAGEDOWN - Scrolls one page down.
        // SB_PAGEUP - Scrolls one page up.
        // SB_THUMBTRACK - The user is dragging the scroll box. This message is sent repeatedly until the user releases the mouse button. The HIWORD indicates the position that the scroll box has been dragged to.
        // SB_TOP - Scrolls to the upper left.
        // SB_ENDSCROLL
    }
    break;
    case WM_PAINT:
    {
        std::unique_lock<std::mutex> guard(hdcMemMutex);
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hWnd, &ps);
        pngButtonManager.onWmPaint(ps, hdc);
        presentationThread.onWmPaint(ps, hdc);
        if (rectsIntersect(&ps.rcPaint, &layout.leftPanelLine)) {
            FillRect(hdc, &layout.leftPanelLine, grayColor);
        }
        if (rectsIntersect(&ps.rcPaint, &layout.rightPanelLine)) {
            FillRect(hdc, &layout.rightPanelLine, grayColor);
        }
        if (rectsIntersect(&ps.rcPaint, &layout.leftPanelHorizLine)) {
            FillRect(hdc, &layout.leftPanelHorizLine, grayColor);
        }
        if (rectsIntersect(&ps.rcPaint, &layout.rightPanelLine)) {
            FillRect(hdc, &layout.rightPanelHorizLine, grayColor);
        }
        inputsDrawing.onWmPaint(ps, hdc);
        EndPaint(hWnd, &ps);
    }
    break;
    case WM_MOUSEMOVE:
    {
        if (mouseLeft) {
            mouseLeft = false;
            TRACKMOUSEEVENT trackMouseEvent;
            trackMouseEvent.cbSize = sizeof(TRACKMOUSEEVENT);
            trackMouseEvent.dwFlags = TME_LEAVE;
            trackMouseEvent.dwHoverTime = 0;
            trackMouseEvent.hwndTrack = mainWindowHandle;
            TrackMouseEvent(&trackMouseEvent);
        }
        if (seekbarIsPressed && !inputsDrawing.seekbarControlledByPanel()) {
            onSeekbarMouseMove();
        } else {
            bool atLeastOnePanelSelecting = false;
            for (int i = 0; i < 2; ++i) {
                if (inputsDrawing.panels[i].isSelecting) {
                    atLeastOnePanelSelecting = true;
                    POINT cursorPos;
                    cursorPos.x = GET_X_LPARAM(lParam);
                    cursorPos.y = GET_Y_LPARAM(lParam);
                    inputsDrawing.panels[i].onMouseMove(cursorPos.x, cursorPos.y);
                    break;
                }
            }
            if (!atLeastOnePanelSelecting) {
                pngButtonManager.onMouseMove(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam), mouseIsDown);
            }
        }
    }
    break;
    case WM_MOUSELEAVE:
    {
        mouseLeft = true;
        pngButtonManager.onMouseMove(-1, -1, mouseIsDown);
    }
    break;
    case WM_LBUTTONDOWN:
    {
        mouseIsDown = true;
        SetCapture(hWnd);
        float ratio;

        POINT cursorPos;
        cursorPos.x = GET_X_LPARAM(lParam);
        cursorPos.y = GET_Y_LPARAM(lParam);

        const bool shiftOrCtrl = (GetKeyState(VK_CONTROL) & 0x8000) != 0 || (GetKeyState(VK_SHIFT) & 0x8000) != 0;

        if (rectHitTest(&layout.leftPanelData, cursorPos.x, cursorPos.y)) {
            inputsDrawing.panels[0].onMouseDown(cursorPos.x, cursorPos.y, shiftOrCtrl);
        } else {
            inputsDrawing.panels[0].isClickedLast = false;
            if (rectHitTest(&layout.rightPanelData, cursorPos.x, cursorPos.y)) {
                inputsDrawing.panels[1].onMouseDown(cursorPos.x, cursorPos.y, shiftOrCtrl);
            } else {
                inputsDrawing.panels[1].isClickedLast = false;
                if (presentationTimer.isFileOpen && seekbarMouseHittest(&ratio, &cursorPos)) {
                    onSeekbarMousePress(ratio, true);
                } else {
                    pngButtonManager.onMouseDown(cursorPos.x, cursorPos.y);
                }
            }
        }
    }
    break;
    case WM_LBUTTONUP:
    {
        mouseIsDown = false;
        ReleaseCapture();
        if (seekbarIsPressed && !inputsDrawing.seekbarControlledByPanel()) {
            onSeekbarMouseUp();
        } else {

            POINT cursorPos;
            cursorPos.x = GET_X_LPARAM(lParam);
            cursorPos.y = GET_Y_LPARAM(lParam);

            bool atLeastOnePanelSelecting = false;
            for (int i = 0; i < 2; ++i) {
                if (inputsDrawing.panels[i].isSelecting) {
                    atLeastOnePanelSelecting = true;
                    inputsDrawing.panels[i].onMouseUp();
                    break;
                }
            }
            if (!atLeastOnePanelSelecting) {
                pngButtonManager.onMouseUp(cursorPos.x, cursorPos.y);
            }
        }
    }
    break;
    case WM_MOUSEWHEEL:
    {
        if (!(seekbarIsPressed && !inputsDrawing.seekbarControlledByPanel())) {
            int wheelDelta = ((long)wParam >> 16);
            POINT cursorPos;
            cursorPos.x = GET_X_LPARAM(lParam);
            cursorPos.y = GET_Y_LPARAM(lParam);
            ScreenToClient(mainWindowHandle, &cursorPos);
            if (rectHitTest(&layout.leftPanelData, cursorPos.x, cursorPos.y)) {
                inputsDrawing.panels[0].onMouseWheel(wheelDelta);
            } else if (rectHitTest(&layout.rightPanelData, cursorPos.x, cursorPos.y)) {
                inputsDrawing.panels[1].onMouseWheel(wheelDelta);
            }
        }
    }
    break;
    case WM_CONTEXTMENU: {
        if ((HWND)wParam == mainWindowHandle) {
            POINT cursorPosScreen;
            POINT cursorPosClient;
            cursorPosScreen.x = GET_X_LPARAM(lParam);
            cursorPosScreen.y = GET_Y_LPARAM(lParam);
            
            bool canCreateMenu = false;

            if (cursorPosScreen.x == -1 && cursorPosScreen.y == -1) {
                if (inputsDrawing.panels[0].isClickedLast) {
                    canCreateMenu = true;
                    cursorPosScreen.x = (layout.leftPanelData.left + layout.leftPanelData.right) / 2;
                    cursorPosScreen.y = (layout.leftPanelData.top + layout.leftPanelData.bottom) / 2;
                }
                if (inputsDrawing.panels[1].isClickedLast) {
                    canCreateMenu = true;
                    cursorPosScreen.x = (layout.rightPanelData.left + layout.rightPanelData.right) / 2;
                    cursorPosScreen.y = (layout.rightPanelData.top + layout.rightPanelData.bottom) / 2;
                }
            } else {
                cursorPosClient = cursorPosScreen;
                ScreenToClient(mainWindowHandle, &cursorPosClient);

                bool atLeastOneHit = false;
                if (rectHitTest(&layout.leftPanelData, cursorPosClient.x, cursorPosClient.y)) {
                    inputsDrawing.panels[0].isClickedLast = true;
                    atLeastOneHit = true;
                }
                else if (rectHitTest(&layout.rightPanelData, cursorPosClient.x, cursorPosClient.y)) {
                    inputsDrawing.panels[1].isClickedLast = true;
                    atLeastOneHit = true;
                }
                if (atLeastOneHit) {
                    canCreateMenu = true;
                }
            }
            if (canCreateMenu) {
                int ctxResult = TrackPopupMenu(GetSubMenu(ctxMenu, 0), ctxStandardFlags | TPM_RETURNCMD, cursorPosScreen.x, cursorPosScreen.y, 0, mainWindowHandle, NULL);
                if (ctxResult == ID_CTXMENU_SELECTALL) {
                    onSelectAll();
                } else if (ctxResult == ID_CTXMENU_COPY) {
                    onCopy();
                }
                break;
            }
        }
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    break;
    case WM_KILLFOCUS:
    {
        mouseIsDown = false;
        ReleaseCapture();
        if (seekbarIsPressed && !inputsDrawing.seekbarControlledByPanel()) {
            onSeekbarMouseUp();
        } else {

            bool atLeastOnePanelSelecting = false;
            for (int i = 0; i < 2; ++i) {
                if (inputsDrawing.panels[i].isSelecting) {
                    atLeastOnePanelSelecting = true;
                    inputsDrawing.panels[i].onMouseUp();
                    break;
                }
            }
            if (!atLeastOnePanelSelecting) {
                pngButtonManager.onMouseMove(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam), mouseIsDown);
            }
        }
    }
    break;
    case WM_KEYDOWN:
    {
        const bool ctrlHeld = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
        const bool shiftHeld = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
        InputsPanel* panel = nullptr;
        for (int i = 0; i < 2; ++i) {
            if (inputsDrawing.panels[i].isClickedLast) {
                panel = &inputsDrawing.panels[i];
                break;
            }
        }
        if ((lParam & 0x40000000) == 0) {
            // key down for the first time of the repeat sequence
            if ((int)wParam == 'O' && ctrlHeld) {
                onCtrlO();
                break;
            }
            if ((int)wParam == 'A' && ctrlHeld) {
                makeAtLeastOnePanelMarkedAsLastClicked();
                onSelectAll();
                break;
            }
            if ((int)wParam == 'C' && ctrlHeld) {
                makeAtLeastOnePanelMarkedAsLastClicked();
                onCopy();
                break;
            }
            if ((int)wParam == 'R') {
                buttonClicked(RECORD_BUTTON);
                break;
            }
            if (wParam == VK_SPACE) {
                buttonClicked(PLAY_BUTTON);
                break;
            }
        }
        if (wParam == VK_RIGHT || wParam == VK_LEFT) {
            onArrowLeftRight(wParam == VK_RIGHT);
            break;
        }
        if (wParam == VK_OEM_PERIOD) {
            buttonClicked(STEP_RIGHT_BUTTON);
            break;
        }
        if (wParam == VK_OEM_COMMA) {
            buttonClicked(STEP_LEFT_BUTTON);
            break;
        }
        if (panel) {
            if (wParam == VK_UP) {
                panel->onArrowUpDown(true, shiftHeld);
                break;
            }
            if (wParam == VK_DOWN) {
                panel->onArrowUpDown(false, shiftHeld);
                break;
            }
            if (wParam == VK_PRIOR) {
                panel->onPageUpDown(true, shiftHeld);
                break;
            }
            if (wParam == VK_NEXT) {
                panel->onPageUpDown(false, shiftHeld);
                break;
            }
            if (wParam == VK_HOME) {
                panel->onHomeEnd(true, shiftHeld);
                break;
            }
            if (wParam == VK_END) {
                panel->onHomeEnd(false, shiftHeld);
                break;
            }
        }
    }
    break;
    case WM_APP: {
        if (lParam <= 1 && lParam >= 0) {
            inputsDrawing.panels[lParam].followToInputBufferIndex((unsigned int)wParam);
        }
    }
    break;
    case WM_SETCURSOR: {
        if ((HWND)wParam == mainWindowHandle) {
            if (LOWORD(lParam) == HTCLIENT) {
                if (presentationTimer.isFileOpen && seekbarMouseHittest()) {
                    SetCursor(handCursor);
                    break;
                }
            }
        }
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    case WM_SIZE: {
        layout.update();

        for (unsigned int i = 0; i < 2; ++i) {
            inputsDrawing.panels[i].moveYourButton();
            inputsDrawing.panels[i].updateScrollbarInfo();
        }

        MoveWindow(leftScrollbar,
            layout.leftPanelData.right - scrollbarWidth,
            layout.leftPanelData.top,
            scrollbarWidth,
            layout.leftPanelData.bottom - layout.leftPanelData.top,
            true);

        MoveWindow(rightScrollbar,
            layout.rightPanelData.right - scrollbarWidth,
            layout.rightPanelData.top,
            scrollbarWidth,
            layout.rightPanelData.bottom - layout.rightPanelData.top,
            true);

    }
    break;
    case WM_DESTROY:
        SetEvent(eventApplicationIsClosing);
        if (ggProcessWatcherIsRunning) {
            ggProcessWatcher.join();
        }
        fileMappingManager.joinAllThreads();
        if (encoderThreadStarted) {
            encoderThreadThread.join();
        }
        if (fileWriterThreadStarted) {
            fileWriterThreadThread.join();
        }
        if (fileReaderThreadStarted) {
            fileReaderThreadThread.join();
        }
        if (decoderThreadStarted) {
            decoderThreadThread.join();
        }
        if (presentationThreadStarted) {
            presentationThreadThread.join();
        }
        encoderInput.destroy();
        fileWriterInput.destroy();
        decoderInput.destroy();
        presentationInput.destroy();
        presentationThread.destroy();
        memoryProvisioner.destroy();
        codec.destroy();
        replayFileData.closeFile();
        if (fileMappingManager.view) {
            FileMappingGuard mapGuard;
            mapGuard.markWroteSomething();
            debugMsg("Set view->unloadDll to true\n");
            fileMappingManager.view->unloadDll = true;
            FlushViewOfFile((LPCVOID)fileMappingManager.view, sizeof(FileMapping));
            UnmapViewOfFile((LPCVOID)fileMappingManager.view);
            fileMappingManager.view = nullptr;
            CloseHandle(fileMappingHandle);
            fileMappingHandle = NULL;
        }
        writeSettings();
        PostQuitMessage(0);
        break;
    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}

// Message handler for about box.
INT_PTR CALLBACK About(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(lParam);
    switch (message)
    {
    case WM_INITDIALOG:
        return (INT_PTR)TRUE;

    case WM_COMMAND:
        if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)
        {
            EndDialog(hDlg, LOWORD(wParam));
            return (INT_PTR)TRUE;
        }
        break;
    }
    return (INT_PTR)FALSE;
}

INT_PTR CALLBACK CtxDialogMsgHandler(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(lParam);
    switch (message)
    {
    case WM_INITDIALOG:
        Edit_SetText(GetDlgItem(hDlg, IDC_CTXDIALOG_EDIT), ctxDialogText.c_str());
        return (INT_PTR)TRUE;

    case WM_COMMAND:
        if (LOWORD(wParam) == IDC_CTXDIALOG_MIRRORINPUTS) {
            bool encounteredMultiplication = false;
            for (TCHAR& c : ctxDialogText) {
                if (c == TEXT('*')) {
                    encounteredMultiplication = true;
                    continue;
                } else if (c == TEXT(',')) {
                    encounteredMultiplication = false;
                    continue;
                }
                if (!encounteredMultiplication) {
                    char cCopy = (char)c;
                    char newC = inputsDrawing.mirrorDirection(cCopy);
                    if (newC != cCopy) {
                        c = (TCHAR)newC;
                    }
                }
            }
            Edit_SetText(GetDlgItem(hDlg, IDC_CTXDIALOG_EDIT), ctxDialogText.c_str());
        } else if (LOWORD(wParam) == IDCANCEL)
        {
            EndDialog(hDlg, LOWORD(wParam));
            return (INT_PTR)TRUE;
        }
        break;
    }
    return (INT_PTR)FALSE;
}

INT_PTR CALLBACK settingsHandler(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam) {
    UNREFERENCED_PARAMETER(lParam);
    switch (message)
    {
    case WM_INITDIALOG: {
        HWND cbHwnd = GetDlgItem(hDlg, IDC_SETTINGS_FRAMESKIP);
        for (debugStrStrType& name : comboBoxItemNames) {
            SendMessage(cbHwnd, (UINT)CB_ADDSTRING, (WPARAM)0, (LPARAM)name.c_str());
        }
        SendMessage(cbHwnd, CB_SETCURSEL, (WPARAM)frameskip, (LPARAM)0);
        CheckDlgButton(hDlg, IDC_SETTINGS_DISPLAYHITBOXES, (displayBoxes ? BST_CHECKED : BST_UNCHECKED));
        CheckDlgButton(hDlg, IDC_SETTINGS_DISPLAYINPUTS, (displayInputs ? BST_CHECKED : BST_UNCHECKED));
        return (INT_PTR)TRUE;
    }

    case WM_COMMAND: {
        if (LOWORD(wParam) == IDOK) {
            frameskip = (int)SendMessage(GetDlgItem(hDlg, IDC_SETTINGS_FRAMESKIP), (UINT)CB_GETCURSEL, (WPARAM)0, (LPARAM)0);
            if (frameskip < 0) frameskip = 0;
            UINT displayHitboxesChecked = IsDlgButtonChecked(hDlg, IDC_SETTINGS_DISPLAYHITBOXES);
            UINT displayInputsChecked = IsDlgButtonChecked(hDlg, IDC_SETTINGS_DISPLAYINPUTS);
            displayBoxes = (displayHitboxesChecked == BST_CHECKED);
            displayInputs = (displayInputsChecked == BST_CHECKED);
            {
                std::unique_lock<std::mutex> guard(ggProcessHandleMutex);
                if (displayBoxes || displayInputs || ggProcessHandle) {
                    if (initializeInterprocess(false)) {
                        fileMappingManager.pushSettingsUpdate(displayBoxes, displayInputs);
                    }
                }
            }
            EndDialog(hDlg, LOWORD(wParam));
            return (INT_PTR)TRUE;
        } else if (LOWORD(wParam) == IDCANCEL) {
            EndDialog(hDlg, LOWORD(wParam));
            return (INT_PTR)TRUE;
        }
        break;
    }
    }
    return (INT_PTR)FALSE;
}

void startRecording() {
    if (!isRecordingGlobal) {
        if (encoderThread.crashed) {
            MessageBox(mainWindowHandle, TEXT("Encoder has crashed. Cannot continue."), TEXT("Error"), MB_OK);
            return;
        }
        if (!stopPlayback()) return;
        encoderThread.reset();
        pngButtonManager.setResourceIndex(PLAY_BUTTON, 2);
        pngButtonManager.setResourceIndex(STEP_LEFT_BUTTON, 1);
        pngButtonManager.setResourceIndex(STEP_RIGHT_BUTTON, 1);
        {
            std::wstring szFile;
            if (!selectRplFile(true, szFile)) return;

            if (!replayFileData.startFile(szFile.c_str(), false)) return;

            fseek(replayFileData.file, sizeof(ReplayFileHeader), SEEK_SET);
        }
        {
            std::unique_lock<std::mutex> guard(ggProcessHandleMutex);
            if (!initializeInterprocess(true)) return;
            fileMappingManager.pushSettingsUpdate(displayBoxes, displayInputs);
            fileMappingManager.sendStartRecordingCommand();
            pngButtonManager.setResourceIndex(RECORD_BUTTON, 1);
            isRecordingGlobal = true;
        }
    } else {
        {
            FileMappingGuard mapGuard;
            fileMappingManager.view->recording = false;
            fileMappingManager.view->needStartRecording = false;
            debugMsg("Sent recording stop request\n");
            mapGuard.markWroteSomething();
        }
        if (encoderThread.crashed) {
            Sleep(500);
        }
        if (!encoderThread.crashed) {
            {
                std::unique_lock<std::mutex> fileWriterGuard(fileWriterInput.mutex);
                fileWriterInput.needNotifyEof = true;
                SetEvent(fileWriterInput.event);
            }
            {
                HANDLE objects[2] = { eventApplicationIsClosing, fileWriterThread.eventResponse };
                DWORD waitResult = WaitForMultipleObjects(2, objects, FALSE, INFINITE);
                if (waitResult == WAIT_OBJECT_0) {
                    return;
                } else if (waitResult != WAIT_OBJECT_0 + 1) {
                    debugMsg("Abnormal wait result in startRecording: %.8x\n", waitResult);
                    return;
                }
            }
            writeFileSupplementaries();
        }
        replayFileData.closeFile();
        {
            std::unique_lock<std::mutex> presentationGuard(presentationTimer.mutex);
            presentationTimer.isFileOpen = false;
        }
        isRecordingGlobal = false;
        pngButtonManager.setResourceIndex(RECORD_BUTTON, 0);
    }
}

void buttonClicked(unsigned int id) {
    if (id == RECORD_BUTTON) {
        if (seekbarIsPressed) return;
        startRecording();
        return;
    }
    if (id == PLAY_BUTTON) {
        if (seekbarIsPressed) return;
        if (!presentationTimer.isFileOpen) return;
        bool isPlaying = false;
        {
            std::unique_lock<std::mutex> presentationTimerGuard(presentationTimer.mutex);
            presentationTimer.isPlaying = !presentationTimer.isPlaying;
            isPlaying = presentationTimer.isPlaying;
            SetEvent(presentationTimer.eventForPresentationThread);
        }
        pngButtonManager.setResourceIndex(PLAY_BUTTON, isPlaying ? 1 : 0);
        return;
    }
    if (id == STEP_LEFT_BUTTON || id == STEP_RIGHT_BUTTON) {
        if (seekbarIsPressed) return;
        if (!presentationTimer.isFileOpen) return;
        bool wasPlaying = false;
        {
            std::unique_lock<std::mutex> presentationTimerGuard(presentationTimer.mutex);
            wasPlaying = presentationTimer.isPlaying;
            if (wasPlaying) {
                presentationTimer.isPlaying = false;
                SetEvent(presentationTimer.eventForPresentationThread);
            }
        }
        if (wasPlaying) {
            pngButtonManager.setResourceIndex(PLAY_BUTTON, 0);
            return;
        }
        unsigned int currentMatchCounter = 0;
        {
            std::unique_lock<std::mutex> presentationTimerGuard(presentationTimer.mutex);
            currentMatchCounter = presentationTimer.matchCounter;
        }
        size_t indexSize;
        if (id == STEP_RIGHT_BUTTON) {
            {
                std::unique_lock<std::mutex> replayIndexGuard(replayFileData.indexMutex);
                indexSize = replayFileData.index.size();
            }
            if (currentMatchCounter <= 0xFFFFFFFF - (indexSize - 1) || currentMatchCounter == 0) {
                return;
            }
            --currentMatchCounter;
        } else if (id == STEP_LEFT_BUTTON) {
            if (currentMatchCounter == 0xFFFFFFFF) {
                return;
            }
            ++currentMatchCounter;
        }
        onSeekbarMousePress(currentMatchCounter, true);
        onSeekbarMouseUp();
        return;
    }
    if (id == LEFT_FOLLOW_BUTTON) {
        inputsDrawing.panels[0].buttonClicked();
        return;
    }
    if (id == RIGHT_FOLLOW_BUTTON) {
        inputsDrawing.panels[1].buttonClicked();
        return;
    }
}

void ggProcessWatcherProc() {
    while (true) {
        HANDLE events[3] = { eventApplicationIsClosing, eventGgProcessHandleObtained, ggProcessHandle };  // fingers crossed that the ggProcessHandle gets read atomically
        DWORD objectsCount = 3;
        if (!events[2]) {
            objectsCount = 2;
        }
        DWORD waitResult = WaitForMultipleObjects(objectsCount, events, FALSE, INFINITE);
        if (waitResult == WAIT_OBJECT_0) {
            return;
        } else if (waitResult == WAIT_OBJECT_0 + 1) {
            continue;
        } else if (waitResult == WAIT_OBJECT_0 + 2) {
            std::unique_lock<std::mutex> guard(ggProcessHandleMutex);
            CloseHandle(ggProcessHandle);  // no one can set the handle to NULL except this thread
            ggProcessHandle = NULL;
            ggProcessId = 0;
            SetEvent(eventGgProcessExited);
        } else {
            debugMsg("ggProcessWatcherProc abnormal result (in decimal): %u\n", waitResult);
            return;
        }
    }
}

bool initializeInterprocess(bool needShowErrorMessageBoxes) {
    class Exiter {
    public:
        bool needSetGgProcessIdToNull = false;
        std::vector<HANDLE> handlesToClose;
        LPVOID thingToVirtualFreeEx = NULL;
        ~Exiter() {
            for (auto it = handlesToClose.begin(); it != handlesToClose.end(); ++it) {
                CloseHandle(*it);
            }
            if (thingToVirtualFreeEx) {
                VirtualFreeEx(ggProcessHandle, thingToVirtualFreeEx, 0, MEM_RELEASE);
            }
            if (needSetGgProcessIdToNull) {
                ggProcessHandle = NULL;
            }
        }
    } exiter;

    if (!ggProcessHandle) {
        PROCESSENTRY32 p32 = findProcess(PROCESS_NAME);
        if (p32.th32ProcessID == 0) {
            showOrLogError(TEXT("Could not find ") PROCESS_NAME TEXT(" process.\nLaunch the game and try again."), needShowErrorMessageBoxes);
            return false;
        }
        ggProcessId = p32.th32ProcessID;
        ggProcessHandle = OpenProcess(PROCESS_ALL_ACCESS, FALSE, ggProcessId);
        if (!ggProcessHandle) {
            showLastWinError(TEXT("Couldn't open ") PROCESS_NAME TEXT(" process: "), needShowErrorMessageBoxes);
            return false;
        }
        exiter.handlesToClose.push_back(ggProcessHandle);
        exiter.needSetGgProcessIdToNull = true;
        if (!fileMappingHandle) {
            fileMappingHandle = CreateFileMappingW(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, MAX_FILEMAP_SIZE, TEXT("ggxrd_replay_file_mapping"));
            if (!fileMappingHandle) {
                showLastWinError(TEXT("Failed to create a file mapping: "), needShowErrorMessageBoxes);
                return false;
            }
        }
        if (!fileMappingManager.view) {
            fileMappingManager.view = (FileMapping*)MapViewOfFile(fileMappingHandle, FILE_MAP_ALL_ACCESS, 0, 0, MAX_FILEMAP_SIZE);
            if (!fileMappingManager.view) {
                showLastWinError(TEXT("Failed to map view of 'file' through MapViewOfFile: "), needShowErrorMessageBoxes);
                return false;
            }
            memset((void*)fileMappingManager.view, 0, MAX_FILEMAP_SIZE);
        }
        if (!eventChangesMade) {
            eventChangesMade = CreateEventW(NULL, FALSE, FALSE, L"ggxrd_replay_app_wrote");
            if (!eventChangesMade) {
                showLastWinError(TEXT("Received error from OpenEventW when creating ggxrd_replay_app_wrote: "), needShowErrorMessageBoxes);
                return false;
            }
        }
        if (!eventChangesReceived) {
            eventChangesReceived = CreateEventW(NULL, FALSE, FALSE, L"ggxrd_replay_dll_wrote");
            if (!eventChangesReceived) {
                showLastWinError(TEXT("Received error from OpenEventW when creating ggxrd_replay_dll_wrote: "), needShowErrorMessageBoxes);
                return false;
            }
        }
        fileMappingManager.startThread();
        if (!fileMapMutex) {
            fileMapMutex = CreateMutexW(NULL, FALSE, L"ggxrd_replay_mutex");
            if (!fileMapMutex) {
                showLastWinError(TEXT("Received error from CreateMutexW when creating ggxrd_replay_mutex: "), needShowErrorMessageBoxes);
                return false;
            }
        }
        if (!encoderInput.event) {
            encoderInput.event = CreateEventW(NULL, FALSE, FALSE, NULL);
            if (!encoderInput.event) {
                showLastWinError(TEXT("Received error from CreateEventW when creating encoderInput.event: "), needShowErrorMessageBoxes);
                return false;
            }
        }
        if (!fileWriterInput.event) {
            fileWriterInput.event = CreateEventW(NULL, FALSE, FALSE, NULL);
            if (!fileWriterInput.event) {
                showLastWinError(TEXT("Received error from CreateEventW when creating fileWriterInput.event: "), needShowErrorMessageBoxes);
                return false;
            }
        }
        MODULEENTRY32 m32 = findModule(ggProcessId, DLL_NAME, true);
        if (m32.modBaseAddr == 0) {
            DWORD dllAtrib = GetFileAttributes(dllPath);
            if (dllAtrib == INVALID_FILE_ATTRIBUTES) {
                showLastWinError(debugStr(TEXT("Couldn't find the DLL (") PRINTF_STRING_ARG TEXT(") to inject into the process: "), dllPath).c_str(), needShowErrorMessageBoxes);
                return false;
            }
            const auto size = (_tcsclen(dllPath) + 1) * sizeof(TCHAR);
            LPVOID buf = VirtualAllocEx(ggProcessHandle, nullptr, size, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
            if (buf == NULL) {
                showLastWinError(TEXT("Failed to allocate memory: "), needShowErrorMessageBoxes);
                return false;
            }
            exiter.thingToVirtualFreeEx = buf;
            if (!WriteProcessMemory(ggProcessHandle, buf, dllPath, size, nullptr)) {
                showLastWinError(TEXT("Failed to write process memory: "), needShowErrorMessageBoxes);
                return false;
            }
            LPTHREAD_START_ROUTINE remoteAddrOfLoadLibrary = NULL;
            if (sizeof(void*) == 4) {
                remoteAddrOfLoadLibrary = (LPTHREAD_START_ROUTINE)LoadLibrary;
            }
            else {
                unsigned int loadLibraryAPtr = 0;
                MODULEENTRY32 ggM32 = findModule(ggProcessId, PROCESS_NAME, true);
                if (!ggM32.modBaseAddr) {
                    showOrLogError(TEXT("Could not find ") PROCESS_NAME TEXT(" module within the process."), needShowErrorMessageBoxes);
                    return false;
                }
                std::vector<char> moduleBytes;
                if (!readWholeModule(ggProcessHandle, ggM32.modBaseAddr, ggM32.modBaseSize, moduleBytes)) {
                    showOrLogError(TEXT("Failed to read whole ") PROCESS_NAME TEXT(" module from the process."), needShowErrorMessageBoxes);
                    return false;
                }
                #ifndef UNICODE
                // ghidra sig: ff 75 c8 ff 15 ?? ?? ?? ?? 8b f8 85 ff 75 41 ff 15 ?? ?? ?? ?? 89 45 dc a1 ?? ?? ?? ?? 85 c0 74 0e
                char* sigscanResult = sigscan(moduleBytes,
                    "\xff\x75\xc8\xff\x15\x00\x00\x00\x00\x8b\xf8\x85\xff\x75\x41\xff\x15\x00\x00\x00\x00\x89\x45\xdc\xa1\x00\x00\x00\x00\x85\xc0\x74\x0e",
                    "xxxxx????xxxxxxxx????xxxx????xxxx");
                if (!sigscanResult) {
                    showOrLogError(TEXT("Failed to find LoadLibraryA calling place."), needShowErrorMessageBoxes);
                    return false;
                }
                loadLibraryAPtr = *(unsigned int*)(sigscanResult + 5);
                #else
                // ghidra sig: eb 05 b8 ?? ?? ?? ?? 50 ff 15 ?? ?? ?? ?? 8b f0 3b f7 74 68
                char* sigscanResult = sigscan(moduleBytes,
                    "\xeb\x05\xb8\x00\x00\x00\x00\x50\xff\x15\x00\x00\x00\x00\x8b\xf0\x3b\xf7\x74\x68",
                    "xxx????xxx????xxxxxx");
                if (!sigscanResult) {
                    showOrLogError(TEXT("Failed to find LoadLibraryW calling place."), needShowErrorMessageBoxes);
                    return false;
                }
                loadLibraryAPtr = *(unsigned int*)(sigscanResult + 10);
                #endif
                loadLibraryAPtr = *(unsigned int*)((loadLibraryAPtr - (unsigned int)ggM32.modBaseAddr) + &moduleBytes.front());
                remoteAddrOfLoadLibrary = (LPTHREAD_START_ROUTINE)loadLibraryAPtr;
            }
            HANDLE newThread = NULL;
            if (remoteAddrOfLoadLibrary) {
                newThread = CreateRemoteThread(ggProcessHandle, nullptr, 0, remoteAddrOfLoadLibrary, buf, 0, nullptr);
            }
            if (newThread == INVALID_HANDLE_VALUE || newThread == NULL) {
                showLastWinError(TEXT("Failed to create remote thread: "), needShowErrorMessageBoxes);
                return false;
            }
            exiter.handlesToClose.push_back(newThread);
            if (WaitForSingleObject(newThread, INFINITE) != WAIT_OBJECT_0) {
                showLastWinError(TEXT("Error waiting for the injected thread: "), needShowErrorMessageBoxes);
                return false;
            }
            CloseHandle(newThread);
            VirtualFreeEx(ggProcessHandle, buf, 0, MEM_RELEASE);
            exiter.thingToVirtualFreeEx = NULL;
        }
        exiter.needSetGgProcessIdToNull = false;
        exiter.handlesToClose.clear();
        if (ggProcessHandle) {
            if (!ggProcessWatcherIsRunning) {
                ggProcessWatcherIsRunning = true;
                ggProcessWatcher = std::thread(ggProcessWatcherProc);
            }
            else {
                SetEvent(eventGgProcessHandleObtained);
            }
        }
        if (!encoderThreadStarted) {
            encoderThreadStarted = true;
            encoderThreadThread = std::thread([](EncoderThread* thisArg) {
                thisArg->threadLoop();
            }, &encoderThread);
        }
        if (!fileWriterThreadStarted) {
            fileWriterThreadStarted = true;
            fileWriterThreadThread = std::thread([](FileWriterThread* thisArg) {
                thisArg->threadLoop();
                }, &fileWriterThread);
        }
        if (!initializePlayback()) return false;
    }
    return true;
}

void onCtrlO() {
    if (isRecordingGlobal) {
        MessageBox(mainWindowHandle, TEXT("Cannot play files while recording."), TEXT("Error"), MB_OK);
        return;
    }
    if (seekbarIsPressed || inputsDrawing.aPanelIsSelecting()) return;
    std::wstring szFile;
    if (!selectRplFile(false, szFile)) return;

    if (!initializePlayback()) return;

    if (!stopPlayback()) return;

    if (!replayFileData.startFile(szFile.c_str(), true)) {
        MessageBox(mainWindowHandle, TEXT("Failed to open file."), TEXT("Error"), MB_OK);
        return;
    }

    inputsDrawing.resetFirstDraw();

    if (!readSupplementaries()) return;

    for (int i = 0; i < 2; ++i) {
        InputsPanel& panel = inputsDrawing.panels[i];
        panel.selectionStart = 0xFFFFFFFF;
        panel.selectionEnd = 0xFFFFFFFF;
        panel.commands.clear();
        panel.commandsIndex.clear();
        panel.firstStartIndex = 0xFFFFFFFF;
        panel.lastEndIndex = 0;
        panel.isFirstDraw = true;

        unsigned int newSize = 0;
        size_t inputsSize = replayFileData.replayInputs[i].size();
        panel.commands.resize(inputsSize);
        panel.commandsIndex.resize(inputsSize);
        inputsDrawing.produceData(replayFileData.replayInputs[i],
                                  &panel.commands.front(),
                                  &panel.commandsIndex.front(),
                                  &newSize,
                                  &panel.firstStartIndex,
                                  &panel.lastEndIndex,
                                  0,
                                  (unsigned int)inputsSize);
        panel.commands.resize(newSize);
        panel.currentCommandIndex = 0xFFFFFFFF;
        panel.scrollToCommandIndex(0xFFFFFFFF, 0, true);
        panel.updateScrollbarInfo();
    }

    pngButtonManager.setResourceIndex(PLAY_BUTTON, 0);
    pngButtonManager.setResourceIndex(STEP_LEFT_BUTTON, 0);
    pngButtonManager.setResourceIndex(STEP_RIGHT_BUTTON, 0);

    {
        std::unique_lock<std::mutex> presentationTimeGuard(presentationTimer.mutex);
        presentationTimer.isPlaying = false;
        presentationTimer.matchCounter = 0xFFFFFFFF;
        presentationTimer.isFileOpen = true;
        SetEvent(presentationTimer.eventForFileReader);
    }

}

bool initializePlayback() {
    
    if (decoderThread.crashed) {
        MessageBox(mainWindowHandle, TEXT("Decoder has crashed. Cannot continue."), TEXT("Error"), MB_OK);
        return false;
    }

    if (!presentationThread.initializeDirect3D()) {
        MessageBox(mainWindowHandle, TEXT("Failed to initialize Direct3D."), TEXT("Error"), MB_OK);
        return false;
    }

    if (!decoderInput.event) {
        decoderInput.event = CreateEventW(NULL, FALSE, FALSE, NULL);
        if (!decoderInput.event) {
            showLastWinError(TEXT("Received error from CreateEventW when creating decoderInput.event: "), true);
            return false;
        }
    }
    if (!presentationTimer.eventForFileReader) {
        presentationTimer.eventForFileReader = CreateEventW(NULL, FALSE, FALSE, NULL);
        if (!presentationTimer.eventForFileReader) {
            showLastWinError(TEXT("Received error from CreateEventW when creating presentationTimer.eventForFileReader: "), true);
            return false;
        }
    }
    if (!presentationTimer.eventForPresentationThread) {
        presentationTimer.eventForPresentationThread = CreateEventW(NULL, FALSE, FALSE, NULL);
        if (!presentationTimer.eventForPresentationThread) {
            showLastWinError(TEXT("Received error from CreateEventW when creating presentationTimer.eventForPresentationThread: "), true);
            return false;
        }
    }
    if (!presentationInput.event) {
        presentationInput.event = CreateEventW(NULL, FALSE, FALSE, NULL);
        if (!presentationInput.event) {
            showLastWinError(TEXT("Received error from CreateEventW when creating presentationInput.event: "), true);
            return false;
        }
    }
    if (!fileWriterThread.eventResponse) {
        fileWriterThread.eventResponse = CreateEventW(NULL, FALSE, FALSE, NULL);
        if (!fileWriterThread.eventResponse) {
            showLastWinError(TEXT("Failed to create eventResponse in fileWriterThread: "), true);
            return false;
        }
    }
    if (!fileReaderThread.eventResponse) {
        fileReaderThread.eventResponse = CreateEventW(NULL, FALSE, FALSE, NULL);
        if (!fileReaderThread.eventResponse) {
            showLastWinError(TEXT("Failed to create eventResponse in fileReaderThread: "), true);
            return false;
        }
    }
    if (!decoderThread.eventResponse) {
        decoderThread.eventResponse = CreateEventW(NULL, FALSE, FALSE, NULL);
        if (!decoderThread.eventResponse) {
            showLastWinError(TEXT("Failed to create eventResponse in decoderThread: "), true);
            return false;
        }
    }
    if (!presentationThread.eventResponse) {
        presentationThread.eventResponse = CreateEventW(NULL, FALSE, FALSE, NULL);
        if (!presentationThread.eventResponse) {
            showLastWinError(TEXT("Failed to create eventResponse in presentationThread: "), true);
            return false;
        }
    }


    if (!decoderThreadStarted) {
        decoderThreadStarted = true;
        decoderThreadThread = std::thread([](DecoderThread* thisArg) {
            thisArg->threadLoop();
            }, &decoderThread);
    }
    if (!presentationThreadStarted) {
        presentationThreadStarted = true;
        presentationThreadThread = std::thread([](PresentationThread* thisArg) {
            thisArg->threadLoop();
            }, &presentationThread);
    }
    if (!fileReaderThreadStarted) {
        fileReaderThreadStarted = true;
        fileReaderThreadThread = std::thread([](FileReaderThread* thisArg) {
            thisArg->threadLoop();
            }, &fileReaderThread);
    }

    return true;
}

void showLastWinError(const wchar_t* msg, bool showInsteadOfLog) {
    WinError winErr;
    debugStrStrType errorMsg = msg;
    errorMsg += winErr.getMessage();
    if (showInsteadOfLog) {
        MessageBox(mainWindowHandle, errorMsg.c_str(), TEXT("Error"), MB_OK);
    } else {
        debugMsg("%ls\n", errorMsg.c_str());
    }
}

bool initializeTooltip() {
    // to get UNICODE tooltips working, had to read this: https://learn.microsoft.com/en-us/windows/win32/controls/cookbook-overview
    // Create a tooltip.
    globalTooltipHandle = CreateWindowEx(WS_EX_TOPMOST, TOOLTIPS_CLASS, NULL,
        WS_POPUP | TTS_NOPREFIX | TTS_ALWAYSTIP,
        CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
        mainWindowHandle, NULL, hInst, NULL);
    if (!globalTooltipHandle) {
        WinError winErr;
        debugMsg("CreateWindowEx for tooltip failed: %s\n", winErr.getMessageA().c_str());
        return false;
    }

    if (!SetWindowPos(globalTooltipHandle, HWND_TOPMOST, 0, 0, 0, 0,
        SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE)) {
        WinError winErr;
        debugMsg("SetWindowPos on tooltip failed: %s\n", winErr.getMessageA().c_str());
        return false;
    }

    return true;
}

bool stopPlayback() {
    {
        std::unique_lock<std::mutex> presentationGuard(presentationTimer.mutex);
        if (!presentationTimer.isFileOpen) return true;
        presentationTimer.isFileOpen = false;
        presentationTimer.isPlaying = false;
        fileReaderThread.needSwitchFiles = true;
        SetEvent(presentationTimer.eventForFileReader);
        SetEvent(presentationTimer.eventForPresentationThread);
    }
    {
        HANDLE objects[2] = { eventApplicationIsClosing, fileReaderThread.eventResponse };
        DWORD waitResult = WaitForMultipleObjects(2, objects, FALSE, INFINITE);
        if (waitResult == WAIT_OBJECT_0) return true;
        if (waitResult != WAIT_OBJECT_0 + 1) {
            debugMsg("Abnormal result in stopPlayback from WaitForMultipleObjects{ eventApplicationIsClosing, fileReaderThread.eventResponse }: %.8x\n", waitResult);
            return false;
        }
    }
    {
        std::unique_lock<std::mutex> fileMutex(replayFileData.mutex);
        replayFileData.closeFile();
    }
    {
        std::unique_lock<std::mutex> queueGuard(decoderInput.mutex);
        decoderThread.needSwitchFiles = true;
        SetEvent(decoderInput.event);
    }
    {
        HANDLE objects[2] = { eventApplicationIsClosing, decoderThread.eventResponse };
        DWORD waitResult = WaitForMultipleObjects(2, objects, FALSE, INFINITE);
        if (waitResult == WAIT_OBJECT_0) return true;
        if (waitResult != WAIT_OBJECT_0 + 1) {
            debugMsg("Abnormal result in stopPlayback from WaitForMultipleObjects{ eventApplicationIsClosing, decoderThread.eventResponse }: %.8x\n", waitResult);
            return false;
        }
    }
    {
        std::unique_lock<std::mutex> queueGuard(presentationInput.mutex);
        presentationThread.needSwitchFiles = true;
        SetEvent(presentationTimer.eventForPresentationThread);
        SetEvent(presentationInput.event);
    }
    {
        HANDLE objects[2] = { eventApplicationIsClosing, presentationThread.eventResponse };
        DWORD waitResult = WaitForMultipleObjects(2, objects, FALSE, INFINITE);
        if (waitResult == WAIT_OBJECT_0) return true;
        if (waitResult != WAIT_OBJECT_0 + 1) {
            debugMsg("Abnormal result in stopPlayback from WaitForMultipleObjects{ eventApplicationIsClosing, presentationThread.eventResponse }: %.8x\n", waitResult);
            return false;
        }
    }
    return true;
}

bool writeFileSupplementaries() {
    std::unique_lock<std::mutex> fileGuard(replayFileData.mutex);
    std::unique_lock<std::mutex> indexGuard(replayFileData.indexMutex);
    std::unique_lock<std::mutex> replayInputsGuard(replayFileData.replayInputsMutex);
    std::unique_lock<std::mutex> hitboxGuard(replayFileData.hitboxDataMutex);
    if (!replayFileData.file) return true;
    ReplayFileHeader header;
    memcpy(header.characterTypes, (const void*)fileMappingManager.view->characterTypes, sizeof(fileMappingManager.view->characterTypes));
    header.logicsCount = replayFileData.index.size();
    header.indexOffset = _ftelli64(replayFileData.file);
    header.hitboxesCount = (unsigned int)replayFileData.hitboxData.size();
    _fseeki64(replayFileData.file, header.indexOffset, SEEK_SET);
    for (const ReplayIndexEntry& entry : replayFileData.index) {
        fwrite(&entry.fileOffset, 8, 1, replayFileData.file);
    }
    header.hitboxDataOffset = _ftelli64(replayFileData.file);
    for (const std::pair<unsigned int, std::vector<HitboxData>>& kv: replayFileData.hitboxData) {
        fwrite(&kv.first, 4, 1, replayFileData.file);
        unsigned int vecSize = (unsigned int)kv.second.size();
        fwrite(&vecSize, 4, 1, replayFileData.file);
        for (const HitboxData& hitboxData : kv.second) {
            fwrite(&hitboxData.offX, 4, 1, replayFileData.file);
            fwrite(&hitboxData.offY, 4, 1, replayFileData.file);
            fwrite(&hitboxData.sizeX, 4, 1, replayFileData.file);
            fwrite(&hitboxData.sizeY, 4, 1, replayFileData.file);
        }
    }
    for (int i = 0; i < 2; ++i) {
        __int64 startingPos = _ftelli64(replayFileData.file);
        header.replayInputsOffset[i] = startingPos;
        if (!replayFileData.replayInputs[i].empty()) {
            fwrite(&replayFileData.replayInputs[i].front(), sizeof(StoredInput), replayFileData.replayInputs[i].size(), replayFileData.file);
        }
        header.replayInputsSize[i] = (unsigned int)(_ftelli64(replayFileData.file) - startingPos);
    }
    fflush(replayFileData.file);
    fseek(replayFileData.file, 0, SEEK_SET);
    fwrite(&header, sizeof(header), 1, replayFileData.file);
    return true;
}

bool readSupplementaries() {
    std::unique_lock<std::mutex> fileGuard(replayFileData.mutex);
    std::unique_lock<std::mutex> indexGuard(replayFileData.indexMutex);
    std::unique_lock<std::mutex> replayInputsGuard(replayFileData.replayInputsMutex);
    std::unique_lock<std::mutex> hitboxGuard(replayFileData.hitboxDataMutex);
    fread(&replayFileData.header, sizeof(ReplayFileHeader), 1, replayFileData.file);
    if (replayFileData.header.version > 0) {
        MessageBox(mainWindowHandle, TEXT("Version of the file is unsupported."), TEXT("Error"), MB_OK);
        return false;
    }
    _fseeki64(replayFileData.file, replayFileData.header.indexOffset, SEEK_SET);
    size_t indicesCount = replayFileData.header.logicsCount;
    replayFileData.index.resize(indicesCount);
    if (indicesCount) {
        fread(&replayFileData.index.front(), sizeof(ReplayIndexEntry), indicesCount, replayFileData.file);
    }
    unsigned int hitboxesCount = replayFileData.header.hitboxesCount;
    while (hitboxesCount) {
        --hitboxesCount;
        unsigned int pointerValue = 0;
        unsigned int boxesCount = 0;
        fread(&pointerValue, 4, 1, replayFileData.file);
        fread(&boxesCount, 4, 1, replayFileData.file);
        std::vector<HitboxData> newBoxData;
        newBoxData.resize(boxesCount);
        if (boxesCount) {
            fread(&newBoxData.front(), sizeof(HitboxData), boxesCount, replayFileData.file);
        }
        replayFileData.hitboxData[pointerValue] = std::move(newBoxData);
    }
    for (int i = 0; i < 2; ++i) {
        replayFileData.replayInputs[i].resize(replayFileData.header.replayInputsSize[i] / sizeof(StoredInput));
        fread(&replayFileData.replayInputs[i].front(), sizeof(StoredInput), replayFileData.replayInputs[i].size(), replayFileData.file);
    }
    fseek(replayFileData.file, sizeof(ReplayFileHeader), SEEK_SET);
    return true;
}

bool seekbarMouseHittest(float* ratio, POINT* cursor, bool forceTrue) {

    POINT cursorPos;
    if (!cursor) {
        GetCursorPos(&cursorPos);
        ScreenToClient(mainWindowHandle, &cursorPos);
        cursor = &cursorPos;
    }
    bool isHit = false;
    RECT seekbarRect;
    {
        std::unique_lock<std::mutex> layoutGuard(layoutMutex);
        seekbarRect = layout.seekbarClickArea;
    }
    if (!forceTrue) {
        if (cursor->x >= seekbarRect.left && cursor->x <= seekbarRect.right
                && cursor->y >= seekbarRect.top && cursor->y <= seekbarRect.bottom) {
            isHit = true;
        }
    }
    if (isHit || forceTrue) {
        if (ratio) *ratio = (float)(cursor->x - seekbarRect.left) / (float)(seekbarRect.right - seekbarRect.left);
        return true;
    }
    return false;
}

void onSeekbarMousePress(float ratio, bool pressedJustNow) {

    unsigned int targetMatchCounter = 0;
    {
        if (ratio >= 1.F) {
            if (replayFileData.header.logicsCount > 0xFFFFFFFF) {
                targetMatchCounter = 0;
            }
            else {
                targetMatchCounter = 0xFFFFFFFF - (unsigned int)replayFileData.header.logicsCount + 1;
            }
        }
        else if (ratio <= 0.F) {
            targetMatchCounter = 0xFFFFFFFF;
        }
        else {
            size_t value = (size_t)(ratio * replayFileData.header.logicsCount);
            if (value > 0xFFFFFFFF) {
                targetMatchCounter = 0;
            }
            else {
                targetMatchCounter = 0xFFFFFFFF - (unsigned int)value;
            }
        }
    }

    onSeekbarMousePress(targetMatchCounter, pressedJustNow);

}

void onSeekbarMousePress(unsigned int targetMatchCounter, bool pressedJustNow) {
    seekbarIsPressed = true;

    if (pressedJustNow) {
        std::unique_lock<std::mutex> presentationInputGuard(presentationInput.mutex);
        presentationInput.needReportScrubbing = true;
    }


    {
        std::unique_lock<std::mutex> presentationTimerGuard(presentationTimer.mutex);
        if (pressedJustNow) {
            wasPlayingBeforeSeekbarPressed = presentationTimer.isPlaying;
        }
        if (!presentationTimer.scrubbing || presentationTimer.matchCounter != targetMatchCounter || presentationTimer.isPlaying) {
            presentationTimer.scrubbing = true;
            presentationTimer.scrubbingMatchCounter = targetMatchCounter;
            presentationTimer.isPlaying = false;
            SetEvent(presentationTimer.eventForPresentationThread);
            SetEvent(presentationTimer.eventForFileReader);
        }
    }

    if (pressedJustNow) {
        HANDLE objects[2] = { eventApplicationIsClosing, presentationThread.eventResponse };
        DWORD waitResult = WaitForMultipleObjects(2, objects, FALSE, INFINITE);
        if (waitResult == WAIT_OBJECT_0) {
            return;
        }
        else if (waitResult != WAIT_OBJECT_0 + 1) {
            debugMsg("onSeekbarMousePress: abnormal wait result: %.8x\n", waitResult);
            return;
        }
    }
}

void onSeekbarMouseMove() {
    float ratio;
    seekbarMouseHittest(&ratio, nullptr, true);
    onSeekbarMousePress(ratio, false);
}

void onSeekbarMouseUp() {
    seekbarIsPressed = false;

    {
        std::unique_lock<std::mutex> presentationInputGuard(presentationInput.mutex);
        presentationInput.needReportNotScrubbing = true;
    }

    {
        std::unique_lock<std::mutex> presentationTimerGuard(presentationTimer.mutex);
        presentationTimer.scrubbing = false;
        presentationTimer.isPlaying = wasPlayingBeforeSeekbarPressed;
        SetEvent(presentationTimer.eventForFileReader);
        SetEvent(presentationTimer.eventForPresentationThread);
    }

    {
        HANDLE objects[2] = { eventApplicationIsClosing, presentationThread.eventResponse };
        DWORD waitResult = WaitForMultipleObjects(2, objects, FALSE, INFINITE);
        if (waitResult == WAIT_OBJECT_0) {
            return;
        }
        else if (waitResult != WAIT_OBJECT_0 + 1) {
            debugMsg("onSeekbarMousePress: abnormal wait result: %.8x\n", waitResult);
            return;
        }
    }
}

void onReachPlaybackEnd() {
    pngButtonManager.setResourceIndex(PLAY_BUTTON, 0);
}

void updateScrollbarInfo() {
    for (int i = 0; i < 2; ++i) {
        inputsDrawing.panels[i].updateScrollbarInfo();
    }

}

bool rectHitTest(const RECT* rect, int x, int y) {
    return x >= rect->left && x < rect->right && y >= rect->top && y < rect->bottom;
}

void onSelectAll() {
    if (isRecordingGlobal) {
        MessageBox(mainWindowHandle, TEXT("Cannot select all inputs while recording."), TEXT("Error"), MB_OK);
        return;
    }
    if (seekbarIsPressed || inputsDrawing.aPanelIsSelecting()) return;
    for (int i = 0; i < 2; ++i) {
        InputsPanel& panel = inputsDrawing.panels[i];
        if (panel.isClickedLast) {
            panel.onSelectAll();
            break;
        }
    }
}

void onCopy() {
    if (isRecordingGlobal) {
        MessageBox(mainWindowHandle, TEXT("Cannot copy inputs while recording."), TEXT("Error"), MB_OK);
        return;
    }
    for (int i = 0; i < 2; ++i) {
        InputsPanel& panel = inputsDrawing.panels[i];
        if (panel.isClickedLast) {
            panel.onCopy();
            break;
        }
    }
}

void openSettings() {
    DialogBox(hInst, MAKEINTRESOURCE(IDD_SETTINGS), mainWindowHandle, settingsHandler);
}

void readSettings() {
    FILE* file = nullptr;
    std::wstring settingsFullPath = settingsPath;
    settingsFullPath += L"\\settings.ini";
    errno_t err = _wfopen_s(&file, settingsFullPath.c_str(), L"r");
    if (err || !file) {
        return;
    }
    char buf[100];
    memset(buf, 0, sizeof(buf));
    while (true) {
        if (!fgets(buf, 100, file)) {
            break;
        }
        if (findChar(buf, '=') != -1) {
            std::string line = buf;
            std::vector<std::string> pieces = split(line, '=');
            if (pieces.size() == 2) {
                for (std::string& piece : pieces) {
                    trim(piece);
                }
                if (pieces[0] == "frameskip") {
                    parseInteger(pieces[1], frameskip);
                    if (frameskip < 0) frameskip = 0;
                } else if (pieces[0] == "displayBoxes") {
                    parseBoolean(pieces[1], displayBoxes);
                } else if (pieces[0] == "displayInputs") {
                    parseBoolean(pieces[1], displayInputs);
                }
            }
        }
        if (feof(file)) {
            break;
        }
    }
    fclose(file);
}

void writeSettings() {
    FILE* file = nullptr;
    std::wstring settingsFullPath = settingsPath;
    settingsFullPath += L"\\settings.ini";
    errno_t err = _wfopen_s(&file, settingsFullPath.c_str(), L"w");
    if (err || !file) {
        char errorString[1024] = { '\0' };
        strerror_s(errorString, err);
        debugMsg("Error saving settings to a file: %s\n", errorString);
        return;
    }
    char buf[100];
    memset(buf, 0, sizeof(buf));

    strcpy(buf, "frameskip = ");
    _itoa(frameskip, buf + strlen(buf), 10);
    buf[strlen(buf)] = '\n';
    fputs(buf, file);

    memset(buf, 0, sizeof(buf));
    if (displayBoxes) {
        fputs("displayBoxes = true\n", file);
    } else {
        fputs("displayBoxes = false\n", file);
    }

    if (displayInputs) {
        fputs("displayInputs = true\n", file);
    }
    else {
        fputs("displayInputs = false\n", file);
    }
    fflush(file);
    fclose(file);
}

int findChar(const char* buf, char c) {
    const char* ptr = buf;
    while (*ptr != '\0') {
        if (*ptr == c) return (int)(ptr - buf);
        ++ptr;
    }
    return -1;
}

void trim(std::string& str) {
    if (str.empty()) return;
    const char* strStart = &str.front();
    const char* c = strStart;
    while (*c <= 32 && *c != '\0') {
        ++c;
    }
    if (*c == '\0') {
        str.clear();
        return;
    }

    const char* cEnd = strStart + str.size() - 1;
    while (cEnd >= c && *cEnd <= 32) {
        --cEnd;
    }
    if (cEnd < c) {
        str.clear();
        return;
    }

    str = std::string(c, cEnd - c + 1);
}

std::vector<std::string> split(const std::string& str, char c) {
    std::vector<std::string> result;
    const char* strStart = &str.front();
    const char* strEnd = strStart + str.size();
    const char* prevPtr = strStart;
    const char* ptr = strStart;
    while (*ptr != '\0') {
        if (*ptr == c) {
            if (ptr > prevPtr) {
                result.emplace_back(prevPtr, ptr - prevPtr);
            }
            else if (ptr == prevPtr) {
                result.emplace_back();
            }
            prevPtr = ptr + 1;
        }
        ++ptr;
    }
    if (prevPtr < strEnd) {
        result.emplace_back(prevPtr, strEnd - prevPtr);
    }
    return result;
}

bool parseInteger(const std::string& keyValue, int& integer) {
    for (auto it = keyValue.begin(); it != keyValue.end(); ++it) {
        if (!(*it >= '0' && *it <= '9')) return false;  // apparently atoi doesn't do this check
    }
    int result = std::atoi(keyValue.c_str());
    if (result == 0 && keyValue != "0") return false;
    integer = result;
    return true;
}

bool parseBoolean(const std::string& keyValue, bool& aBooleanValue) {
    if (_stricmp(keyValue.c_str(), "true") == 0) {
        aBooleanValue = true;
        return true;
    }
    if (_stricmp(keyValue.c_str(), "false") == 0) {
        aBooleanValue = false;
        return true;
    }
    return false;
}

void showOrLogError(debugStrArgType msg, bool showInsteadOfLog) {
    if (showInsteadOfLog) {
        MessageBox(mainWindowHandle, msg, TEXT("Error"), MB_OK);
    }
    else {
        debugMsg(PRINTF_STRING_ARGA "\n", msg);
    }
}

void showOrLogErrorWide(const wchar_t* msg, bool showInsteadOfLog) {
    if (showInsteadOfLog) {
        MessageBoxW(mainWindowHandle, msg, L"Error", MB_OK);
    }
    else {
        debugMsg("%ls\n", msg);
    }
}

void onArrowLeftRight(bool isRight) {
    if (!presentationTimer.isFileOpen) return;
    unsigned int currentMatchCounter = 0;
    {
        std::unique_lock<std::mutex> presentationTimerGuard(presentationTimer.mutex);
        currentMatchCounter = presentationTimer.matchCounter;
    }
    size_t indexSize;
    if (isRight) {
        {
            std::unique_lock<std::mutex> replayIndexGuard(replayFileData.indexMutex);
            indexSize = replayFileData.index.size();
        }
        if (currentMatchCounter <= 0xFFFFFFFF - (indexSize - 1) || currentMatchCounter == 0) {
            return;
        }
        if (currentMatchCounter <= 30) {
            currentMatchCounter = 0;
        } else {
            currentMatchCounter -= 30;
        }
        if (currentMatchCounter < 0xFFFFFFFF - (indexSize - 1)) {
            currentMatchCounter = 0xFFFFFFFF - (indexSize - 1);
        }
    }
    else {
        if (currentMatchCounter == 0xFFFFFFFF) {
            return;
        }
        if (0xFFFFFFFF - currentMatchCounter <= 30) {
            currentMatchCounter = 0xFFFFFFFF;
        } else {
            currentMatchCounter += 30;
        }
    }
    onSeekbarMousePress(currentMatchCounter, true);
    onSeekbarMouseUp();
}

void makeAtLeastOnePanelMarkedAsLastClicked() {
    bool neitherClickedLast = true;
    for (int i = 0; i < 2; ++i) {
        if (inputsDrawing.panels[i].isClickedLast) {
            neitherClickedLast = false;
            break;
        }
    }
    if (neitherClickedLast) {
        inputsDrawing.panels[0].isClickedLast = true;
    }
}

bool rectsIntersect(const RECT* a, const RECT* b) {
    return a->left < b->right && a->right > b->left && a->top < b->bottom && a->bottom > b->top;
}

bool selectRplFile(bool forWriting, std::wstring& path) {
    std::wstring szFile;
    szFile.resize(MAX_PATH);

    OPENFILENAMEW selectedFiles{ 0 };
    selectedFiles.lStructSize = sizeof(OPENFILENAMEW);
    selectedFiles.hwndOwner = NULL;
    selectedFiles.lpstrFile = &szFile.front();
    selectedFiles.lpstrFile[0] = L'\0';
    selectedFiles.nMaxFile = (DWORD)szFile.size() + 1;
    selectedFiles.lpstrFilter = FILTER_STRING;
    selectedFiles.nFilterIndex = 1;
    selectedFiles.lpstrFileTitle = NULL;
    selectedFiles.nMaxFileTitle = 0;
    selectedFiles.lpstrInitialDir = NULL;
    selectedFiles.Flags = OFN_PATHMUSTEXIST | (forWriting ? OFN_OVERWRITEPROMPT : OFN_FILEMUSTEXIST);

    BOOL success;
    if (forWriting) {
        success = GetSaveFileNameW(&selectedFiles);
    } else {
        success = GetOpenFileNameW(&selectedFiles);
    }
    if (!success) {
        DWORD errCode = CommDlgExtendedError();
        if (!errCode) {
            debugMsg("The file selection dialog was closed by the user.\n");
        }
        else {
            debugMsg("Error selecting file. Error code: %.8x\n", errCode);
        }
        return false;
    }
    szFile.resize(lstrlenW(szFile.c_str()));

    if (!endsWithCaseInsensitive(szFile, L".rpl")) {
        path = szFile + L".rpl";
        return true;
    }
    path = szFile;
    return true;
}

bool endsWithCaseInsensitive(std::wstring str, const wchar_t* endingPart) {
    unsigned int length = 0;
    const wchar_t* ptr = endingPart;
    while (*ptr != L'\0') {
        if (length == 0xFFFFFFFF) return false;
        ++ptr;
        ++length;
    }
    if (str.size() < length) return false;
    if (length == 0) return true;
    --ptr;
    auto it = str.begin() + (str.size() - 1);
    while (length) {
        if (towupper(*it) != towupper(*ptr)) return false;
        --length;
        --ptr;
        if (it == str.begin()) break;
        --it;
    }
    return length == 0;
}
