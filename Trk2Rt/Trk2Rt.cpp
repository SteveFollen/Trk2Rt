// Track to Route Converter - Trk2Rt.cpp
// by Steve Follen, October 2023 - December 2025

#include "Trk2Rt.h"
#include <shellapi.h> // for command line


// Window horizontal (x) positions
constexpr int X_SEPLFT = 20;
constexpr int X_LEFT = 30;
constexpr int X_INDENT = 50;

// standard Window title bar height
constexpr int HT_WIN_TITLE = 32;
// separator bar hieght
constexpr int HT_SEP = 3;

// Maximum lengths, including the null terminator (number of wide charaters, not bytes)
constexpr int MAX_LOADSTRING = 100;
constexpr int MAX_EXP_INFO_LEN = 90;
constexpr int MAX_TRACK_INFO = 10;

constexpr int RND_FACTOR_2 = 100; // round percent to 2 decimal places

// Global Variables:
HINSTANCE hInst;                     // current instance
WCHAR szTitle[MAX_LOADSTRING];       // title bar text
WCHAR szWindowClass[MAX_LOADSTRING]; // main window class name

WCHAR ptNameBase[MAX_BASE_NAME_LEN]; // Base for shaping point naming; track point number will be appended to this base
WCHAR importPathname[MAX_PATH]; // Complete path and file name of the import file
WCHAR exportPathname[MAX_PATH]; // Complete path and file name of the export file
int nShPtIdx;   // Index for global % of intermediate track points to export as additional shaping points between each primary route point pair (nShPtVal / hwndNumShPt).
                // Note that this initially seeds and, on change, overwrites the route point specific nShPtIdx / hwndRtPtNSP.
Track theTrack; // The imported track, if any. Only one track per import is supported.
RoutePointList prmRtPtList(1); // Linked list of primary route points which are included in the route(s). Start assuming one route.
RoutePointList excPtList(0);   // Linked list of primary route points which are excluded from the route(s). There are never any routes in this list.
prmRtPtSrc_t prmRtPtSource;    // primary route point source, none, route, or waypoints
trackSrc_t trackSource;        // track source - none, route, or track

bool runBkGrnd; //true to run windowless in the background - supports commnad line execution
bool parseImFl; // true if there is an import file from the command line to be parsed
bool reLocPrmRtPt;           // relocate each primary route point to its closest track point
bool stripPrev;              // strip previous prefix and numbering, if any, from imported route points
ptNumStyle_t numStyIdx;      // point numbering style on export

bool expWpt; // export waypoints
bool expTrk; // export track
bool expRt;  // export route


int cxChar;   // average character width (pixels)
int cyChar;   // charater height (pixels)
int cxClient; // width  of main window's client area (pixels)
int cyClient; // height of main window's client area (pixels)
int cyMenu;    // height of menu bar (pixels)
int cxHScroll; // width  of horizontal scroll bar (pixels)
int cyHScroll; // height of horizontal scroll bar (pixels)
int cxVScroll; // width of vertical scroll bar (pixels)

// Fixed x positions of primary route point detail windows.
// These keep the detail windows aligned and allow main window width to be fixed
int xPosViaPtName; // x position of the primary route point name windows
int xPosViaPtOps;  // x position of the primary route point options checkboxes & dropdown
int xPosViaPtDist; // x position of the primary route point distance vlaue & units dropdown

// widths (x dimension sizes) shared by multiple windows
int xSzSep;  // width of all seperator lines

// widths (x dimension sizes) of primary route point detail windows
int xSzPtPrfxLbl; // static label
int xSzPtStNumLbl;// static label
int xSzViaPtLd;   // lead, the prefix and number - static window

// widths (x dimension sizes) of non-detail windows
int xSzMain;         // parent window
int xSzExpLbl;       // Export checkboxes label
int xSzExpWpLbl;     // Export Waypoints - label
int xSzExpTrkLbl;    // Export Track - label
int xSzExpRtLbl;     // Export Route - label
int xSzRouteNameLbl; // track / route name - label
int xSzNumShPtLbl;   // % of intermediate track points to export as additional shaping points between each primary route point pair - label
int xSzNumShPt;      // % of intermediate track points to export as additional shaping points between each primary route point pair - combo box
int xSzPtNameLbl;    // additional point name - label
int xSzPtNumSty;     // numbering style - combo bow
int xSzPtNumLbl;     // point numbering style - label
int xSzExportButton; // Export - button
int xSzExcListLbl;   // Excluded route points list label
int xSz2ndExcLbl;    // second exclude label; this one is for the excluded route points list

bool updatingViaPtDetail = false; // prevent reentry to t2rUpdateRtPtDetailContent() due to SetWindowTextW() => EN_CHANGE

// Units to display distance, and multiplier. Distance is stored in feet.
wchar_t dstUnText[2][7] =
{ TEXT("Feet"), TEXT("Meters") };
double unitMult[2] = { 1.0, 0.3048 };

// Additional shaping point % options for track points and linestring coordinates
wchar_t nShPtTextTrk[6][6] =
{
    TEXT(" 0.00"), TEXT(" 1.00"), TEXT(" 2.00"),
    TEXT(" 3.00"), TEXT(" 5.00"), TEXT("10.00")
};
int nShPtValTrk[6] = { 0, 1, 2, 3, 5, 10 };

// Additional shaping point % options for road changes
wchar_t nShPtTextRt[6][7] =
{
    TEXT("  0.00"), TEXT(" 10.00"), TEXT(" 20.00"),
    TEXT(" 30.00"), TEXT(" 50.00"), TEXT("100.00")
};
int nShPtValRt[6] = { 0, 10, 20, 30, 50, 100 };

int* nShPtVal; // points to whichever of the above is applicable
double shPtPC; // global additional shaping point % value

// global window handles (in top to bottom display order)
HWND hwndMain;         // Parent window
HWND hwndOpeningLbl;   // Initial display at startup
HWND hwndImportInfo;   // Import information - static
HWND hwndUpSep;        // Upper separator line
HWND hwndExpLbl;       // Export checkboxes label
HWND hwndExpWpLbl;     // Export Waypoints - label
HWND hwndExpWpCheck;   // Export Waypoints - checkbox
HWND hwndExpTrkLbl;    // Export Track - label
HWND hwndExpTrkCheck;  // Export Track - checkbox
HWND hwndExpRtLbl;     // Export Route - label
HWND hwndExpRtCheck;   // Export Route - checkbox
HWND hwndMidSep;       // Mid separator line
HWND hwndGblSet;       // Global Settings - label
HWND hwndRouteNameLbl; // Track / route name - label
HWND hwndRouteName;    // Track / route name - edit
HWND hwndVpLocCheckLbl;// Route point re-location - label
HWND hwndVpLocCheck;   // Route point re-location - checkbox
HWND hwndNumShPtLbl;   // Convert - label
HWND hwndNumShPtLbl2;  // % of intermediate ___ to additional shaping points between each primary route point pair - label
HWND hwndNumShPt;      // % of intermediate track points to export as additional shaping points between each primary route point pair - combo box
HWND hwndPtNameLbl;    // Additional point name - label
HWND hwndPtName;       // Additional point name - edit
HWND hwndRtPtNameCheckLbl;// Substitute route point naming - label
HWND hwndRtPtNameCheck;// Substitute route point naming  - checkbox
HWND hwndPtNumLbl;     // Point numbering style - label
HWND hwndPtNumSty;     // Point numbering style - combo box
HWND hwndRemoveButton; // Remove previous numbering button
HWND hwndLwSep;        // Lower separator line
HWND hwndDetailLbl;    // Route point detail label
HWND hwndShapeLbl;     // Shaping label for route point options checkbox
HWND hwndExcludeLbl;   // Exclude label for route point options checkbox
HWND hwndDistlLbl;     // Distance label route point distance from closest track point
HWND hwndSplitlLbl;    // Split label for route point options checkbox
HWND hwndDistUnits;    // Distance units - combobox
// primary route point details display here
HWND hwndSplitButton;  // Split route - button
HWND hwndExportButton; // Export - button
HWND hwndExportInfo;   // Export information - static
HWND hwndExpFileLbl;   // Exported File label "route exported to"
HWND hwndExpFile;      // Exported File Display
HWND hwndExcSep;       // Seperator above excluded route points
HWND hwndExcListLbl;   // Excluded route points list label
HWND hwnd2ndExcLbl;    // Second exclude label; this one is for the excluded route points list
// excluded route points display here

// Forward declarations
// General Windows related
ATOM                MyRegisterClass(HINSTANCE hInstance);
BOOL                InitInstance(HINSTANCE, int);
LRESULT CALLBACK    WndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK    About(HWND, UINT, WPARAM, LPARAM);
// Route endpoint related
void t2rCheckRemoveViaPtsAtTrackEnds(void);
// utility
HWND t2rCreateLabelWindow(HWND hWnd, int x, int y, int w, int h, LPCWSTR lpString, bool rightJust = false);
// primary route point detail windows creation
void t2rCreatePrmRtPtDetailWindows(RoutePoint* pt, HDC& hdc);
// Overall Display content based on export selection
void t2rUpdateDspOnRtExportSelect(void);
void t2rUpdateDspWpTrkRtSelect(void);
// route point detail change related
void t2rUpdateRtPtDetailContent();
void t2rRemovePreviousNumbering(void);
void t2rUpdateRtPtDetailName(bool useWptNames);
void t2rUpdateRtPtDetailDist();
void t2rUpdateRtPtShpStateAndSltOptions();
void t2rUpdateRtPtShapeOptions();
void t2rUpdateRtPtTrkPtPercent(bool selectChnage);
void t2rUpdateRtPtLists();
// Route split action
void t2rSplitRoute();
// Export information display
void t2rUpdateExportInfoDisplay();


// main
int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
                     _In_opt_ HINSTANCE hPrevInstance,
                     _In_ LPWSTR    lpCmdLine,
                     _In_ int       nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);

    // set initial values for global options
    expWpt = expTrk = expRt = true;
    reLocPrmRtPt = true;
    stripPrev = false;
    numStyIdx = NS_ALL;
    importPathname[0] = L'\0';
    exportPathname[0] = L'\0';
    parseImFl = false;
    runBkGrnd = false;

    int retVal = 0;
    wchar_t* pos;
    wchar_t* endPos;
    int argc;
    LPWSTR cmdLn = GetCommandLineW();
    LPWSTR* argv = CommandLineToArgvW(cmdLn, &argc);

    do {
        if (argc > 1) {
            // import file name is the last thing on the command line
            if (lstrlenW(argv[argc - 1]) < MAX_IN_FILE_NAME_LEN) {
                WCHAR argFileName[MAX_PATH];
                wcscpy_s(argFileName, MAX_PATH, argv[argc - 1]);
                parseImFl = true;
                GetFullPathNameW(argFileName, MAX_PATH, importPathname, NULL);
            }
            else {
                MessageBoxW(NULL, L"Import path file is too long.", L"Trk2Rt Error While Importing", MB_OK | MB_ICONERROR);
                retVal = -1;
            }
        }
        LocalFree(argv);

        if (argc > 2) {
            runBkGrnd = true; // run windowless in the background
            if (retVal == 0) {
                if (pos = wcsstr(cmdLn, L"noRelocate")) {
                    reLocPrmRtPt = false;
                }

                shPtPC = 200.0;  // t2rParseImportFile() sets this to default value if it is not set by the command line parameter here
                if (pos = wcsstr(cmdLn, L"exportPercent=")) {
                    pos += wcslen(L"exportPercent=");
                    double tempDbl = wcstod(pos, NULL);
                    if ((tempDbl >= 0.0) && (tempDbl <= 100.0)) {
                        shPtPC = tempDbl;
                    }
                }

                if (pos = wcsstr(cmdLn, L"stripPrev")) {
                    stripPrev = true;
                }

                if (pos = wcsstr(cmdLn, L"NS_ALL")) {
                    numStyIdx = NS_ALL;
                }
                else if (pos = wcsstr(cmdLn, L"NS_VIA")) {
                    numStyIdx = NS_VIA;
                }
                else if (pos = wcsstr(cmdLn, L"NS_NONE")) {
                    numStyIdx = NS_NONE;
                }

                if (pos = wcsstr(cmdLn, L"exportPathname=")) {
                    if (*pos == L'\"') {
                        pos += 1;
                        endPos = wcschr(pos, L'\"');
                    }
                    else {
                        endPos = wcschr(pos, L' ');
                    }
                    if (endPos) {
                        wcsncpy_s(exportPathname, MAX_PATH, pos, endPos - pos);
                    }
                }
                else if (pos = wcsstr(cmdLn, L"exportPath=")) {
                    pos += wcslen(L"exportPath=");
                    if (*pos == L'\"') {
                        pos += 1;
                        endPos = wcschr(pos, L'\"');
                    }
                    else {
                        endPos = wcschr(pos, L' ');
                    }
                    if (endPos) {
                        wchar_t expPath[MAX_PATH] = L"";   // export file complete path
                        wcsncpy_s(expPath, MAX_PATH, pos, endPos - pos);
                        fs::path path = expPath;
                        fs::path fsImportPathname = importPathname; // complete path and file name
                        path.replace_extension(); // strip extension if there is one
                        wcscpy_s(exportPathname, MAX_PATH, path.c_str());
                        if (exportPathname[lstrlenW(exportPathname) - 1] != L'\\') {
                            wcscat_s(exportPathname, MAX_PATH, L"\\");
                        }
                        wcscat_s(exportPathname, MAX_PATH, fsImportPathname.stem().c_str());
                        wcscat_s(exportPathname, MAX_PATH, L"_T2R.gpx");
                    }
                }
                else if (pos = wcsstr(cmdLn, L"exportSubdir=")) {
                    pos += wcslen(L"exportSubdir=");
                    if (*pos == L'\"') {
                        pos += 1;
                        endPos = wcschr(pos, L'\"');
                    }
                    else {
                        endPos = wcschr(pos, L' ');
                    }
                    if (endPos) {
                        wchar_t exportFileStem[MAX_PATH];  // export file name without extension
                        wchar_t expSubdir[MAX_PATH]; // export file sub-directory name
                        wcsncpy_s(expSubdir, MAX_PATH, pos, endPos - pos);
                        fs::path subPath = expSubdir;
                        fs::path fsImportPathname = importPathname; // complete path and file name
                        wcscpy_s(exportFileStem, MAX_PATH, fsImportPathname.stem().c_str());
                        subPath.replace_extension();// strip extension if there is one
                        fsImportPathname.remove_filename();

                        wcscpy_s(exportPathname, MAX_PATH, fsImportPathname.c_str());
                        wcscat_s(exportPathname, MAX_PATH, subPath.c_str());
                        if (exportPathname[lstrlenW(exportPathname) - 1] != L'\\') {
                            wcscat_s(exportPathname, MAX_PATH, L"\\");
                        }
                        wcscat_s(exportPathname, MAX_PATH, exportFileStem);
                        wcscat_s(exportPathname, MAX_PATH, L"_T2R.gpx");
                    }
                }
                else {
                    // default export file
                    wchar_t exportFileStem[MAX_PATH];  // export file name without extension
                    fs::path fsImportPathname = importPathname; // complete path and file name
                    wcscpy_s(exportFileStem, MAX_PATH, fsImportPathname.stem().c_str());
                    fsImportPathname.remove_filename();
                    wcscpy_s(exportPathname, MAX_PATH, fsImportPathname.c_str());
                    wcscat_s(exportPathname, MAX_PATH, L"T2R\\");
                    wcscat_s(exportPathname, MAX_PATH, exportFileStem);
                    wcscat_s(exportPathname, MAX_PATH, L"_T2R.gpx");
                }

                if ((retVal = t2rParseImportFile()) == 0) {
                    if (stripPrev && importedNum) {
                        t2rRemovePreviousNumbering();
                    }
                    t2rExport();
                }
            }
            return retVal;
        }
    } while (0);

    HRESULT hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    if (!SUCCEEDED(hr))
    {
        return FALSE;
    }

    // Initialize global strings
    LoadStringW(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
    LoadStringW(hInstance, IDC_TRK2RT, szWindowClass, MAX_LOADSTRING);
    MyRegisterClass(hInstance);

    // Perform application initialization:
    if (!InitInstance (hInstance, nCmdShow))
    {
        return FALSE;
    }

    if (parseImFl) {
        t2rParseImportFile();
        parseImFl = false;
    }

    HACCEL hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_TRK2RT));

    MSG msg;

    // Main message loop:
    while (GetMessage(&msg, nullptr, 0, 0))
    {
        if (!TranslateAccelerator(msg.hwnd, hAccelTable, &msg))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }

    CoUninitialize();

    return (int) msg.wParam;
}

// Register window class.
ATOM MyRegisterClass(HINSTANCE hInstance)
{
    WNDCLASSEXW wcex;

    wcex.cbSize = sizeof(WNDCLASSEX);

    wcex.style          = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc    = WndProc;
    wcex.cbClsExtra     = 0;
    wcex.cbWndExtra     = 0;
    wcex.hInstance      = hInstance;
    wcex.hIcon          = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_TRK2RT));
    wcex.hCursor        = LoadCursor(nullptr, IDC_ARROW);
    wcex.hbrBackground  = (HBRUSH)(COLOR_WINDOW);
    wcex.lpszMenuName   = MAKEINTRESOURCEW(IDC_TRK2RT);
    wcex.lpszClassName  = szWindowClass;
    wcex.hIconSm        = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));

    return RegisterClassExW(&wcex);
}

// Save instance handle and create main window
BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
{
    SCROLLINFO si;

    //theTrack = NULL;

    hInst = hInstance; // Store instance handle in our global variable

    cyMenu = GetSystemMetrics(SM_CYMENU);
    cxHScroll = GetSystemMetrics(SM_CXHSCROLL);
    cyHScroll = GetSystemMetrics(SM_CYHSCROLL);
    cxVScroll = GetSystemMetrics(SM_CXVSCROLL);

    hwndMain = CreateWindowW(szWindowClass, szTitle, WS_OVERLAPPEDWINDOW | WS_HSCROLL | WS_VSCROLL,
                             CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
                             nullptr, nullptr, hInstance, nullptr);
    if (!hwndMain)
    {
        return FALSE;
    }

    ShowWindow(hwndMain, nCmdShow);

    SetWindowPos(hwndMain, NULL, 0, 0, xSzMain, 500, SWP_NOMOVE | SWP_NOZORDER);

    // The main window is initailly sized such that it's client area is large enough
    // to incldue all of the initial child windows.

    // Regarding the scroll info struct si:
    // nMin and nMax specify the left/top and right/bottom of scrollable area.
    // nPage specifies the size of the client rectangle, but limited to the size of scrollable area; 
    //        the value must be between 0 and (nMax - nMin + 1).
    // nPos specifies the left/top edge of the scroll box;
    //       the value must be between nMin and (nMax - max(nPage – 1, 0)).
    // When the client area is larger than the scrollable area, nPage is limited, nPos is zero,
    // and the scoll bar is not visible.
    // When the client area is smaller than the scrollable area, the scroll bar is visible
    // and the size of the scroll box represents the size of client area within the scrollable area.
    // 
    // For this implementation, the entire area is scrollable.
    si.cbSize = sizeof(si);
    si.fMask = SIF_RANGE | SIF_PAGE | SIF_POS;

    si.nMin = 0;
    si.nMax = cxClient;
    si.nPage = cxClient + 1; // +1 so scrollbar does not show initially
    si.nPos = 0;
    SetScrollInfo(hwndMain, SB_HORZ, &si, false);

    si.nMin = 0;
    si.nMax = cyClient;
    si.nPage = cyClient + 1; // +1 so scrollbar does not show initially
    si.nPos = 0;
    SetScrollInfo(hwndMain, SB_VERT, &si, false);

    UpdateWindow(hwndMain);

    return TRUE;
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    SCROLLINFO si;
    si.cbSize = sizeof(si);

    switch (message)
    {
    case WM_COMMAND:
    {
        int wmId = LOWORD(wParam);
        switch (wmId)
        {
        case IDC_EXPORT_RT_CHK:
            expRt = (SendMessage(hwndExpRtCheck, BM_GETCHECK, (WPARAM)0, (LPARAM)0) == BST_CHECKED);
            t2rUpdateDspOnRtExportSelect();
            [[fallthrough]];
        case IDC_EXPORT_CHK:
            expWpt = (SendMessage(hwndExpWpCheck, BM_GETCHECK, (WPARAM)0, (LPARAM)0) == BST_CHECKED);
            expTrk = (SendMessage(hwndExpTrkCheck, BM_GETCHECK, (WPARAM)0, (LPARAM)0) == BST_CHECKED);
            t2rUpdateDspWpTrkRtSelect();
            t2rUpdateExportInfoDisplay();
            t2rClearExportedFileDisplay();
            break;
        case IDC_RT_NAME:
        case IDC_VIA_PT:
            // global route name and route point detail name edit controls
            if (HIWORD(wParam) == EN_CHANGE) {
                t2rClearExportedFileDisplay();
            }
            break;
        case IDC_PTNAME:
            // global point naming edit control
            if (HIWORD(wParam) == EN_CHANGE) {
                GetWindowTextW(hwndPtName, ptNameBase, MAX_BASE_NAME_LEN);
                t2rUpdateRtPtDetailName((SendMessage(hwndRtPtNameCheck, BM_GETCHECK, (WPARAM)0, (LPARAM)0) == BST_CHECKED));
                t2rClearExportedFileDisplay();
            }
            break;
        case IDC_VPNMCHK:
            // global route point naming checkbox
            if (HIWORD(wParam) == BN_CLICKED) {
                t2rUpdateRtPtDetailName((SendMessage(hwndRtPtNameCheck, BM_GETCHECK, (WPARAM)0, (LPARAM)0) == BST_CHECKED));
                t2rClearExportedFileDisplay();
            }
            break;
        case IDC_RLOCCHK:
            // global relocate primary point checkbox
            reLocPrmRtPt = (SendMessage(hwndVpLocCheck, BM_GETCHECK, (WPARAM)0, (LPARAM)0) == BST_CHECKED);
            if (HIWORD(wParam) == BN_CLICKED) {
                t2rClearExportedFileDisplay();
            }
            break;
        case IDC_PTNUMSTY:
            // global point numbering style combobox
            if (HIWORD(wParam) == CBN_SELCHANGE) {
                numStyIdx = (ptNumStyle_t)SendMessage(hwndPtNumSty, CB_GETCURSEL, (WPARAM)0, (LPARAM)0);
                if (!updatingViaPtDetail) {
                    t2rUpdateRtPtDetailContent();
                }
                t2rClearExportedFileDisplay();
            }
            break;
        case IDC_REMOVE_BUTTON:
            // remove previous numbering button
            t2rRemovePreviousNumbering();
            break;
        case IDC_DIST_UNITS:
            // distance units combobox
            if (HIWORD(wParam) == CBN_SELCHANGE) {
                t2rUpdateRtPtDetailDist();
            }
            break;
        case IDC_DT_PTPRFX:
        case IDC_DT_PTSTNUM:
            // route point detail numbering prefix and starting number edit controls
            if (HIWORD(wParam) == EN_CHANGE) {
                if (!updatingViaPtDetail) {
                    t2rUpdateRtPtDetailContent();
                }
                t2rClearExportedFileDisplay();
            }
            break;
        case IDC_VIA_PT_SP:
            // route point detail shaping point checkbox
            if (HIWORD(wParam) == BN_CLICKED) {
                t2rUpdateRtPtShpStateAndSltOptions();
                if (!updatingViaPtDetail) {
                    t2rUpdateRtPtDetailContent();
                }
                t2rUpdateExportInfoDisplay();
                t2rClearExportedFileDisplay();
            }
            break;
        case IDC_VIA_PT_EX:
            // route point detail exclude point checkbox
            if (HIWORD(wParam) == BN_CLICKED) {
                t2rUpdateRtPtLists();
                if (!updatingViaPtDetail) {
                    t2rUpdateRtPtDetailContent();
                }
                t2rUpdateExportInfoDisplay();
                t2rClearExportedFileDisplay();

                t2rPositionChildWindows();
                InvalidateRect(hwndMain, NULL, true);
                UpdateWindow(hwndMain);
            }
            break;
        case IDC_VIA_PT_SLT:
            if (HIWORD(wParam) == BN_CLICKED) {
                // route point detail route split selection checkbox
                t2rUpdateRtPtShapeOptions();
                t2rClearExportedFileDisplay();
            }
            break;
        case IDC_NUMSHPT:
        {
            // global % of trackpoints or road id changes to export as shaping points combobox
            bool change = false;
            bool selectionChange = false;

            if (HIWORD(wParam) == CBN_SELCHANGE) {
                change = selectionChange = true;
                nShPtIdx = (int)SendMessage(hwndNumShPt, CB_GETCURSEL, (WPARAM)0, (LPARAM)0);
                shPtPC = nShPtVal[nShPtIdx];
            }
            else if (HIWORD(wParam) == CBN_EDITCHANGE) {
                change = true;
                WCHAR tempStr[10];
                GetWindowTextW(hwndNumShPt, tempStr, 10);
                shPtPC = wcstod(tempStr, NULL);
                shPtPC = max(0.0, shPtPC);
                shPtPC = min(shPtPC, 100.0);
                shPtPC = round(shPtPC * RND_FACTOR_2) / RND_FACTOR_2;
            }
            else if (HIWORD(wParam) == CBN_KILLFOCUS) {
                wstringstream pcStrm;
                pcStrm << fixed << right << setw(6) << setprecision(2) << shPtPC;
                wstring pcStr(pcStrm.str());
                SendMessageW(hwndNumShPt, WM_SETTEXT, 0, (LPARAM)pcStr.c_str());
            }
            if (change) {
                t2rUpdateGlobalTrkPtPercent(selectionChange);
                if (!updatingViaPtDetail) {
                    t2rUpdateRtPtDetailContent();
                }
                t2rClearExportedFileDisplay();
                t2rUpdateExportInfoDisplay();
            }
            break;
        }
        case IDC_VIA_PT_NSP:
        {
            // individual % of trackpoints or road id changes to export as shaping points combobox
            bool change = false;

            if (HIWORD(wParam) == CBN_SELCHANGE) {
                change = true;
                t2rUpdateRtPtTrkPtPercent(true);
            }
            else if (HIWORD(wParam) == CBN_EDITCHANGE) {
                change = true;
                t2rUpdateRtPtTrkPtPercent(false);
            }
            else if (HIWORD(wParam) == CBN_KILLFOCUS) {
                RoutePoint* pt = prmRtPtList.getFirst();
                while (pt) {
                    wstringstream pcStrm;
                    pcStrm << fixed << right << setw(6) << setprecision(2) << pt->shPtPC;
                    wstring pcStr(pcStrm.str());
                    SendMessageW(pt->hwndRtPtNSP, WM_SETTEXT, 0, (LPARAM)pcStr.c_str());
                    pt = prmRtPtList.getNext(pt);
                }
            }
            if (change) {
                if (!updatingViaPtDetail) {
                    t2rUpdateRtPtDetailContent();
                }
                t2rUpdateExportInfoDisplay();
                t2rClearExportedFileDisplay();
            }
            break;
        }
        case IDC_SPLT_BTN:
            // split route button
            t2rSplitRoute();
            if (!updatingViaPtDetail) {
                t2rUpdateRtPtDetailContent();
            }
            t2rUpdateExportInfoDisplay();
            break;
        case IDC_EXPORT_BUTTON:
            // export button
            if (HIWORD(wParam) == BN_CLICKED) {
                t2rGetExportFileName(false); // default export file pathname
                t2rExport();
            }
            break;
        case IDM_HELP:
            // about menu item
            DialogBox(hInst, MAKEINTRESOURCE(IDD_HELPBOX), hWnd, About);
            break;
        case IDM_ABOUT:
            // about menu item
            DialogBox(hInst, MAKEINTRESOURCE(IDD_ABOUTBOX), hWnd, About);
            break;
        case ID_FILE_IMPORT:
            // import menu item
            if (t2rGetImportFileName() == 0) {
                t2rParseImportFile();
            }
            break;
        case ID_FILE_EXPORT:
            // export menu item
            t2rGetExportFileName(true); // get export file pathname using standard windows file save dialog
            t2rExport();
            break;
        case IDM_EXIT:
            // exit menu item
            DestroyWindow(hWnd);
            break;
        }

        break;
    }

    case WM_CREATE:
    {
        wchar_t styOptions[3][24] =
        {
            TEXT("All points continuous"), TEXT("Via points continuous"), TEXT("None")
        };

        SIZE szStr;
        TEXTMETRIC tm;
        HDC hdc = GetDC(hWnd);
        GetTextMetrics(hdc, &tm);
        cxChar = tm.tmAveCharWidth;
        cyChar = tm.tmHeight + tm.tmExternalLeading;

        xPosViaPtName = X_LEFT + (int)(MAX_NAME_LEAD * cxChar * 1.1);
        xPosViaPtOps = xPosViaPtName + (int)(MAX_NAME_LEN * cxChar * 1.1) + cxChar;
        xPosViaPtDist = xPosViaPtOps + (cxChar * 16);

        xSzMain = xPosViaPtDist + (cxChar * 12) + X_LEFT + cxVScroll; // (cxChar * 12) is hwndDistUnits size, equal space at right, and scroll bar
        xSzSep = xSzMain - (X_SEPLFT * 2) - cxVScroll;

        wchar_t openingStr[] = L"Use File->Import to select:\r\n"
            "- a gpx file containing waypoints, a track, and/or a route\r\nor\r\n"
            "- a kml file containing placemark points and/or a linestring  ";
        GetTextExtentPoint32W(hdc, openingStr, 62, &szStr);
        hwndOpeningLbl = CreateWindowExW(0, TEXT("STATIC"), NULL, WS_CHILD | WS_VISIBLE | SS_LEFT,
            0, 0, szStr.cx, szStr.cy * 4, hWnd, nullptr, hInst, NULL);
        SetWindowTextW(hwndOpeningLbl, openingStr);

        hwndImportInfo = CreateWindowExW(0, TEXT("STATIC"), NULL, WS_CHILD | WS_VISIBLE,
            0, 0, (int)(MAX_NAME_LEN * cxChar * 1.1), cyChar, hWnd, nullptr, hInst, NULL);

        hwndUpSep = CreateWindowExW(0, TEXT("STATIC"), NULL, WS_CHILD | WS_VISIBLE | SS_ETCHEDHORZ,
            0, 0, xSzSep, HT_SEP, hWnd, nullptr, hInst, NULL);

        GetTextExtentPoint32W(hdc, L"Export: ", 8, &szStr);
        xSzExpLbl = szStr.cx;
        hwndExpLbl = t2rCreateLabelWindow(hWnd, 0, 0, xSzExpLbl, cyChar, L"Export:");

        GetTextExtentPoint32W(hdc, L"Waypoints ", 10, &szStr);
        xSzExpWpLbl = szStr.cx;
        hwndExpWpLbl = t2rCreateLabelWindow(hWnd, 0, 0, xSzExpWpLbl, cyChar, L"Waypoints");
        hwndExpWpCheck = CreateWindowExW(0, TEXT("BUTTON"), NULL, WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
            0, 0, cyChar, cyChar, hWnd, (HMENU)IDC_EXPORT_CHK, hInst, NULL);
        Button_SetCheck(hwndExpWpCheck, BST_CHECKED);
        expWpt = true;

        GetTextExtentPoint32W(hdc, L"Track ", 6, &szStr);
        xSzExpTrkLbl = szStr.cx;
        hwndExpTrkLbl = t2rCreateLabelWindow(hWnd, 0, 0, xSzExpTrkLbl, cyChar, L"Track");
        hwndExpTrkCheck = CreateWindowExW(0, TEXT("BUTTON"), NULL, WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
            0, 0, cyChar, cyChar, hWnd, (HMENU)IDC_EXPORT_CHK, hInst, NULL);
        Button_SetCheck(hwndExpTrkCheck, BST_CHECKED);
        expTrk = true;

        GetTextExtentPoint32W(hdc, L"Route  ", 7, &szStr);
        xSzExpRtLbl = szStr.cx;
        hwndExpRtLbl = t2rCreateLabelWindow(hWnd, 0, 0, xSzExpRtLbl, cyChar, L"Route");
        hwndExpRtCheck = CreateWindowExW(0, TEXT("BUTTON"), NULL, WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
            0, 0, cyChar, cyChar, hWnd, (HMENU)IDC_EXPORT_RT_CHK, hInst, NULL);
        Button_SetCheck(hwndExpRtCheck, BST_CHECKED);
        expRt = true;

        hwndMidSep = CreateWindowExW(0, TEXT("STATIC"), NULL, WS_CHILD | WS_VISIBLE | SS_ETCHEDHORZ,
            0, 0, xSzSep, HT_SEP, hWnd, nullptr, hInst, NULL);

        GetTextExtentPoint32W(hdc, L"Global Settings: ", 17, &szStr);
        hwndGblSet = t2rCreateLabelWindow(hWnd, 0, 0, szStr.cx, cyChar, L"Global Settings:");

        GetTextExtentPoint32W(hdc, L"Track / Route Name:  ", 21, &szStr);
        xSzRouteNameLbl = szStr.cx;
        hwndRouteNameLbl = t2rCreateLabelWindow(hWnd, 0, 0, xSzRouteNameLbl, cyChar, L"Track / Route Name:");
        hwndRouteName = CreateWindowExW(0, TEXT("EDIT"), NULL, WS_CHILD | WS_VISIBLE | ES_LEFT | ES_AUTOHSCROLL | WS_DISABLED,
            0, 0, (int)(MAX_NAME_LEN * cxChar * 1.1), cyChar, hWnd, (HMENU)IDC_RT_NAME, hInst, NULL);
        SendMessage(hwndRouteName, EM_SETLIMITTEXT, MAX_NAME_LEN - 1, 0);

        hwndVpLocCheck = CreateWindowExW(0, TEXT("BUTTON"), NULL, WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
            0, 0, cyChar, cyChar, hWnd, (HMENU)IDC_RLOCCHK, hInst, NULL);
        reLocPrmRtPt = true;
        Button_SetCheck(hwndVpLocCheck, BST_CHECKED);
        GetTextExtentPoint32W(hdc, L" Re-locate each primary route point to its closest track point ", 63, &szStr);
        hwndVpLocCheckLbl = t2rCreateLabelWindow(hWnd, 0, 0, szStr.cx, cyChar, L" Re-locate each primary route point to its closest track point");

        GetTextExtentPoint32W(hdc, L"Convert  ", 9, &szStr);
        xSzNumShPtLbl = szStr.cx;
        hwndNumShPtLbl = t2rCreateLabelWindow(hWnd, 0, 0, xSzNumShPtLbl, cyChar, L"Convert ");

        GetTextExtentPoint32W(hdc, L"100.0 ", 6, &szStr); // longest entry in the box
        xSzNumShPt = szStr.cx + (cxHScroll * 2);
        hwndNumShPt = CreateWindowExW(0, TEXT("COMBOBOX"), NULL, WS_CHILD | WS_VISIBLE | CBS_DROPDOWN,
            0, 0, xSzNumShPt, cyChar * 10, hWnd, (HMENU)IDC_NUMSHPT, hInst, NULL);
        // options and initial value, which depend on track vs route import, are set in t2rParseImportFile()

        GetTextExtentPoint32W(hdc, L" % of linestring coordinates to additional route shaping points  ", 63, &szStr); // longest possiblity
        hwndNumShPtLbl2 = t2rCreateLabelWindow(hWnd, 0, 0, szStr.cx, cyChar, L"");

        GetTextExtentPoint32W(hdc, L"Additional / substitute route point name:  ", 43, &szStr);
        xSzPtNameLbl = szStr.cx;
        hwndPtNameLbl = t2rCreateLabelWindow(hWnd, 0, 0, xSzPtNameLbl, cyChar, L"Additional / substitute route point name:");
        hwndPtName = CreateWindowExW(0, TEXT("EDIT"), NULL, WS_CHILD | WS_VISIBLE | ES_LEFT | ES_AUTOHSCROLL | WS_DISABLED,
            0, 0, (int)(MAX_BASE_NAME_LEN * cxChar * 1.1), cyChar, hWnd, (HMENU)IDC_PTNAME, hInst, NULL);
        SendMessage(hwndPtName, EM_SETLIMITTEXT, MAX_BASE_NAME_LEN - 1, 0);

        hwndRtPtNameCheck = CreateWindowExW(0, TEXT("BUTTON"), NULL, WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
            0, 0, cyChar, cyChar, hWnd, (HMENU)IDC_VPNMCHK, hInst, NULL);
        Button_SetCheck(hwndRtPtNameCheck, BST_UNCHECKED);
        GetTextExtentPoint32W(hdc, L" Substitute original primary route point names  ", 48, &szStr);
        hwndRtPtNameCheckLbl = t2rCreateLabelWindow(hWnd, 0, 0, szStr.cx, cyChar, L" Substitute original primary route point names");

        GetTextExtentPoint32W(hdc, L"Route point numbering style:  ", 30, &szStr);
        xSzPtNumLbl = szStr.cx;
        hwndPtNumLbl = t2rCreateLabelWindow(hWnd, 0, 0, xSzPtNumLbl, cyChar, L"Route point numbering style:");

        GetTextExtentPoint32W(hdc, L"Via points continuous ", 23, &szStr); // longest entry in the box
        xSzPtNumSty = szStr.cx + (cxHScroll * 2);
        hwndPtNumSty = CreateWindowExW(0, TEXT("COMBOBOX"), NULL, WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST,
            0, 0, xSzPtNumSty, cyChar * 5, hWnd, (HMENU)IDC_PTNUMSTY, hInst, NULL);
        for (int k = NS_ALL; k <= NS_NONE; k += 1)
        {
            SendMessage(hwndPtNumSty, (UINT)CB_ADDSTRING, (WPARAM)0, (LPARAM)styOptions[k]);// Add option strings to combobox
        }
        numStyIdx = NS_ALL;
        SendMessage(hwndPtNumSty, CB_SETCURSEL, (WPARAM)numStyIdx, (LPARAM)0); // Set and display initial selection

        GetTextExtentPoint32W(hdc, L"Remove Previous Numbering ", 26, &szStr);
        hwndRemoveButton = CreateWindowExW(0, TEXT("BUTTON"), L"Remove Previous Numbering", WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON | BS_TEXT,
            0, 0, szStr.cx + (cyChar * 4), cyChar * 2, hWnd, (HMENU)IDC_REMOVE_BUTTON, hInst, NULL);

        hwndLwSep = CreateWindowExW(0, TEXT("STATIC"), NULL, WS_CHILD | WS_VISIBLE | SS_ETCHEDHORZ,
            0, 0, xSzSep, HT_SEP, hWnd, nullptr, hInst, NULL);

        showGlobalConfig(false);

        hwndDetailLbl = NULL;
        hwndShapeLbl = NULL;
        hwndExcludeLbl = NULL;
        hwndDistlLbl = NULL;
        hwndSplitlLbl = NULL;
        hwndDistUnits = NULL;
        hwndSplitButton = NULL;
        hwndExportButton = NULL;
        hwndExportInfo = NULL;
        hwndExpFileLbl = NULL;
        hwndExpFile = NULL;
        hwndExcSep = NULL;
        hwndExcListLbl = NULL;
        hwnd2ndExcLbl = NULL;

        ReleaseDC(hWnd, hdc);

        break;
    }

    case WM_SIZE:
    {
        cxClient = LOWORD(lParam);
        cyClient = HIWORD(lParam);

        si.fMask = SIF_PAGE;
        si.nPage = cxClient;
        SetScrollInfo(hWnd, SB_HORZ, &si, TRUE);

        si.fMask = SIF_PAGE;
        si.nPage = cyClient;
        SetScrollInfo(hWnd, SB_VERT, &si, TRUE);

        t2rPositionChildWindows();

        break;
    }

    case WM_HSCROLL:
    {
        int xPosStart;

        si.fMask = SIF_ALL;
        GetScrollInfo(hWnd, SB_HORZ, &si);
        xPosStart = si.nPos;

        switch (LOWORD(wParam))
        {
            // User clicked the scroll bar shaft left of the scroll box. 
        case  SB_PAGELEFT:
            si.nPos -= (int)si.nPage;
            break;

            // User clicked the scroll bar shaft right of the scroll box. 
        case SB_PAGERIGHT:
            si.nPos += (int)si.nPage;
            break;

            // User clicked the left arrow. 
        case SB_LINELEFT:
            si.nPos -= cxChar;
            break;

            // User clicked the right arrow. 
        case SB_LINERIGHT:
            si.nPos += cxChar;
            break;

            // User dragged the scroll box. 
        case SB_THUMBTRACK:
            si.nPos = HIWORD(wParam);
            break;

        default:
            break;
        }
        // The above can set si.nPos to values outside the valid scroll range which is si.nMin to (si.nMax - si.nPage).
        // (si.nPos specifiies the left edge of the scroll box, which is of size si.nPage)
        // That is OK with SetScrollInfo(). GetScrollInfo() then gets the valid value.
        si.fMask = SIF_POS;
        SetScrollInfo(hwndMain, SB_HORZ, &si, true);
        GetScrollInfo(hwndMain, SB_HORZ, &si);
        if (si.nPos != xPosStart) {
            ScrollWindow(hwndMain, (xPosStart - si.nPos), 0, NULL, NULL);
            UpdateWindow(hwndMain);
        }
        break;
    }

    case WM_VSCROLL:
    {
        int yPosStart;

        si.fMask = SIF_ALL;
        GetScrollInfo(hWnd, SB_VERT, &si);
        yPosStart = si.nPos;

        switch (LOWORD(wParam))
        {
        case SB_TOP:
            si.nPos = si.nMin; // always 0
            break;

        case SB_BOTTOM:
            si.nPos = si.nMax;
            break;

            // User clicked the scroll bar shaft above the scroll box. 
        case SB_PAGEUP:
            si.nPos -= (int)si.nPage;
            break;

            // User clicked the scroll bar shaft below the scroll box. 
        case SB_PAGEDOWN:
            si.nPos += (int)si.nPage;
            break;

            // User clicked the top arrow. 
        case SB_LINEUP:
            si.nPos -= cyChar;
            break;

            // User clicked the bottom arrow. 
        case SB_LINEDOWN:
            si.nPos += cyChar;
            break;

            // User dragged the scroll box. 
        case SB_THUMBTRACK:
            si.nPos = HIWORD(wParam);
            break;

        default:
            break;
        }
        // The above can set si.nPos to values outside the valid scroll range which is si.nMin to (si.nMax - si.nPage).
        // (si.nPos specifiies the top edge of the scroll box, which is of size si.nPage)
        // That is OK with SetScrollInfo(). GetScrollInfo() then gets the valid value.
        si.fMask = SIF_POS;
        SetScrollInfo(hwndMain, SB_VERT, &si, true);
        GetScrollInfo(hwndMain, SB_VERT, &si);
        if (si.nPos != yPosStart) {
            ScrollWindow(hwndMain, 0, (yPosStart - si.nPos), NULL, NULL);
            UpdateWindow(hwndMain);
        }
        break;
    }

    case WM_KEYDOWN:
    {
        WORD wScrollNotify = 0xFFFF;
        UINT scrOrnt = WM_VSCROLL;  // scroll orientation

        switch (wParam)
        {
        case VK_PRIOR:
            wScrollNotify = SB_PAGEUP;
            break;
        case VK_NEXT:
            wScrollNotify = SB_PAGEDOWN;
            break;
        case VK_END:
            wScrollNotify = SB_BOTTOM;
            break;
        case VK_HOME:
            wScrollNotify = SB_TOP;
            break;
        case VK_LEFT:
            wScrollNotify = SB_LINELEFT;
            scrOrnt = WM_HSCROLL;
            break;
        case VK_UP:
            wScrollNotify = SB_LINEUP;
            break;
        case VK_RIGHT:
            wScrollNotify = SB_LINERIGHT;
            scrOrnt = WM_HSCROLL;
            break;
        case VK_DOWN:
            wScrollNotify = SB_LINEDOWN;
            break;
        }

        if (wScrollNotify != -1)
            SendMessage(hWnd, scrOrnt, MAKELONG(wScrollNotify, 0), 0L);

        break;
    }

    case WM_MOUSEWHEEL:
    {
        int yNewPos;
        DWORD zDelta;

        si.fMask = SIF_RANGE | SIF_PAGE | SIF_POS;
        GetScrollInfo(hWnd, SB_VERT, &si);

        zDelta = GET_WHEEL_DELTA_WPARAM(wParam) / WHEEL_DELTA;

        yNewPos = si.nPos - (int)(cyChar * zDelta);
        yNewPos = max(0, yNewPos);
        yNewPos = min((si.nMax - (int)si.nPage), yNewPos);

        SendMessage(hWnd, WM_VSCROLL, MAKELONG(SB_THUMBTRACK, yNewPos), 0L);

        break;
    }


    case WM_DESTROY:
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

// Show or hide global configuration windows
void showGlobalConfig(bool show)
{
    int showHide;

    if (show) {
        showHide = SW_SHOWNORMAL;
    }
    else {
        showHide = SW_HIDE;
    }

    ShowWindow(hwndExpLbl, showHide);
    ShowWindow(hwndExpWpLbl, showHide);
    ShowWindow(hwndExpWpCheck, showHide);
    ShowWindow(hwndExpTrkLbl, showHide);
    ShowWindow(hwndExpTrkCheck, showHide);
    ShowWindow(hwndExpRtLbl, showHide);
    ShowWindow(hwndExpRtCheck, showHide);
    ShowWindow(hwndMidSep, showHide);
    ShowWindow(hwndGblSet, showHide);
    ShowWindow(hwndRouteNameLbl, showHide);
    ShowWindow(hwndRouteName, showHide);
    ShowWindow(hwndRtPtNameCheckLbl, showHide);
    ShowWindow(hwndVpLocCheck, showHide);
    ShowWindow(hwndVpLocCheckLbl, showHide);
    if (trackSource != TS_NO_TRK) {
        ShowWindow(hwndNumShPtLbl, showHide);
        ShowWindow(hwndNumShPt, showHide);
        ShowWindow(hwndNumShPtLbl2, showHide);
    }
    else {
        ShowWindow(hwndNumShPtLbl, SW_HIDE);
        ShowWindow(hwndNumShPt, SW_HIDE);
        ShowWindow(hwndNumShPtLbl2, SW_HIDE);
    }
    ShowWindow(hwndPtNameLbl, showHide);
    ShowWindow(hwndPtName, showHide);
    ShowWindow(hwndRtPtNameCheck, showHide);
    ShowWindow(hwndPtNumLbl, showHide);
    ShowWindow(hwndPtNumSty, showHide);
    if (importedNum) {
        ShowWindow(hwndRemoveButton, showHide);
    }
    else {
        ShowWindow(hwndRemoveButton, SW_HIDE);
    }
    ShowWindow(hwndLwSep, showHide);
}

// Position or reposition all child windows as needed.
// Window sizes were set in the two versions of t2rCreatePrmRtPtDetailWindows() or where other windows are created.
int t2rPositionChildWindows()
{
    int xOffset;
    int yPos;
    int xPos;
    int vSpace = cyChar * 2;

    RoutePoint* pt;
    HDWP mwps;
    SCROLLINFO si;

    si.cbSize = sizeof(si);
    si.fMask = SIF_POS;
    GetScrollInfo(hwndMain, SB_HORZ, &si);
    xOffset = si.nPos;
    GetScrollInfo(hwndMain, SB_VERT, &si);
    yPos = cyChar - si.nPos; // start one character height down from the top of the client area

    UINT flags = SWP_NOOWNERZORDER | SWP_NOREDRAW | SWP_NOSIZE | SWP_NOZORDER;

    if (hwndOpeningLbl) {
        SetWindowPos(hwndOpeningLbl, NULL, X_LEFT - xOffset, yPos, 0, 0, flags);
    }
    else {
        mwps = BeginDeferWindowPos(10);
        DeferWindowPos(mwps, hwndImportInfo, NULL, X_LEFT - xOffset, yPos, 0, 0, flags);
        yPos += vSpace;
        DeferWindowPos(mwps, hwndUpSep, NULL, X_SEPLFT - xOffset, yPos, 0, 0, flags);
        yPos += cyChar;
        xPos = X_LEFT - xOffset;
        DeferWindowPos(mwps, hwndExpLbl, NULL, xPos, yPos, 0, 0, flags);
        xPos += xSzExpLbl + cyChar * 3;
        DeferWindowPos(mwps, hwndExpWpLbl, NULL, xPos, yPos, 0, 0, flags);
        xPos += xSzExpWpLbl;
        DeferWindowPos(mwps, hwndExpWpCheck, NULL, xPos, yPos, 0, 0, flags);
        xPos += cyChar * 5;
        DeferWindowPos(mwps, hwndExpTrkLbl, NULL, xPos, yPos, 0, 0, flags);
        xPos += xSzExpTrkLbl;
        DeferWindowPos(mwps, hwndExpTrkCheck, NULL, xPos, yPos, 0, 0, flags);
        xPos += cyChar * 5;
        DeferWindowPos(mwps, hwndExpRtLbl, NULL, xPos, yPos, 0, 0, flags);
        xPos += xSzExpRtLbl;
        DeferWindowPos(mwps, hwndExpRtCheck, NULL, xPos, yPos, 0, 0, flags);
        yPos += vSpace;
        DeferWindowPos(mwps, hwndMidSep, NULL, X_SEPLFT, yPos, 0, 0, flags);
        yPos += HT_SEP + cyChar;
        EndDeferWindowPos(mwps);

        if (expTrk || expRt) {
            SetWindowPos(hwndGblSet, NULL, X_LEFT - xOffset, yPos, 0, 0, flags);
            yPos += vSpace;
            SetWindowPos(hwndRouteNameLbl, NULL, X_INDENT - xOffset, yPos, 0, 0, flags);
            SetWindowPos(hwndRouteName, NULL, X_INDENT + xSzRouteNameLbl - xOffset, yPos, 0, 0, flags);
            yPos += vSpace;
        }

        if (expRt) {
            SetWindowPos(hwndVpLocCheck, NULL, X_INDENT - xOffset, yPos, 0, 0, flags);
            SetWindowPos(hwndVpLocCheckLbl, NULL, X_INDENT + cyChar - xOffset, yPos, 0, 0, flags);
            yPos += vSpace;
            if (trackSource != TS_NO_TRK) {
                SetWindowPos(hwndNumShPtLbl, NULL, X_INDENT - xOffset, yPos, 0, 0, flags);
                SetWindowPos(hwndNumShPt, NULL, X_INDENT + xSzNumShPtLbl - xOffset, yPos - (int)(cyChar * 0.2), 0, 0, flags);
                SetWindowPos(hwndNumShPtLbl2, NULL, X_INDENT + xSzNumShPtLbl + xSzNumShPt - xOffset, yPos, 0, 0, flags);
                yPos += vSpace;
            }
            SetWindowPos(hwndPtNameLbl, NULL, X_INDENT - xOffset, yPos, 0, 0, flags);
            SetWindowPos(hwndPtName, NULL, X_INDENT + xSzPtNameLbl - xOffset, yPos, 0, 0, flags);
            yPos += vSpace;
            SetWindowPos(hwndRtPtNameCheck, NULL, X_INDENT - xOffset, yPos, 0, 0, flags);
            SetWindowPos(hwndRtPtNameCheckLbl, NULL, X_INDENT + cyChar - xOffset, yPos, 0, 0, flags);
            yPos += vSpace;
            SetWindowPos(hwndPtNumLbl, NULL, X_INDENT - xOffset, yPos, 0, 0, flags);
            SetWindowPos(hwndPtNumSty, NULL, X_INDENT + xSzPtNumLbl - xOffset, yPos, 0, 0, flags);
            SetWindowPos(hwndRemoveButton, NULL, X_INDENT + xSzPtNumLbl + xSzPtNumSty + (cxChar * 8) - xOffset, yPos, 0, 0, flags);

            yPos += vSpace + cyChar;
            SetWindowPos(hwndLwSep, NULL, X_SEPLFT - xOffset, yPos, 0, 0, flags);
            yPos += HT_SEP + cyChar;

            // The primary route points
            pt = prmRtPtList.getFirst();
            if (pt && hwndDetailLbl) {
                SetWindowPos(hwndDetailLbl, NULL, X_LEFT - xOffset, yPos, 0, 0, flags);
                SetWindowPos(hwndShapeLbl, NULL, xPosViaPtOps - xOffset, yPos, 0, 0, flags);
                yPos += cyChar;
                SetWindowPos(hwndExcludeLbl, NULL, xPosViaPtOps + (cxChar * 4) - xOffset, yPos, 0, 0, flags);
                if (hwndDistlLbl) {
                    SetWindowPos(hwndDistlLbl, NULL, xPosViaPtDist - xOffset, yPos, 0, 0, flags);
                }
                yPos += cyChar;
                SetWindowPos(hwndSplitlLbl, NULL, xPosViaPtOps + (cxChar * 8) - xOffset, yPos, 0, 0, flags);
                if (hwndDistUnits) {
                    SetWindowPos(hwndDistUnits, NULL, xPosViaPtDist - xOffset, yPos, 0, 0, flags);
                }
            }
            while (pt) {
                int xPosStNumLbl = X_INDENT + xSzPtPrfxLbl + (int)(MAX_PREFIX_LEN * cxChar * 1.1) + cyChar - xOffset;
                if (pt->hwndPtPrfxLbl) {
                    SetWindowPos(pt->hwndPtPrfxLbl, NULL, X_INDENT - xOffset, yPos, 0, 0, flags);
                    SetWindowPos(pt->hwndPtPrfx, NULL, X_INDENT + xSzPtPrfxLbl - xOffset, yPos, 0, 0, flags);

                    SetWindowPos(pt->hwndPtStNumLbl, NULL, xPosStNumLbl, yPos, 0, 0, flags);
                    SetWindowPos(pt->hwndPtStNum, NULL, xPosStNumLbl + xSzPtStNumLbl, yPos, 0, 0, flags);
                    yPos += vSpace;
                }
                SetWindowPos(pt->hwndRtPtLd, NULL, X_LEFT - xOffset, yPos, 0, 0, flags);
                SetWindowPos(pt->hwndRtPtName, NULL, xPosViaPtName - xOffset, yPos, 0, 0, flags);
                xPos = xPosViaPtOps - xOffset;
                if (pt->hwndRtPtSP) {
                    SetWindowPos(pt->hwndRtPtSP, NULL, xPos, yPos, 0, 0, flags);
                }
                xPos += cyChar * 2;
                if (pt->hwndRtPtEx) {
                    SetWindowPos(pt->hwndRtPtEx, NULL, xPos, yPos, 0, 0, flags);
                }
                xPos += cyChar * 2;
                if (pt->hwndRtPtSlt) {
                    SetWindowPos(pt->hwndRtPtSlt, NULL, xPos, yPos, 0, 0, flags);
                }
                xPos += cyChar * 2;
                if (pt->hwndRtPtDist) {
                    SetWindowPos(pt->hwndRtPtDist, NULL, xPos, yPos, 0, 0, flags);
                }
                if (pt->hwndRtPtAdd) {
                    yPos += (int)(cyChar * 1.2);
                    SetWindowPos(pt->hwndRtPtAdd, NULL, xPosViaPtName - xOffset, yPos, 0, 0, flags);
                }
                if (pt->hwndRtPtNSP) {
                    SetWindowPos(pt->hwndRtPtNSP, NULL, xPosViaPtOps - xOffset, yPos, 0, 0, flags);
                }

                pt = prmRtPtList.getNext(pt);
                yPos += vSpace;
            }

            if (hwndSplitButton) {
                SetWindowPos(hwndSplitButton, NULL, xPosViaPtOps - xOffset, yPos, 0, 0, flags);
            }
        }

        if (hwndExportButton) {
            yPos += cyChar;
            SetWindowPos(hwndExportButton, NULL, X_LEFT - xOffset, yPos, 0, 0, flags);
            SetWindowPos(hwndExportInfo, NULL, X_LEFT + cyChar * 7 - xOffset, yPos, 0, 0, flags);
            yPos += cyChar * (prmRtPtList.getNumRoutes() + 3);
        }

        if (hwndExpFile && IsWindowVisible(hwndExpFile)) {
            SetWindowPos(hwndExpFileLbl, NULL, X_LEFT - xOffset, yPos, 0, 0, flags);
            yPos += (int)(cyChar * 1.5);
            SetWindowPos(hwndExpFile, NULL, X_LEFT - xOffset, yPos, 0, 0, flags);
            yPos += cyChar * (prmRtPtList.getNumRoutes() + 1);
        }
        pt = excPtList.getFirst();
        if (expRt && pt) {
            SetWindowPos(hwndExcSep, NULL, X_SEPLFT - xOffset, yPos, 0, 0, flags);
            yPos += cyChar;
            SetWindowPos(hwndExcListLbl, NULL, X_LEFT - xOffset, yPos, 0, 0, flags);
            SetWindowPos(hwnd2ndExcLbl, NULL, xPosViaPtOps + (cxChar * 4) - xOffset, yPos, 0, 0, flags);
            yPos += vSpace;

            while (pt) {
                SetWindowPos(pt->hwndRtPtName, NULL, X_LEFT + xSzViaPtLd - xOffset, yPos, 0, 0, flags);
                xPos = xPosViaPtOps + (cyChar * 2) - xOffset;
                // if pt is on the ecxPtList, it must have an hwndRtPtEx
                SetWindowPos(pt->hwndRtPtEx, NULL, xPos, yPos, 0, 0, flags);
                pt = excPtList.getNext(pt);
                yPos += (int)(cyChar * 1.5);
            }
        }
    }
    yPos += cyChar;

    return yPos + si.nPos; // total height (y dimension) for all current child windows
}

// Utility to create a window for a static label and set the label.
HWND t2rCreateLabelWindow(HWND hWnd, int x, int y, int w, int h, LPCWSTR lpString, bool rightJust)
{
    DWORD dwStyle = SS_SIMPLE;
    if (rightJust) {
        dwStyle = SS_RIGHT;
    }

    HWND hwndLabel = CreateWindowExW(0, TEXT("STATIC"), NULL, WS_CHILD | WS_VISIBLE | dwStyle,
        x, y, w, h, hWnd, nullptr, hInst, NULL);
    SetWindowTextW(hwndLabel, lpString);
    return hwndLabel;
}

// Routes must always begin and end with via points.
// If there is a track, check that there are via points
// for the first and last points of the track. Create via(s) if not.
void t2rCheckAddViaPtsAtTrackEnds(void)
{
    RoutePoint* fstVia;
    RoutePoint* lstVia = prmRtPtList.getLast();
    RoutePoint* newPt;
    RoutePoint* nextPt;
    wchar_t ptName[8];
    int yPos = 0;
    HDC hdc = NULL;
    int lastTrkPtIdx;
    TrackPoint* trkPt;

    if (theTrack.nPts() > 1) {
        lastTrkPtIdx = theTrack.getLast()->getIdx();
        if (hwndDetailLbl) {
            // if detail windows have already been created for other primary route points
            hdc = GetDC(hwndMain);
        }

        // If there is no last primary point (meaning there are no primary route points) or
        // the last primary route point's closest track point is not the last track point,
        // then add an end primary point located at the end of the track.
        if (!lstVia || (lstVia->clsTrkPtIdx != lastTrkPtIdx)) {
            trkPt = theTrack.getLast();
            _snwprintf_s(ptName, 8, L"%s", L"END");
            newPt = prmRtPtList.appendLast(trkPt->lat, trkPt->lon, ptName);
            newPt->genEndPt = true;
            newPt->clsTrkPtDist = 0;
            newPt->clsTrkPtIdx = lastTrkPtIdx;
            newPt->trkSeg = theTrack.getLast();
            newPt->clsPt.lat = newPt->lat;
            newPt->clsPt.lon = newPt->lon;
            newPt->nShPtIdx = nShPtIdx;
            newPt->shPtPC = shPtPC;
            newPt->nTrkPts = 0;
            newPt->nRdIdChg = 0;
            newPt->nShpPts = 0;
            if (hwndDetailLbl) {
                t2rCreatePrmRtPtDetailWindows(newPt, hdc);
            }
        }

        fstVia = prmRtPtList.getFirst(); // There will always be one - it may have just been created immediately above
        // if the first primary route point's closet track point is not the first track point,
        // (that is, if the first primary route point is not at the start of the track,)
        // then add a primary route point located at the start of the track.
        if (fstVia->clsTrkPtIdx != 0) {
            trkPt = theTrack.getFirst();
            _snwprintf_s(ptName, 8, L"%s", L"BEGIN");
            newPt = prmRtPtList.createFirst(trkPt->lat, trkPt->lon, ptName);
            newPt->genEndPt = true;
            newPt->clsTrkPtDist = 0;
            newPt->clsTrkPtIdx = 0;
            newPt->trkSeg = theTrack.getFirst();
            newPt->clsPt.lat = newPt->lat;
            newPt->clsPt.lon = newPt->lon;
            newPt->nShPtIdx = nShPtIdx;
            newPt->shPtPC = shPtPC;
            nextPt = prmRtPtList.getNext(newPt);
            if (nextPt) {
                newPt->nTrkPts = nextPt->clsTrkPtIdx - 1;
                if (trackSource == TS_ROUTE) {
                    TrackPoint* trkPt = theTrack.getFirst();
                    int rdIdChgCount = 0;
                    if (trkPt && (nextPt->clsTrkPtIdx > newPt->clsTrkPtIdx)) {
                        while (trkPt->getIdx() < nextPt->clsTrkPtIdx) {
                            if (trkPt->isRdIdChg()) {
                                rdIdChgCount++;
                            }
                            trkPt = theTrack.getNext(trkPt);
                        }
                        newPt->nRdIdChg = rdIdChgCount;
                    }
                    else {
                        newPt->nRdIdChg = newPt->nTrkPts = 0;
                        }
                }
                else {
                    newPt->nRdIdChg = 0;
                }
            }
            if (trackSource == TS_ROUTE) {
                newPt->nShpPts = (int)(newPt->nRdIdChg * newPt->shPtPC / 100);
            }
            else {
                newPt->nShpPts = (int)(newPt->nTrkPts * newPt->shPtPC / 100);
            }
            if (hwndDetailLbl) {
                t2rCreatePrmRtPtDetailWindows(newPt, hdc);
            }
        }
        if (hwndDetailLbl) {
            ReleaseDC(hwndMain, hdc);
        }
    }
}

// Check if any end primary route points previously added by t2rCheckAddViaPtsAtTrackEnds()
// are duplicate ends to original primary route points. This could occur due to previously excluded
// primary route points being re-incldued in the route. Remove generated duplicates if so.
// Routes must always begin and end with via points, never shaping points.
void t2rCheckRemoveViaPtsAtTrackEnds(void)
{
    RoutePoint* pt = prmRtPtList.getFirst();
    RoutePoint* next = NULL;

    if (pt) {
        next = prmRtPtList.getNext(pt);
    }

    // If an original end point has been reincluded, it will be after the generated end point - see orderedInsert()
    if (pt && next && pt->genEndPt && (next->clsTrkPtIdx == 0)) {
        // The first point (pt) is a generated duplicated of the 2nd (next), so remove it.
        prmRtPtList.remove(pt);
        // nTrkPts and nRdIdChg could not have changed while the point was on the excPtList, but shPtPC could have, so recalculate nShpPts
        if (trackSource == TS_ROUTE) {
            next->nShpPts = (int)(next->nRdIdChg * next->shPtPC / 100);
        }
        else {
            next->nShpPts = (int)(next->nTrkPts * next->shPtPC / 100);
        }
        delete pt;
    }
    
    pt = next;
    while (pt) {
        next = prmRtPtList.getNext(pt);
        if (pt->genEndPt && next && (pt->clsTrkPtIdx == next->clsTrkPtIdx)) {
            // the 2nd last point (pt) is a generated duplicate of the last point (next), so remove it
            prmRtPtList.remove(pt);
            // For the last point, nTrkPts, nRdIdChg, and nShpPts will always be 0.
            delete pt;
        }
        pt = next;
    }
}

// Create primary route point detail windows
// Window sizes here set here. Window positions are set in t2rPositionChildWindows()
void t2rCreatePrmRtPtDetailWindows(RoutePoint* pt, HDC& hdc)
{
    static ptNumStyle_t numStyIdx; // point numbering style
    static bool subNames; // true to substitute original point names
    static long expPtNum; // point number
    SIZE szStr; // size of various strings; used for window sizing
    wchar_t ptNameLead[MAX_NAME_LEAD]; // leading part of primary point name (prefix and number)
    wchar_t longAddShpTrk[200];       // addtional shaping point line text - may be too long for display
    wchar_t addShpTrk[MAX_NAME_LEN];  // addtional shaping point line text - safe length
    wchar_t ptName[MAX_NAME_LEN]; // point name
    wchar_t clsTrkPtDist[MAX_TRACK_INFO]; // primary route pt's distance to its closest track point

    bool rtFst = (pt->posInRt == RTPOS_FIRST); // true for first point of a route
    bool rtLast = (pt->posInRt == RTPOS_LAST); // true for lart point of a route

    subNames = (SendMessage(hwndRtPtNameCheck, BM_GETCHECK, (WPARAM)0, (LPARAM)0) == BST_CHECKED);

    if (pt == prmRtPtList.getFirst()) {
        expPtNum = 0;
    }

    if (rtFst) {
        GetTextExtentPoint32W(hdc, L"Prefix:  ", 9, &szStr);
        xSzPtPrfxLbl = szStr.cx;
        pt->hwndPtPrfxLbl = t2rCreateLabelWindow(hwndMain, 0, 0, xSzPtPrfxLbl, cyChar, L"Prefix:");
        pt->hwndPtPrfx = CreateWindowExW(0, TEXT("EDIT"), NULL, WS_CHILD | WS_VISIBLE | ES_LEFT,
            0, 0, (int)(MAX_PREFIX_LEN * cxChar * 1.1), cyChar, hwndMain, (HMENU)IDC_DT_PTPRFX, hInst, NULL);
        SendMessage(pt->hwndPtPrfx, EM_SETLIMITTEXT, MAX_PREFIX_LEN - 1, 0);
        SetWindowTextW(pt->hwndPtPrfx, L"");

        GetTextExtentPoint32W(hdc, L"Starting at:  ", 14, &szStr);
        xSzPtStNumLbl = szStr.cx;
        pt->hwndPtStNumLbl = t2rCreateLabelWindow(hwndMain, 0, 0, xSzPtStNumLbl, cyChar, L"Starting at:");
        pt->hwndPtStNum = CreateWindowExW(0, TEXT("EDIT"), NULL, WS_CHILD | WS_VISIBLE | ES_RIGHT | ES_NUMBER,
            0, 0, (int)(MAX_START_NUM_LEN * cxChar * 1.1), cyChar, hwndMain, (HMENU)IDC_DT_PTSTNUM, hInst, NULL);
        SendMessage(pt->hwndPtStNum, EM_SETLIMITTEXT, MAX_START_NUM_LEN - 1, 0);
        SetWindowTextW(pt->hwndPtStNum, L"");
    }

    _snwprintf_s(ptNameLead, MAX_NAME_LEAD, _TRUNCATE, L"%02d  ", expPtNum);
    xSzViaPtLd = (int)(MAX_NAME_LEAD * cxChar * 1.1);
    pt->hwndRtPtLd = t2rCreateLabelWindow(hwndMain, 0, 0, xSzViaPtLd, cyChar, ptNameLead, true);

    pt->hwndRtPtName = CreateWindowExW(0, TEXT("EDIT"), NULL, WS_CHILD | WS_VISIBLE | ES_LEFT | ES_AUTOHSCROLL,
        0, 0, (int)(MAX_NAME_LEN * cxChar * 1.1), cyChar, hwndMain, (HMENU)IDC_VIA_PT, hInst, NULL);
    SendMessage(pt->hwndRtPtName, EM_SETLIMITTEXT, MAX_NAME_LEN - 1, 0);
    if (subNames) {
        _snwprintf_s(ptName, MAX_NAME_LEN, _TRUNCATE, L"%s %d", ptNameBase, pt->clsTrkPtIdx);
        SetWindowTextW(pt->hwndRtPtName, ptName);
    }
    else {
        SetWindowTextW(pt->hwndRtPtName, pt->ptName);
    }

    if (!rtFst && !rtLast) {
        pt->hwndRtPtSP = CreateWindowExW(0, TEXT("BUTTON"), NULL, WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
            0, 0, cyChar, cyChar, hwndMain, (HMENU)IDC_VIA_PT_SP, hInst, NULL);
        if (pt->shPt) {
            Button_SetCheck(pt->hwndRtPtSP, BST_CHECKED);
        }
        else {
            Button_SetCheck(pt->hwndRtPtSP, BST_UNCHECKED);
        }
    }

    if (!(pt->genEndPt)) {
        pt->hwndRtPtEx = CreateWindowExW(0, TEXT("BUTTON"), NULL, WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
            0, 0, cyChar, cyChar, hwndMain, (HMENU)IDC_VIA_PT_EX, hInst, NULL);
        Button_SetCheck(pt->hwndRtPtEx, BST_UNCHECKED);
    }

    if (!rtFst && !rtLast) {
        pt->hwndRtPtSlt = CreateWindowExW(0, TEXT("BUTTON"), NULL, WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
            0, 0, cyChar, cyChar, hwndMain, (HMENU)IDC_VIA_PT_SLT, hInst, NULL);
        Button_SetCheck(pt->hwndRtPtSlt, BST_UNCHECKED);
    }

    if (trackSource != TS_NO_TRK) {
        _snwprintf_s(clsTrkPtDist, MAX_TRACK_INFO, _TRUNCATE, L"%8.0lf", pt->clsTrkPtDist);  // always in feet here; no multiplier needed
        pt->hwndRtPtDist = t2rCreateLabelWindow(hwndMain, 0, 0, cxChar * 10, cyChar, clsTrkPtDist, true);
    }

    if (!rtLast) {
        if (trackSource == TS_ROUTE) {
             _snwprintf_s(longAddShpTrk, MAX_NAME_LEN, _TRUNCATE, L"%d Additional shaping points from %d intermediate road changes", pt->nShpPts, pt->nRdIdChg);
       }
        else if (trackSource == TS_TRACK) {
            _snwprintf_s(longAddShpTrk, MAX_NAME_LEN, _TRUNCATE, L"%d Additional shaping points from %d intermediate track points", pt->nShpPts, pt->nTrkPts);
        }
        else if (trackSource == TS_LINESTRING) {
            _snwprintf_s(longAddShpTrk, MAX_NAME_LEN, _TRUNCATE, L"%d Additional shaping points from %d intermediate linestring coordinates", pt->nShpPts, pt->nTrkPts);
        }
        else {
            _snwprintf_s(longAddShpTrk, MAX_NAME_LEN, _TRUNCATE, L"No additional shaping points available");
        }

        if (trackSource != TS_NO_TRK) {
            wcsncpy_s(addShpTrk, MAX_NAME_LEN, longAddShpTrk, _TRUNCATE);
            pt->hwndRtPtAdd = t2rCreateLabelWindow(hwndMain, 0, 0, (int)(MAX_NAME_LEN * cxChar * 1.1), cyChar, addShpTrk, false);

            pt->hwndRtPtNSP = CreateWindowExW(0, TEXT("COMBOBOX"), NULL, WS_CHILD | WS_VISIBLE | CBS_DROPDOWN,
                0, 0, xSzNumShPt, cyChar * 10, hwndMain, (HMENU)IDC_VIA_PT_NSP, hInst, NULL);
            for (int k = 0; k <= 5; k += 1)
            {
                if (trackSource == TS_ROUTE) {
                    SendMessage(pt->hwndRtPtNSP, (UINT)CB_ADDSTRING, (WPARAM)0, (LPARAM)nShPtTextRt[k]);// Add option strings to combobox
                }
                else {
                    SendMessage(pt->hwndRtPtNSP, (UINT)CB_ADDSTRING, (WPARAM)0, (LPARAM)nShPtTextTrk[k]);// Add option strings to combobox
                }
            }
            SendMessage(pt->hwndRtPtNSP, CB_SETCURSEL, (WPARAM)(pt->nShPtIdx), (LPARAM)0);
        }

        // always a via point at create, so no shaping point sub-numbering here
        if (numStyIdx == NS_ALL) { // continuous numbering of all points
            expPtNum += pt->nShpPts + 1;
        }
        else if (numStyIdx == NS_VIA) { // continuous waypoint numbering with shaping point sub-numbering
            expPtNum++;
        } // else NS_NONE
    }
}

// Create primary route point detail windows
// Window sizes are set here. Window positions are set in t2rPositionChildWindows()
void t2rCreatePrmRtPtDetailWindows(void)
{
    RoutePoint* pt;
    SIZE szStr;
    HDC hdc = GetDC(hwndMain);

    GetTextExtentPoint32W(hdc, L"Primary Route Point Detailed Settings:  ", 40, &szStr);
    hwndDetailLbl = t2rCreateLabelWindow(hwndMain, 0, 0, szStr.cx, cyChar, L"Primary Route Point Detailed Settings:");

    GetTextExtentPoint32W(hdc, L"Shaping  ", 9, &szStr);
    hwndShapeLbl = t2rCreateLabelWindow(hwndMain, 0, 0, szStr.cx, cyChar, L"Shaping");
     
    GetTextExtentPoint32W(hdc, L"Exclude  ", 9, &szStr);
    hwndExcludeLbl = t2rCreateLabelWindow(hwndMain, 0, 0, szStr.cx, cyChar, L"Exclude");

    if (trackSource != TS_NO_TRK) {
        GetTextExtentPoint32W(hdc, L"Distance  ", 10, &szStr);
        hwndDistlLbl = t2rCreateLabelWindow(hwndMain, 0, 0, szStr.cx, cyChar, L"Distance");

        hwndDistUnits = CreateWindowExW(0, TEXT("COMBOBOX"), NULL, WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST,
            0, 0, cxChar * 12, cyChar * 6, hwndMain, (HMENU)IDC_DIST_UNITS, hInst, NULL);
        for (int k = 0; k <= 1; k += 1)
        {
            SendMessage(hwndDistUnits, (UINT)CB_ADDSTRING, (WPARAM)0, (LPARAM)dstUnText[k]);// Add option strings to combobox
        }
        SendMessage(hwndDistUnits, CB_SETCURSEL, (WPARAM)0, (LPARAM)0);
    }

    GetTextExtentPoint32W(hdc, L"Split  ", 7, &szStr);
    hwndSplitlLbl = t2rCreateLabelWindow(hwndMain, 0, 0, szStr.cx, cyChar, L"Split");

    pt = prmRtPtList.getFirst();
    while (pt) {
        t2rCreatePrmRtPtDetailWindows(pt, hdc);
        pt = prmRtPtList.getNext(pt);
    }
    GetTextExtentPoint32W(hdc, L"  Split Route  ", 15, &szStr);
    hwndSplitButton = CreateWindowExW(0, TEXT("BUTTON"), L"Split Route", WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON | BS_TEXT | WS_DISABLED,
        0, 0, szStr.cx, cyChar * 2, hwndMain, (HMENU)IDC_SPLT_BTN, hInst, NULL);

    hwndExportButton = CreateWindowExW(0, TEXT("BUTTON"), L"Export", WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON | BS_TEXT,
        0, 0, cyChar * 6, cyChar * 3, hwndMain, (HMENU)IDC_EXPORT_BUTTON, hInst, NULL);

    hwndExportInfo = CreateWindowExW(0, TEXT("STATIC"), NULL, WS_CHILD | WS_VISIBLE | SS_LEFT,
        0, 0, (int)(MAX_EXP_INFO_LEN * cxChar * 1.1), cyChar * 3, hwndMain, nullptr, hInst, NULL);
    t2rUpdateExportInfoDisplay();

    GetTextExtentPoint32W(hdc, L"Exported to: ", 13, &szStr);
    hwndExpFileLbl = t2rCreateLabelWindow(hwndMain, 0, 0, szStr.cx, cyChar, L"Exported to:");
    hwndExpFile = CreateWindowExW(0, TEXT("STATIC"), NULL, WS_CHILD | WS_VISIBLE | SS_LEFT,
        0, 0, (int)(MAX_PATH * cxChar * 1.1), cyChar, hwndMain, nullptr, hInst, NULL);
    ShowWindow(hwndExpFileLbl, SW_HIDE);
    ShowWindow(hwndExpFile, SW_HIDE);

    hwndExcSep = CreateWindowExW(0, TEXT("STATIC"), NULL, WS_CHILD | WS_VISIBLE | SS_ETCHEDHORZ,
        0, 0, xSzSep, HT_SEP, hwndMain, nullptr, hInst, NULL);
    ShowWindow(hwndExcSep, SW_HIDE);
    GetTextExtentPoint32W(hdc, L"Route Points Excluded From Route:  ", 33, &szStr);
    hwndExcListLbl = t2rCreateLabelWindow(hwndMain, 0, 0, szStr.cx, cyChar, L"Route Points Excluded From Route:");
    ShowWindow(hwndExcListLbl, SW_HIDE);
    GetTextExtentPoint32W(hdc, L"Exclude  ", 9, &szStr);
    hwnd2ndExcLbl = t2rCreateLabelWindow(hwndMain, 0, 0, szStr.cx, cyChar, L"Exclude");
    ShowWindow(hwnd2ndExcLbl, SW_HIDE);

    ReleaseDC(hwndMain, hdc);

    // needed when some exports are deselected prior to import
    t2rUpdateDspOnRtExportSelect();
    t2rUpdateDspWpTrkRtSelect();

    t2rUpdateMainWindowSize(t2rPositionChildWindows());

    InvalidateRect(hwndMain, NULL, true);
    UpdateWindow(hwndMain);
}

// Update the height of the parent window, and the scroll range, to accomodate a total
// client height of ySzClient, but limit window size to the bottom of the screen.
// Scroll position is not adjusted.
void t2rUpdateMainWindowSize(int ySzClient)
{
    SCROLLINFO si;
    HMONITOR hMon;       // handle to the monitor the main window is currently [mostly] on
    MONITORINFO monInfo; // monitor information
    RECT mainWndRect;    // main [aka parent] window rectangle
    int mainWndVertSz = HT_WIN_TITLE + cyMenu + ySzClient + cyHScroll;  // desired main window height

    si.cbSize = sizeof(si);
    si.fMask = SIF_RANGE;
    si.nMin = 0;
    si.nMax = ySzClient;
    SetScrollInfo(hwndMain, SB_VERT, &si, true);

    hMon = MonitorFromWindow(hwndMain, MONITOR_DEFAULTTOPRIMARY);  // current monitor
    monInfo.cbSize = sizeof(MONITORINFO);
    GetMonitorInfoW(hMon, &monInfo);
    GetWindowRect(hwndMain, &mainWndRect); // current main window size (screen coordinates)

    SetWindowPos(hwndMain, NULL, 0, 0,
        xSzMain, min(mainWndVertSz, monInfo.rcWork.bottom - mainWndRect.top),
        SWP_NOMOVE | SWP_NOZORDER);

    si.fMask = SIF_PAGE;
    si.nPage = cyClient;
    SetScrollInfo(hwndMain, SB_VERT, &si, true);
}

// Update the content of the primary route point detail windows.
// Called whenever anything changes that affects the information displayed
void t2rUpdateRtPtDetailContent()
{
    RoutePoint* pt;     // current primary route point
    RoutePoint* nextPt; // next primary route point
    wchar_t newPtPrefix[MAX_PREFIX_LEN];   // point numbering prefix
    wchar_t ptPrefix[MAX_PREFIX_LEN] = L"";// point numbering prefix
    wchar_t ptStNumTxt[MAX_START_NUM_LEN]; // point numbering starting number (as text)
    long expPtNum = 0;                     // point numbering, start numbering at zero unless something else is specified
    int subNum = 0; // shaping point sub-numbering; used only if numStyIdx == NS_VIA (waypoints continuous)
    wchar_t ptNameLead[MAX_NAME_LEAD]; // point name lead (prefix and number)
    wchar_t addShpTrk[MAX_NAME_LEN]; // additional shaping point line
    double distUnitMult = 0; // distance units

    if (hwndDistUnits) {
        distUnitMult = unitMult[(int)SendMessage(hwndDistUnits, CB_GETCURSEL, (WPARAM)0, (LPARAM)0)];
    }

    updatingViaPtDetail = true;

    pt = prmRtPtList.getFirst();

    while (pt) {
        // The first primary route point always has prefix and starting number windows; others do where (if) the route is split
        // Either or both may be blank however.
        if (pt->hwndPtPrfx) {
            GetWindowTextW(pt->hwndPtPrfx, newPtPrefix, MAX_PREFIX_LEN);
            if (lstrlenW(newPtPrefix)) {
                wcscpy_s(ptPrefix, MAX_PREFIX_LEN, newPtPrefix);
            } // else continue with the previous prefix, if any

            GetWindowTextW(pt->hwndPtStNum, ptStNumTxt, MAX_START_NUM_LEN);
            if (lstrlenW(ptStNumTxt)) {
                expPtNum = wcstol(ptStNumTxt, NULL, 10);
                subNum = 0;
            }
        }
        if (numStyIdx == NS_NONE) {
            _snwprintf_s(ptNameLead, MAX_NAME_LEAD, _TRUNCATE, L"%s  ", ptPrefix);
        }
        else if ((numStyIdx == NS_VIA) && pt->shPt) {
            // continuous numbering of via points with sub-numbering of shaping points, and this is a shaping point
            subNum++;
            _snwprintf_s(ptNameLead, MAX_NAME_LEAD, _TRUNCATE, L"%s%02d.%d  ", ptPrefix, expPtNum, subNum);
        }
        else {
            // continuous numbering of all points or not a shaping point (or both)
            _snwprintf_s(ptNameLead, MAX_NAME_LEAD, _TRUNCATE, L"%s%02d  ", ptPrefix, expPtNum);
        }
        SetWindowTextW(pt->hwndRtPtLd, ptNameLead);

        nextPt = prmRtPtList.getNext(pt);
        if (!nextPt) {
            break; // done
        }

        if (pt->hwndRtPtAdd) {
            if (trackSource == TS_ROUTE) {
                _snwprintf_s(addShpTrk, MAX_NAME_LEN, _TRUNCATE, L"%d Additional shaping points from %d intermediate road changes", pt->nShpPts, pt->nRdIdChg);
            }
            else {
                _snwprintf_s(addShpTrk, MAX_NAME_LEN, _TRUNCATE, L"%d Additional shaping points from %d intermediate track points", pt->nShpPts, pt->nTrkPts);
            }
            SetWindowTextW(pt->hwndRtPtAdd, addShpTrk);
        }
        subNum += pt->nShpPts;

        if (pt->posInRt == RTPOS_LAST) {
            // End of a split route so this point:
            // - is a via point, never a shaping point
            // - never has any following shaping points.
            // The next point:
            // - starts a new route
            // - is also a via point, never a shaping point
            // - gets the same expPtNum, unless it got a new starting number above, but
            //   but either way, sub-numbering restarts.
            subNum = 0;
        }
        else if (numStyIdx == NS_ALL) {
            // continuous numbering of all points
            expPtNum += pt->nShpPts + 1;
        }
        else if ((numStyIdx == NS_VIA) && nextPt && !nextPt->shPt)
        {
            // continuous numbering of via points with sub-numbering of shaping points
            // and the next point, if any, is not a shaping point
            expPtNum++;
            subNum = 0;
        }
        // else NS_NONE - no numbering (or no next point)
        pt = nextPt;
    }
    updatingViaPtDetail = false;
}

// Remove previous prefix and/or point numbering, if any, from all primary route point names
void t2rRemovePreviousNumbering(void)
{
    RoutePoint* pt = prmRtPtList.getFirst();
    wchar_t* pos;
    wchar_t tempName[MAX_NAME_LEN];

    while (pt) {
        if (pos = wcsstr(pt->ptName, L": ")) {
            wcscpy_s(tempName, MAX_NAME_LEN, pos + 2);
            wcscpy_s(pt->ptName, MAX_NAME_LEN, tempName);
            SetWindowTextW(pt->hwndRtPtName, pt->ptName);
        }
        pt = prmRtPtList.getNext(pt);
    }
    importedNum = false;
    ShowWindow(hwndRemoveButton, SW_HIDE);
}

// Show / hide almost all route related windows based on whether export route is selected or not
// see also t2rUpdateWpTrkRtExport(), which is always called after this
void t2rUpdateDspOnRtExportSelect(void)
{
    int showState;

    if (expRt) {
        showState = SW_NORMAL;
    }
    else {
        showState = SW_HIDE;
    }

    // hwndRouteNameLbl && hwndRouteName are handled in t2rUpdateWpTrkRtExport()

    ShowWindow(hwndRtPtNameCheckLbl, showState);
    ShowWindow(hwndVpLocCheckLbl, showState);
    ShowWindow(hwndVpLocCheck, showState);
    if (trackSource != TS_NO_TRK) {
        ShowWindow(hwndNumShPtLbl, showState);
        ShowWindow(hwndNumShPt, showState);
        ShowWindow(hwndNumShPtLbl2, showState);
    }
    ShowWindow(hwndPtNameLbl, showState);
    if (importedNum) {
        ShowWindow(hwndRemoveButton, showState);
    }  // else it will remain hidden
    ShowWindow(hwndPtName, showState);
    ShowWindow(hwndRtPtNameCheck, showState);
    ShowWindow(hwndPtNumLbl, showState);
    ShowWindow(hwndPtNumSty, showState);
    ShowWindow(hwndLwSep, showState);
    ShowWindow(hwndDetailLbl, showState);
    ShowWindow(hwndShapeLbl, showState);
    ShowWindow(hwndExcludeLbl, showState);
    if (hwndDistlLbl) {
        ShowWindow(hwndDistlLbl, showState);
    }
    ShowWindow(hwndSplitlLbl, showState);
    if (hwndDistUnits) {
        ShowWindow(hwndDistUnits, showState);
    }
    ShowWindow(hwndSplitButton, showState);

    if (showState == SW_HIDE) {
        ShowWindow(hwndExpFileLbl, showState);
        ShowWindow(hwndExpFile, showState);
    }

    RoutePoint* pt = prmRtPtList.getFirst();
    while (pt) {
        if (pt->hwndPtPrfx) {
            ShowWindow(pt->hwndPtPrfx, showState);
            ShowWindow(pt->hwndPtPrfxLbl, showState);
            ShowWindow(pt->hwndPtStNum, showState);
            ShowWindow(pt->hwndPtStNumLbl, showState);
        }
        ShowWindow(pt->hwndRtPtName, showState);
        ShowWindow(pt->hwndRtPtLd, showState);
        if (pt->hwndRtPtSP) {
            ShowWindow(pt->hwndRtPtSP, showState);
        }
        if (pt->hwndRtPtEx) {
            ShowWindow(pt->hwndRtPtEx, showState);
        }
        if (pt->hwndRtPtSlt) {
            ShowWindow(pt->hwndRtPtSlt, showState);
        }
        if (pt->hwndRtPtDist) {
            ShowWindow(pt->hwndRtPtDist, showState);
        }
        if (pt->hwndRtPtNSP) {
            ShowWindow(pt->hwndRtPtNSP, showState);
        }
        if (pt->hwndRtPtAdd) {
            ShowWindow(pt->hwndRtPtAdd, showState);
        }

        pt = prmRtPtList.getNext(pt);
    }

    pt = excPtList.getFirst();
    if (pt) {
        // if the exclude list is empty, these are already hidden
        ShowWindow(hwndExcSep, showState);
        ShowWindow(hwndExcListLbl, showState);
        ShowWindow(hwnd2ndExcLbl, showState);
    }
    while (pt) {
        ShowWindow(pt->hwndRtPtName, showState);
        // if pt is on the excPtList, it must have a hwndRtPtEx
        ShowWindow(pt->hwndRtPtEx, showState);
        pt = excPtList.getNext(pt);
    }
}

// Show / hide route / track name depending on whether track and/or route export is selected
// Also show / hide the export button depending on whether any export is selected
void t2rUpdateDspWpTrkRtSelect(void)
{
    int showState;

    if (expTrk || expRt) {
        showState = SW_NORMAL;
    }
    else {
        showState = SW_HIDE;
    }
    ShowWindow(hwndGblSet, showState);
    ShowWindow(hwndRouteNameLbl, showState);
    ShowWindow(hwndRouteName, showState);

    if (expWpt || expTrk || expRt) {
        showState = SW_NORMAL;
    }
    else {
        showState = SW_HIDE;
    }
    ShowWindow(hwndExportButton, showState);
    ShowWindow(hwndExportInfo, showState);
}

// Update the display of distance between each primary route point and its closest track point.
// Called whenever the distance units (feet or meters) changes.
void t2rUpdateRtPtDetailDist()
{
    RoutePoint* pt = prmRtPtList.getFirst();
    double distUnitMult = unitMult[(int)SendMessage(hwndDistUnits, CB_GETCURSEL, (WPARAM)0, (LPARAM)0)];
    wchar_t clsTrkPtDist[MAX_TRACK_INFO];

    while (pt) {
        _snwprintf_s(clsTrkPtDist, MAX_TRACK_INFO, _TRUNCATE, L"%8.0lf", pt->clsTrkPtDist * distUnitMult);  // always in feet here; no multiplier needed
        SetWindowTextW(pt->hwndRtPtDist, clsTrkPtDist);
        pt = prmRtPtList.getNext(pt);
    }
}

// Called whenever the substitute original point name checkbox changes or
// the point name changes
void t2rUpdateRtPtDetailName(bool substitute)
{
    RoutePoint* pt = prmRtPtList.getFirst();
    wchar_t ptName[MAX_NAME_LEN];

    while (pt) {
        if (substitute) {
            _snwprintf_s(ptName, MAX_NAME_LEN, _TRUNCATE, L"%s %d", ptNameBase, pt->clsTrkPtIdx);
            SetWindowTextW(pt->hwndRtPtName, ptName);
        }
        else {
            SetWindowTextW(pt->hwndRtPtName, pt->ptName);
        }
        pt = prmRtPtList.getNext(pt);
    }
}

// Called whenever a primary route detail shaping point point button is clicked.
// Sets the point's shaping point state (shPt) and enables or disables the point's split options accordingly.
// Routes can not be split at shaping points, only via points, so a shaping point can not be selected for route split.
void t2rUpdateRtPtShpStateAndSltOptions() {
    RoutePoint* pt = prmRtPtList.getFirst();

    while (pt) {
        if (pt->hwndRtPtSP) {
            pt->shPt = (SendMessage(pt->hwndRtPtSP, BM_GETCHECK, (WPARAM)0, (LPARAM)0) == BST_CHECKED);
            if (pt->hwndRtPtSlt) {
                EnableWindow(pt->hwndRtPtSlt,!pt->shPt);
            }
        }
        pt = prmRtPtList.getNext(pt);
    }
}

// Update route point split state and enable / disable shaping point <-> via point options whenever a primary route point detail split checkbox is clicked.
// Routes can not be split at shaping points, only via points, so a via point selected for route split can not be changed to a shaping point.
void t2rUpdateRtPtShapeOptions() {
    RoutePoint* pt = prmRtPtList.getFirst();
    bool split = false;
    bool enableSplit = false;

    while (pt) {
        if (pt->hwndRtPtSlt) {
            if (SendMessage(pt->hwndRtPtSlt, BM_GETCHECK, (WPARAM)0, (LPARAM)0) == BST_CHECKED) {
                split = true;
                enableSplit = true;
            }
            else {
                split = false;
            }
        }
        if (pt->hwndRtPtSP) {
            EnableWindow(pt->hwndRtPtSP, !split);
        }
        pt = prmRtPtList.getNext(pt);
    }
    EnableWindow(hwndSplitButton, enableSplit);
}

// Update individual primary route point's stored % of track points to use as shaping points,
// and that points additional shaping point display.
// Called whenever this parameter is changed for an individual primary route point in the primary route point detail display.
// t2rUpdateGlobalTrkPtPercent() handles the global change of this parameter.
void t2rUpdateRtPtTrkPtPercent(bool selectChange)
{
    int idx = 0;
    double ptPC = 0.0;
    wchar_t addShpTrk[MAX_NAME_LEN];
    RoutePoint* pt = prmRtPtList.getFirst();

    while (pt) {
        if (pt->hwndRtPtNSP) {
            if (selectChange) {
                idx = (int)SendMessage(pt->hwndRtPtNSP, CB_GETCURSEL, (WPARAM)0, (LPARAM)0);
                if (pt->nShPtIdx != idx) {
                    pt->nShPtIdx = idx;
                    pt->shPtPC = nShPtVal[idx];
                    break; // this is the pt that changed
                }
            }
            else {
                // value changed by edit, not drop-down selection
                WCHAR tempStr[10];
                GetWindowTextW(pt->hwndRtPtNSP, tempStr, 10);
                ptPC = wcstod(tempStr, NULL);
                if (pt->shPtPC != ptPC) {
                    ptPC = max(0.0, ptPC);
                    ptPC = min(ptPC, 100.0);
                    pt->shPtPC = round(ptPC * RND_FACTOR_2) / RND_FACTOR_2;
                    break; // this is the pt that changed
                }
            }
        }
        pt = prmRtPtList.getNext(pt);
    }
    if (pt && pt->hwndRtPtAdd) {
        if (trackSource == TS_ROUTE) {
            pt->nShpPts = (int)(pt->nRdIdChg * pt->shPtPC / 100);
            _snwprintf_s(addShpTrk, MAX_NAME_LEN, _TRUNCATE, L"%d Additional shaping points from %d intermediate road changes", pt->nShpPts, pt->nRdIdChg);
        }
        else {
            pt->nShpPts = (int)(pt->nTrkPts * pt->shPtPC / 100);
            _snwprintf_s(addShpTrk, MAX_NAME_LEN, _TRUNCATE, L"%d Additional shaping points from %d intermediate track points", pt->nShpPts, pt->nTrkPts);
        }
        SetWindowTextW(pt->hwndRtPtAdd, addShpTrk);
    }
}

// Move primary route points between the prmRtPtList and the excPtList as needed.
// Called whenever the exclude checkbox selection changes for any primary or excluded route point.
void t2rUpdateRtPtLists()
{
    RoutePoint* pt = prmRtPtList.getFirst();
    RoutePoint* next;

    // first handle point moving from primary to exclude list, if any
    while (pt) {
        next = prmRtPtList.getNext(pt);
        if (pt->hwndRtPtEx && (SendMessage(pt->hwndRtPtEx, BM_GETCHECK, (WPARAM)0, (LPARAM)0) == BST_CHECKED)) {
            prmRtPtList.remove(pt);
            excPtList.orderedInsert(pt);
            ShowWindow(pt->hwndRtPtLd, SW_HIDE);
            ShowWindow(pt->hwndRtPtSP, SW_HIDE);
            ShowWindow(pt->hwndRtPtSlt, SW_HIDE);
            if (pt->hwndRtPtDist) {
                ShowWindow(pt->hwndRtPtDist, SW_HIDE);
            }
            if (pt->hwndRtPtNSP) {
                ShowWindow(pt->hwndRtPtNSP, SW_HIDE);
            }
            if (pt->hwndRtPtAdd) {
                ShowWindow(pt->hwndRtPtAdd, SW_HIDE);
            }
        }
        pt = next;
    }
    // Check if primary route point at either end of track has been removed;
    // generate end primary route point(s) if so.
    t2rCheckAddViaPtsAtTrackEnds();

    // now handle point moving from exclude back to primary list, if any
    pt = excPtList.getFirst();
    while (pt) {
        next = excPtList.getNext(pt);
        // if pt is on the excPtList, it must have a hwndRtPtEx
        if (SendMessage(pt->hwndRtPtEx, BM_GETCHECK, (WPARAM)0, (LPARAM)0) != BST_CHECKED) {
            excPtList.remove(pt);
            prmRtPtList.orderedInsert(pt);
            ShowWindow(pt->hwndRtPtLd, SW_NORMAL);
            ShowWindow(pt->hwndRtPtSP, SW_NORMAL);
            ShowWindow(pt->hwndRtPtSlt, SW_NORMAL);
            if (pt->hwndRtPtDist) {
                ShowWindow(pt->hwndRtPtDist, SW_NORMAL);
            }
            if (pt->hwndRtPtNSP) {
                ShowWindow(pt->hwndRtPtNSP, SW_NORMAL);
            }
            if (pt->hwndRtPtAdd) {
                ShowWindow(pt->hwndRtPtAdd, SW_NORMAL);
            }
        }
        pt = next;
    }
    // Check if original primary route point(s) at ends of track have been restored;
    // remove generated begin and/or end points if so.
    t2rCheckRemoveViaPtsAtTrackEnds();
    // Correct the intermediate point counts for the changed primary route point list.
    t2rSetIntermediateCounts();

    // Show or hide the excludued point list headings
    if (excPtList.nPts()) {
        ShowWindow(hwndExcSep, SW_NORMAL);
        ShowWindow(hwndExcListLbl, SW_NORMAL);
        ShowWindow(hwnd2ndExcLbl, SW_NORMAL);
    }
    else {
        ShowWindow(hwndExcSep, SW_HIDE);
        ShowWindow(hwndExcListLbl, SW_HIDE);
        ShowWindow(hwnd2ndExcLbl, SW_HIDE);
    }
}

// Update the export information display
void t2rUpdateExportInfoDisplay()
{
    RoutePoint* pt = NULL;
    wstring  exportInfoString = L"";
    int rtNum;   // route number
    int fTrkPt;  // index of first track point in each route
    int nWayPts; // number of waypoints associated with each route
    int nViaPts; // number of via points in each route
    int nAdlSPs; // number of additional shaping points in each route
    int nPrmSPs; // number of primary shaping points in each route
    int nTrkPts; // number of track points associated with each route

    if (hwndExportInfo) {
        pt = prmRtPtList.getFirst();
        for (rtNum = 1; rtNum <= prmRtPtList.getNumRoutes(); rtNum++) {
            if (pt) {
                fTrkPt = pt->clsTrkPtIdx;
            }
            nWayPts = nViaPts = nAdlSPs = nPrmSPs = nTrkPts = 0;
            while (pt) {
                nWayPts++;
                nAdlSPs += pt->nShpPts;
                if (pt->shPt) {
                    nPrmSPs++;
                }
                else {
                    nViaPts++;
                }
                if (pt->posInRt == RTPOS_LAST) {
                    nTrkPts = pt->clsTrkPtIdx - fTrkPt + 1;
                    pt = prmRtPtList.getNext(pt);
                    break; // Done with this route; break out of while.
                }
                pt = prmRtPtList.getNext(pt);
            }
            // add this route to the export info
            if (prmRtPtList.getNumRoutes() == 1) {
                if (expWpt) {
                    exportInfoString += to_wstring(nWayPts) + L" Waypoints\n";
                }
                if (expRt) {
                    exportInfoString += L"Route with: " + to_wstring(nViaPts) + L" via points, " + to_wstring(nPrmSPs)
                        + L" primary shaping points, and " + to_wstring(nAdlSPs) + L" additional shaping points\n";
                }
                if (expTrk) {
                    exportInfoString += L"Track with: " + to_wstring(nTrkPts) + L" track points";
                }
            }
            else { // more than one route
                exportInfoString += L"Part " + to_wstring(rtNum) + L":  "
                    + to_wstring(nWayPts) + L" waypts, Route: " + to_wstring(nViaPts) + L" via, "
                    + to_wstring(nPrmSPs) + L" prm and " + to_wstring(nAdlSPs) + L" adl shaping pts, Track: " + to_wstring(nTrkPts) + L" pts\n";
            }
            // move on to the next route, if any
        }

        SetWindowTextW(hwndExportInfo, exportInfoString.c_str());
        UpdateWindow(hwndExportInfo);
    }
}

// Assign the global % of track or road change points to be exported as additional shaping points
// to each individual primary route point and update each primary route point's nShpPts. 
// This overwrites any route point specific values with the global value.
// Called when finished importing a file and whenever the value in the global hwndNumShPt control changes.
void t2rUpdateGlobalTrkPtPercent(bool setSelection)
{
    wchar_t addShpTrk[MAX_NAME_LEN];
    RoutePoint* pt = prmRtPtList.getFirst();

    while (pt) {
        // first update each point's values
        pt->shPtPC = shPtPC;
        if (setSelection) {
            pt->nShPtIdx = nShPtIdx;
        }
        if (trackSource == TS_ROUTE) {
            pt->nShpPts = (int)(pt->nRdIdChg * shPtPC / 100);
        }
        else {
            pt->nShpPts = (int)(pt->nTrkPts * shPtPC / 100);
        }
        // then update the windows to display the new values
        if (pt->hwndRtPtNSP) {
            if (setSelection) {
                // global selection changed, so set each primary route point's selection
                SendMessage(pt->hwndRtPtNSP, CB_SETCURSEL, (WPARAM)(pt->nShPtIdx), (LPARAM)0);
            }
            else {
                // global value (edit) changed (not selected from the drop-down) so set each primary route point's values
                wstringstream pcStrm;
                pcStrm << fixed << right << setw(6) << setprecision(2) << shPtPC;
                wstring pcStr(pcStrm.str());
                SendMessageW(pt->hwndRtPtNSP, WM_SETTEXT, 0, (LPARAM)pcStr.c_str());
            }
            if (pt->hwndRtPtAdd) {
                // and update each primary route point's additional shaping point display line
                if (trackSource == TS_ROUTE) {
                    _snwprintf_s(addShpTrk, MAX_NAME_LEN, _TRUNCATE, L"%d Additional shaping points from %d intermediate road changes", pt->nShpPts, pt->nRdIdChg);
                }
                else {
                    _snwprintf_s(addShpTrk, MAX_NAME_LEN, _TRUNCATE, L"%d Additional shaping points from %d intermediate track points", pt->nShpPts, pt->nTrkPts);
                }
                SetWindowTextW(pt->hwndRtPtAdd, addShpTrk);
            }
        }
        pt = prmRtPtList.getNext(pt);
    }
}

// Called when the split route button is clicked to split the current route(s) into one or
// more additional routes(s) per any currently selected pt->hwndRtPtSlt.
// Once split, a route point cannot be "unsplit".
void t2rSplitRoute() {
    RoutePoint* pt = prmRtPtList.getFirst();
    RoutePoint* rtEndPt; // new (duplicate) point to form the end of a route being split
    int addedRoutes = 0; // number of routes added by this split
    int totAddedPx = 0;  // total additional main window height (in pixels) needed as a result of the route split
    int yPos = 0;        // y position
    bool shapingPt = false; // true for a shping points; the first point can never be a shaping point
    SCROLLINFO si;

    double distUnitMult = 0; // distance unit multiplier
    HDC hdc = GetDC(hwndMain);

    if (hwndDistUnits) {
        distUnitMult = unitMult[(int)SendMessage(hwndDistUnits, CB_GETCURSEL, (WPARAM)0, (LPARAM)0)];
    }

    while (pt) {
        if (pt->hwndRtPtSlt && (SendMessage(pt->hwndRtPtSlt, BM_GETCHECK, (WPARAM)0, (LPARAM)0) == BST_CHECKED)) {
            // Split the route at this primary route point.  Create a new (duplicate) point to end the current route.
            rtEndPt = prmRtPtList.split(pt); // this increments the number of routes in the list
            t2rCreatePrmRtPtDetailWindows(rtEndPt, hdc); // rtEndPt->hwndRtPtLd will be set in t2rUpdateRtPtDetailContent()

            // and set up this pt to begin the next route
            DestroyWindow(pt->hwndRtPtSP);
            pt->hwndRtPtSP = NULL;
            DestroyWindow(pt->hwndRtPtSlt);
            pt->hwndRtPtSlt = NULL;

            pt->hwndPtPrfxLbl = t2rCreateLabelWindow(hwndMain, 0, 0, xSzPtPrfxLbl, cyChar, L"Prefix:");
            pt->hwndPtPrfx = CreateWindowExW(0, TEXT("EDIT"), NULL, WS_CHILD | WS_VISIBLE | ES_LEFT,
                0, 0, (int)(MAX_PREFIX_LEN * cxChar * 1.1), cyChar, hwndMain, (HMENU)IDC_DT_PTPRFX, hInst, NULL);
            SendMessage(pt->hwndPtPrfx, EM_SETLIMITTEXT, MAX_PREFIX_LEN - 1, 0);

            pt->hwndPtStNumLbl = t2rCreateLabelWindow(hwndMain, 0, 0, xSzPtStNumLbl, cyChar, L"Starting at:");
            pt->hwndPtStNum = CreateWindowExW(0, TEXT("EDIT"), NULL, WS_CHILD | WS_VISIBLE | ES_RIGHT | ES_NUMBER,
                0, 0, (int)(MAX_START_NUM_LEN * cxChar * 1.1), cyChar, hwndMain, (HMENU)IDC_DT_PTSTNUM, hInst, NULL);
            SendMessage(pt->hwndPtStNum, EM_SETLIMITTEXT, MAX_START_NUM_LEN - 1, 0);

            addedRoutes++;
        }
        pt = prmRtPtList.getNext(pt);
    }
    ReleaseDC(hwndMain, hdc);

    EnableWindow(hwndSplitButton, false);

    // Prevent any further route point exclusion from or inclusion to any route, because capability to remove / reinsert
    // route points from / to split routes would require code complexity that is not worth the functionality it would provide.
    prmRtPtList.disableExclude();
    excPtList.disableExclude();

    // The vertical heights of the export info and export file windows need to be expanded to accomodate the new route(s).
    // For a single route, export info also has 2 lines for waypoints and track.
    SetWindowPos(hwndExportInfo, NULL, 0, 0, (int)(MAX_EXP_INFO_LEN * cxChar * 1.1), cyChar * max(3, prmRtPtList.getNumRoutes()),
        SWP_NOOWNERZORDER | SWP_NOREDRAW | SWP_NOMOVE | SWP_NOZORDER);

    SetWindowPos(hwndExpFile, NULL, 0, 0, (int)(MAX_PATH * cxChar * 1.1), cyChar * prmRtPtList.getNumRoutes(),
        SWP_NOOWNERZORDER | SWP_NOREDRAW | SWP_NOMOVE | SWP_NOZORDER);

    t2rUpdateMainWindowSize(t2rPositionChildWindows()); // determine new window size needed and resize main window accordingly

    si.cbSize = sizeof(si);
    si.fMask = SIF_RANGE | SIF_PAGE | SIF_POS;
    GetScrollInfo(hwndMain, SB_VERT, &si);
    // Set the scroll position to the bottom
    si.nPos = si.nMax - si.nPage;
    SetScrollInfo(hwndMain, SB_VERT, &si, true);

    t2rPositionChildWindows(); // re-positions child windows in the newly sized main window

    InvalidateRect(hwndMain, NULL, true);
    UpdateWindow(hwndMain);
}
