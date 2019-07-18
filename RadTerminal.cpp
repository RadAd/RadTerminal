#include <windows.h>
#include <windowsx.h>
#include <tchar.h>
#include <string>
#include "ProcessUtils.h"
#include "WinUtils.h"
#include "DarkMode.h"
#include "libtsm\src\tsm\libtsm.h"
#include "libtsm\external\xkbcommon\xkbcommon-keysyms.h"
#include "resource.h"

// TODO
// https://stackoverflow.com/questions/5966903/how-to-get-mousemove-and-mouseclick-in-bash
// keyboard select mode
// specify an icon on command line
// remove polling
// unicode/emoji
// find
// status bar ???
// tooltip while resizing ???
// flash window on updates
// dynamically change font
// transparency
// hide scrollbar if scrollback not enabled
// support bel
// support
//     ESC [ ? 12 h                                     ATT160 Text Cursor Enable Blinking
//     ESC [ ? 12 l                                     ATT160 Text Cursor Enable Blinking
//     ESC ] 4 ; <i> ; rgb : <r> / <g> / <b> ESC        Modify Screen Colors
// See https://docs.microsoft.com/en-us/windows/console/console-virtual-terminal-sequences
// Tabs in frame - see https://docs.microsoft.com/en-au/windows/desktop/dwm/customframe

#define PROJ_NAME TEXT("RadTerminal")
#define PROJ_CODE TEXT("RadTerminal")
#define REG_BASE  TEXT("Software\\RadSoft\\") PROJ_CODE

template <class T>
bool MemEqual(const T& a, const T& b)
{
    return memcmp(&a, &b, sizeof(T)) == 0;
}

void ShowError(HWND hWnd, LPCTSTR msg, HRESULT hr)
{
    TCHAR fullmsg[1024];
    _stprintf_s(fullmsg, _T("%s: 0x%08x"), msg, hr);
    MessageBox(hWnd, fullmsg, PROJ_NAME, MB_ICONERROR);
}

#define CHECK(x, r) \
    if (!(x)) \
    { \
        ShowError(hWnd, __FUNCTIONW__ TEXT(": ") TEXT(#x), HRESULT_FROM_WIN32(GetLastError())); \
        return (r); \
    }

#define CHECK_ONLY(x) \
    if (!(x)) \
    { \
        ShowError(hWnd, __FUNCTIONW__ TEXT(": ") TEXT(#x), HRESULT_FROM_WIN32(GetLastError())); \
    }

#define VERIFY(x) \
    if (!(x)) \
    { \
        ShowError(hWnd, __FUNCTIONW__ TEXT(": ") TEXT(#x), 0); \
    }

HWND CreateRadTerminalFrame(HINSTANCE hInstance);
LRESULT CALLBACK RadTerminalWindowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

HWND ActionNewWindow(HWND hWnd, bool bParseCmdLine, const std::tstring& profile);

ATOM RegisterRadTerminal(HINSTANCE hInstance)
{
    WNDCLASS wc = {};

    wc.lpfnWndProc = RadTerminalWindowProc;
    wc.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_ICON1));
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    //wc.hbrBackground = GetSysColorBrush(COLOR_WINDOW);
    wc.hInstance = hInstance;
    wc.lpszClassName = PROJ_CODE;

    return RegisterClass(&wc);
}

ATOM GetRadTerminalAtom(HINSTANCE hInstance)
{
    static ATOM g_atom = RegisterRadTerminal(hInstance);
    return g_atom;
}

struct RadTerminalCreate
{
    int iFontHeight;
    std::tstring strFontFace;
    std::tstring strScheme;
    COORD szCon;
    int sb;
    std::tstring strCommand;
};

void LoadRegistry(RadTerminalCreate& rtc, LPCWSTR strSubKey)
{
    HKEY hMainKey = NULL;
    if (RegOpenKey(HKEY_CURRENT_USER, REG_BASE TEXT("\\Profiles"), &hMainKey) == ERROR_SUCCESS)
    {
        HKEY hKey = NULL;
        if (RegOpenKey(hMainKey, strSubKey, &hKey) == ERROR_SUCCESS)
        {
            rtc.iFontHeight = RegGetDWORD(hKey, _T("FontSize"), rtc.iFontHeight);
            rtc.strFontFace = RegGetString(hKey, _T("FontFace"), rtc.strFontFace);
            rtc.strScheme = RegGetString(hKey, _T("Scheme"), rtc.strScheme);
            rtc.szCon.X = (SHORT) RegGetDWORD(hKey, _T("Width"), rtc.szCon.X);
            rtc.szCon.Y = (SHORT) RegGetDWORD(hKey, _T("Height"), rtc.szCon.Y);
            rtc.sb = RegGetDWORD(hKey, _T("Scrollback"), rtc.sb);
            rtc.strCommand = RegGetString(hKey, _T("Command"), rtc.strCommand);

            RegCloseKey(hKey);
        }

        RegCloseKey(hMainKey);
    }
}

void ParseCommandLine(RadTerminalCreate& rtc)
{
    bool command = false;
    for (int i = 1; i < __argc; ++i)
    {
        LPCTSTR arg = __targv[i];
        if (command)
        {
            rtc.strCommand += ' ';
            rtc.strCommand += arg;
        }
        else if (_tcsicmp(arg, _T("-w")) == 0)
            rtc.szCon.X = _tstoi(__targv[++i]);
        else if (_tcsicmp(arg, _T("-h")) == 0)
            rtc.szCon.Y = _tstoi(__targv[++i]);
        else if (_tcsicmp(arg, _T("-scheme")) == 0)
            rtc.strScheme = __targv[++i];
        else if (_tcsicmp(arg, _T("-font_face")) == 0)
            rtc.strFontFace = __targv[++i];
        else if (_tcsicmp(arg, _T("-font_size")) == 0)
            rtc.iFontHeight = _tstoi(__targv[++i]);
        else if (_tcsicmp(arg, _T("-sb")) == 0)
            rtc.sb = _tstoi(__targv[++i]);
        else
        {
            rtc.strCommand = arg;
            command = true;
        }
    }
}

RadTerminalCreate GetTerminalCreate(bool bParseCmdLine, std::tstring profile)
{
    if (profile.empty())
        profile = RegGetString(HKEY_CURRENT_USER, REG_BASE, TEXT("Profile"), TEXT("Cmd"));

    RadTerminalCreate rtc = {};
    rtc.iFontHeight = 16;
    rtc.strFontFace = _T("Consolas");
    //rtc.strScheme = _T("solarized");
    rtc.szCon = { 80, 25 };
    rtc.sb = 1000;
    //rtc.strCmd = _T("%COMSPEC%");
    //rtc.strCmd = _T("cmd");

    LoadRegistry(rtc, _T("Default"));
    LoadRegistry(rtc, profile.c_str());
    if (bParseCmdLine)
        ParseCommandLine(rtc);

    return rtc;
}

int WINAPI _tWinMain(HINSTANCE hInstance, HINSTANCE, PTSTR pCmdLine, int nCmdShow)
{
    InitDarkMode();

    HWND hWnd = NULL;
    HWND hWndMDIClient = NULL;
    HACCEL hAccel1 = NULL;
    bool bMDI = RegGetDWORD(HKEY_CURRENT_USER, REG_BASE, TEXT("MDI"), TRUE) > 0;

    CHECK(GetRadTerminalAtom(hInstance), EXIT_FAILURE);

    if (bMDI)
    {
        hWnd = CreateRadTerminalFrame(hInstance);
        CHECK(hWnd, EXIT_FAILURE);

        hWndMDIClient = GetMDIClient(hWnd);
        hAccel1 = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDR_ACCELERATOR1));

        HWND hChildWnd = ActionNewWindow(hWnd, true, TEXT(""));

        if (true && hChildWnd != NULL)
        {
            RECT r = {};
            CHECK(GetWindowRect(hChildWnd, &r), EXIT_FAILURE);
            CHECK(UnadjustWindowRectEx(&r, GetWindowStyle(hChildWnd), GetMenu(hChildWnd) != NULL, GetWindowExStyle(hChildWnd)), EXIT_FAILURE);
            CHECK(AdjustWindowRectEx(&r, GetWindowStyle(hWnd), GetMenu(hWnd) != NULL, GetWindowExStyle(hWnd)), EXIT_FAILURE);
            CHECK(SetWindowPos(hWnd, 0, r.left, r.top, r.right - r.left, r.bottom - r.top, SWP_NOMOVE | SWP_NOZORDER), EXIT_FAILURE);
            ShowWindow(hChildWnd, SW_MAXIMIZE);
        }
    }
    else
    {
        RadTerminalCreate rtc = GetTerminalCreate(true, TEXT(""));

        hWnd = CreateWindowEx(
            WS_EX_ACCEPTFILES,
            MAKEINTATOM(GetRadTerminalAtom(hInstance)),
            PROJ_NAME,
            WS_OVERLAPPEDWINDOW | WS_VSCROLL,
            CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
            NULL,       // Parent window
            NULL,       // Menu
            hInstance,
            &rtc
        );
        CHECK(hWnd, EXIT_FAILURE);
    }

    if (g_darkModeEnabled)
    {
        SetWindowTheme(hWnd, L"DarkMode_Explorer", NULL);   // Needed for scrollbar
        AllowDarkModeForWindow(hWnd, true);
        RefreshTitleBarThemeColor(hWnd);
    }

    ShowWindow(hWnd, nCmdShow);

    HACCEL hAccel2 = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDR_ACCELERATOR2));

    MSG msg = {};
    while (GetMessage(&msg, (HWND) NULL, 0, 0))
    {
        if (!TranslateMDISysAccel(hWndMDIClient, &msg) &&
            !TranslateAccelerator(hWnd, hAccel1, &msg) &&
            !TranslateAccelerator(hWnd, hAccel2, &msg))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }

    return EXIT_SUCCESS;
}

template<class T, class U>
struct ThreadData2
{
    typedef void HandleFunc(T hHandle, U hWnd);
    HandleFunc* pFunc;
    T t;
    U u;

    void Do() const
    {
        pFunc(t, u);
    }
};

template<class T>
DWORD WINAPI MyThread(LPVOID lpParameter)
{
    const T* htd = (T*) lpParameter;
    htd->Do();
    delete htd;
    return 0;
}

template<class T, class U>
void CreateThread(void (*pFunc)(T, U), T t, U u)
{
    ThreadData2<T, U>* htd = new ThreadData2<T, U>;
    htd->pFunc = pFunc;
    htd->t = t;
    htd->u = u;
    CreateThread(nullptr, 0, MyThread<ThreadData2<T, U>>, htd, 0, nullptr);
}

#define WM_WATCH (WM_USER + 5)
void WatchThread(HANDLE hHandle, HWND hWnd)
{
    do
    {
        WaitForSingleObject(hHandle, INFINITE);
    } while (SendMessage(hWnd, WM_WATCH, (WPARAM) hHandle, 0) != 0);
}

#define WM_READ (WM_USER + 7)
void ReadThread(HANDLE hHandle, HWND hWnd)
{
    while (true)
    {
        char buf[1024];
        const DWORD toread = ARRAYSIZE(buf);
        DWORD read = 0;
        if (!ReadFile(hHandle, buf, toread, &read, nullptr))
            break;
        SendMessage(hWnd, WM_READ, (WPARAM) buf, read);
    }
    CloseHandle(hHandle);
}

void tsm_log(void *data,
    const char *file,
    int line,
    const char *func,
    const char *subs,
    unsigned int sev,
    const char *format,
    va_list args)
{
    char buf[1024];
    sprintf_s(buf, "tsm_log: %d %s:%d %s %s - ", sev, strrchr(file, '\\'), line, func, subs);
    OutputDebugStringA(buf);
    vsprintf_s(buf, format, args);
    OutputDebugStringA(buf);
    OutputDebugStringA("\n");
}

struct tsm_screen_draw_info
{
    HFONT hFonts[2][2][2]; // bold, italic, underline
    TEXTMETRIC tm;
};

struct tsm_screen_draw_state
{
    HFONT hFont;
    COLORREF bg;
    COLORREF fg;
    BOOL inverse;
};

struct tsm_screen_draw_data
{
    HDC hdc;
    const tsm_screen_draw_info* info;
    COORD cur_pos;
    unsigned int flags;

    POINT pos;
    std::tstring drawbuf;
    tsm_screen_draw_state state;
};

inline SIZE GetCellSize(const tsm_screen_draw_info* di)
{
    return { di->tm.tmAveCharWidth, di->tm.tmHeight };
}

inline POINT GetScreenPos(const tsm_screen_draw_info* di, COORD pos)
{
    SIZE sz = GetCellSize(di);
    return { pos.X * sz.cx, pos.Y * sz.cy };
}

inline COORD GetCellPos(const tsm_screen_draw_info* di, POINT pos)
{
    SIZE sz = GetCellSize(di);
    return { (SHORT) (pos.x / sz.cx), (SHORT) (pos.y / sz.cy) };
}

void Flush(tsm_screen_draw_data* const draw)
{
    if (!draw->drawbuf.empty())
    {
        HFONT hFontOrig = SelectFont(draw->hdc, draw->state.hFont);
        SetBkColor(draw->hdc, draw->state.bg);
        SetTextColor(draw->hdc, draw->state.fg);
        TextOut(draw->hdc, draw->pos.x, draw->pos.y, draw->drawbuf.c_str(), (int) draw->drawbuf.length());
        if (draw->state.inverse)
        {
            SIZE sz = GetCellSize(draw->info);
            sz.cx *= (LONG) draw->drawbuf.length();
            RECT rc = Rect(draw->pos, sz);
            --rc.left;
            InvertRect(draw->hdc, &rc);
        }
        draw->drawbuf.clear();
        SelectFont(draw->hdc, hFontOrig);
    }
}

static size_t ucs4_to_utf16(uint32_t wc, wchar_t *wbuf)
{
    if (wc < 0x10000)
    {
        wbuf[0] = wc;
        return 1;
    }
    else
    {
        wc -= 0x10000;
        wbuf[0] = 0xD800 | ((wc >> 10) & 0x3FF);
        wbuf[1] = 0xDC00 | (wc & 0x3FF);
        return 2;
    }
}

int tsm_screen_draw(struct tsm_screen *con,
    uint64_t id,
    const uint32_t *ch,
    size_t len,
    unsigned int width,
    unsigned int posx,
    unsigned int posy,
    const struct tsm_screen_attr *attr,
    tsm_age_t age,
    void *data)
{
    tsm_screen_draw_data* const draw = (tsm_screen_draw_data*) data;
    // TODO Protect, Blink
    COORD pos = { (SHORT) posx, (SHORT) posy };
    tsm_screen_draw_state state = {};
    POINT scpos = GetScreenPos(draw->info, pos);
    state.hFont = draw->info->hFonts[attr->bold][attr->italic][attr->underline];
    state.bg = RGB(attr->br, attr->bg, attr->bb);
    state.fg = RGB(attr->fr, attr->fg, attr->fb);
    if (MemEqual(draw->cur_pos, pos) && // mouse is inversed in tsm, we undo that here
        !(draw->flags & TSM_SCREEN_HIDE_CURSOR))
        state.inverse = !attr->inverse;
    else
        state.inverse = attr->inverse;
    if (scpos.y != draw->pos.y || !MemEqual(state, draw->state))
    {
        Flush(draw);
        draw->pos = scpos;
        draw->state = state;
    }
    if (len > 0)
    {
        for (int i = 0; i < len; ++i)
        {
            uint32_t chr = ch[i];
#ifdef _UNICODE
            wchar_t buf[2];
            size_t buflen = ucs4_to_utf16(chr, buf);
            draw->drawbuf.append(buf, buflen);
#else
            char buf[4];
            size_t buflen = tsm_ucs4_to_utf8(chr, buf);
            draw->drawbuf.append(buf, buflen);
#endif
        }
    }
    else
        draw->drawbuf += ' ';
    return 0;
}

void tsm_vte_write(struct tsm_vte *vte,
    const char *u8,
    size_t len,
    void *data)
{
    const HANDLE hInput = (HANDLE) data;

    while (len > 0)
    {
        DWORD written = 0;
        WriteFile(hInput, u8, (DWORD) len, &written, nullptr);
        len -= written;
    }
    //FlushFileBuffers(hInput);
}

void tsm_vte_paste(struct tsm_vte *vte,
    LPCTSTR lptstr)
{
    const uint32_t keysym = XKB_KEY_NoSymbol;

    while (*lptstr != '\0')
    {
#ifdef _UNICODE
        uint32_t ascii = 0;
        uint32_t unicode = *lptstr;
#else
        uint32_t ascii = *lptstr;
        uint32_t unicode = 0;
#endif
        unsigned int mods = 0; // TODO Should capital letters be faked with a Shift?
        tsm_vte_handle_keyboard(vte, keysym, ascii, mods, unicode);
        ++lptstr;
    }
}

void tsm_vte_osc(struct tsm_vte *vte,
    const char *u8,
    size_t len,
    void *data)
{
    if (strncmp(u8, "0;", 2) == 0 || strncmp(u8, "2;", 2) == 0)
    {
        HWND hWnd = (HWND) data;
        SetWindowTextA(hWnd, u8 + 2);
    }
}

struct RadTerminalData
{
    struct tsm_screen *screen;
    struct tsm_vte *vte;
    tsm_screen_draw_info draw_info;
    SubProcessData spd;
};

void DrawCursor(HDC hdc, const RadTerminalData* const data)
{
    const unsigned int flags = tsm_screen_get_flags(data->screen);
    if (!(flags & TSM_SCREEN_HIDE_CURSOR))
    {
        const COORD cur_pos = { (SHORT) tsm_screen_get_cursor_x(data->screen), (SHORT) (tsm_screen_get_cursor_y(data->screen) + tsm_screen_sb_depth(data->screen)) };
        RECT rc = Rect(GetScreenPos(&data->draw_info, cur_pos), GetCellSize(&data->draw_info));
        // TODO Different cursor styles
        rc.top += (rc.bottom - rc.top) * 8 / 10;
        InvertRect(hdc, &rc);
    }
}

void FixScrollbar(HWND hWnd)
{
    const RadTerminalData* const data = (RadTerminalData*) GetWindowLongPtr(hWnd, GWLP_USERDATA);
    const unsigned int flags = tsm_screen_get_flags(data->screen);

    SCROLLINFO si = {};
    si.cbSize = sizeof(si);
    si.fMask = SIF_PAGE | SIF_RANGE | SIF_POS | SIF_DISABLENOSCROLL;
    si.nPage = tsm_screen_get_height(data->screen);
    if (!(flags & TSM_SCREEN_ALTERNATE))
    {
        si.nMax = si.nPage + tsm_screen_sb_count(data->screen) - 1;
        si.nPos = si.nMax - si.nPage - tsm_screen_sb_depth(data->screen) + 1;
    }
    else
    {
        si.nMax = si.nPage - 1;
        si.nPos = si.nMax - si.nPage + 1;
    }
    SetScrollInfo(hWnd, SB_VERT, &si, TRUE);
}

BOOL CheckScrollBar(HWND hWnd)
{
    const RadTerminalData* const data = (RadTerminalData*) GetWindowLongPtr(hWnd, GWLP_USERDATA);
    int nPage = tsm_screen_get_height(data->screen);
    int nMax = nPage + tsm_screen_sb_count(data->screen) - 1;
    int nPos = nMax - nPage - tsm_screen_sb_depth(data->screen) + 1;
    return GetScrollPos(hWnd, SB_VERT) == nPos;
}

// trim empty space from end of lines
void TrimLines(char* buf, int* plen)
{
    char* lastnonnull = nullptr;
    for (int i = 0; i < *plen; ++i)
    {
        switch (buf[i])
        {
        case ' ':
        case '\0':
            //buf[i] = ' ';
            break;

        case '\n':
            if (lastnonnull != nullptr)
            {
                strncpy_s(lastnonnull, *plen - (lastnonnull - buf), buf + i, *plen - i);  // TODO len should be *plen - (lastnonnull - buf) I think
                int cut = (int) ((buf + i) - lastnonnull);
                *plen -= cut;
                i -= cut;
            }
            // fallthrough

        default:
            lastnonnull = buf + i + 1;
            break;
        }
    }

    if (lastnonnull != nullptr)
    {
        *lastnonnull = '\0';
        int cut = (int) ((buf + *plen) - lastnonnull);
        *plen -= cut;
    }
}

int ActionCopyToClipboard(HWND hWnd)
{
    const RadTerminalData* const data = (RadTerminalData*) GetWindowLongPtr(hWnd, GWLP_USERDATA);
    char* buf = nullptr;
    int len = tsm_screen_selection_copy(data->screen, &buf);
    if (len > 0)
    {
        TrimLines(buf, &len);

        HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, len + 1);
        memcpy(GlobalLock(hMem), buf, len + 1);
        GlobalUnlock(hMem);
        while (!OpenClipboard(hWnd))
            ;
        CHECK_ONLY(EmptyClipboard());
        CHECK_ONLY(SetClipboardData(CF_TEXT, hMem));
        CHECK_ONLY(CloseClipboard());

        tsm_screen_selection_reset(data->screen);
        InvalidateRect(hWnd, nullptr, TRUE);
    }
    free(buf);
    return len;
}

int ActionClearSelection(HWND hWnd)
{
    const RadTerminalData* const data = (RadTerminalData*) GetWindowLongPtr(hWnd, GWLP_USERDATA);
    char* buf = nullptr;
    int len = tsm_screen_selection_copy(data->screen, &buf);
    if (len > 0)
    {
        tsm_screen_selection_reset(data->screen);
        InvalidateRect(hWnd, nullptr, TRUE);
    }
    free(buf);
    return len;
}

int ActionPasteFromClipboard(HWND hWnd)
{
    const RadTerminalData* const data = (RadTerminalData*) GetWindowLongPtr(hWnd, GWLP_USERDATA);
    if (IsClipboardFormatAvailable(CF_TEXT))
    {
        while (!OpenClipboard(hWnd))
            ;
#ifdef _UNICODE
        HANDLE hData = GetClipboardData(CF_UNICODETEXT);
#else
        HANDLE hData = GetClipboardData(CF_TEXT);
#endif
        if (hData != NULL)
        {
            LPCTSTR lptstr = (LPCTSTR) GlobalLock(hData);
            tsm_vte_paste(data->vte, lptstr);
            GlobalUnlock(hData);
        }
        CHECK_ONLY(CloseClipboard());
    }
    return 0;
}

int ActionScrollbackUp(HWND hWnd)
{
    const RadTerminalData* const data = (RadTerminalData*) GetWindowLongPtr(hWnd, GWLP_USERDATA);
    const unsigned int flags = tsm_screen_get_flags(data->screen);
    if (!(flags & TSM_SCREEN_ALTERNATE))
    {
        tsm_screen_sb_up(data->screen, 1);
        int sp = GetScrollPos(hWnd, SB_VERT);
        sp -= 1;
        SetScrollPos(hWnd, SB_VERT, sp, TRUE);
        VERIFY(CheckScrollBar(hWnd));
        InvalidateRect(hWnd, nullptr, TRUE);
    }
    return 0;
}

int ActionScrollbackDown(HWND hWnd)
{
    const RadTerminalData* const data = (RadTerminalData*) GetWindowLongPtr(hWnd, GWLP_USERDATA);
    const unsigned int flags = tsm_screen_get_flags(data->screen);
    if (!(flags & TSM_SCREEN_ALTERNATE))
    {
        tsm_screen_sb_down(data->screen, 1);
        int sp = GetScrollPos(hWnd, SB_VERT);
        sp += 1;
        SetScrollPos(hWnd, SB_VERT, sp, TRUE);
        VERIFY(CheckScrollBar(hWnd));
        InvalidateRect(hWnd, nullptr, TRUE);
    }
    return 0;
}

HWND ActionNewWindow(HWND hWnd, bool bParseCmdLine, const std::tstring& profile)
{
    const HINSTANCE hInstance = GetWindowInstance(hWnd);
    const HWND hWndMDIClient = GetMDIClient(hWnd);
    BOOL bMaximized = FALSE;
    GetMDIActive(hWndMDIClient, &bMaximized);

    const RadTerminalCreate rtc = GetTerminalCreate(bParseCmdLine, profile);

    HWND hChildWnd = CreateMDIWindow(
        MAKEINTATOM(GetRadTerminalAtom(hInstance)),
        PROJ_NAME,
        (bMaximized ? WS_MAXIMIZE : 0) | WS_OVERLAPPEDWINDOW | WS_VSCROLL,
        CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
        hWndMDIClient,       // Parent window
        hInstance,
        (LPARAM) &rtc
    );
    CHECK(hChildWnd != NULL, NULL);

    SetWindowLong(hChildWnd, GWL_EXSTYLE, GetWindowExStyle(hChildWnd) | WS_EX_ACCEPTFILES);
    if (false && g_darkModeEnabled) // TODO Doesn't seem to work MDI child windows
    {
        SetWindowTheme(hWnd, L"DarkMode_Explorer", NULL);   // Needed for scrollbar
        AllowDarkModeForWindow(hWnd, true);
        RefreshTitleBarThemeColor(hWnd);
    }

    return hChildWnd;
}

inline LRESULT MyDefWindowProc(_In_ HWND hWnd, _In_ UINT Msg, _In_ WPARAM wParam, _In_ LPARAM lParam)
{
    if (IsMDIChild(hWnd))
        return DefMDIChildProc(hWnd, Msg, wParam, lParam);
    else
        return DefWindowProc(hWnd, Msg, wParam, lParam);
}

BOOL RadTerminalWindowOnCreate(HWND hWnd, LPCREATESTRUCT lpCreateStruct)
{
    FORWARD_WM_CREATE(hWnd, lpCreateStruct, MyDefWindowProc);

    MDICREATESTRUCT* mdics = (MDICREATESTRUCT*) lpCreateStruct->lpCreateParams;
    const RadTerminalCreate* const rtc = IsMDIChild(hWnd) ? (RadTerminalCreate*) mdics->lParam : (RadTerminalCreate*) lpCreateStruct->lpCreateParams;

    RadTerminalData* const data = new RadTerminalData;
    ZeroMemory(data, sizeof(RadTerminalData));
    SetWindowLongPtr(hWnd, GWLP_USERDATA, (LONG_PTR) data);

    data->spd = CreateSubProcess(rtc->strCommand.c_str(), rtc->szCon, true);
    if (data->spd.hr != S_OK)
    {
        ShowError(hWnd, _T("CreateSubProcess"), data->spd.hr);
        return FALSE;
    }

    CreateThread(WatchThread, data->spd.pi.hProcess, hWnd);
    CreateThread(ReadThread, data->spd.hOutput, hWnd);
    data->spd.hOutput = NULL;

    // TODO Report error
    int e = 0;
    e = tsm_screen_new(&data->screen, tsm_log, nullptr);
    e = tsm_screen_resize(data->screen, rtc->szCon.X, rtc->szCon.Y);
    if (rtc->sb > 0)
        tsm_screen_set_max_sb(data->screen, rtc->sb);
    e = tsm_vte_new(&data->vte, data->screen, tsm_vte_write, data->spd.hInput, tsm_log, nullptr);
    tsm_vte_set_osc_cb(data->vte, tsm_vte_osc, hWnd);
    if (!rtc->strScheme.empty())
    {
#ifdef _UNICODE
        char scheme[1024];
        WideCharToMultiByte(CP_UTF8, 0, rtc->strScheme.c_str(), -1, scheme, ARRAYSIZE(scheme), nullptr, nullptr);
        e = tsm_vte_set_palette(data->vte, scheme);
#else
        e = tsm_vte_set_palette(data->vte, rtc->strScheme.c_str());
#endif
    }

    for (int b = 0; b < 2; ++b)
        for (int i = 0; i < 2; ++i)
            for (int u = 0; u < 2; ++u)
                CHECK(data->draw_info.hFonts[b][i][u] = CreateFont(rtc->strFontFace.c_str(), rtc->iFontHeight, b == 0 ? FW_NORMAL : FW_BOLD, i, u), FALSE);

    HDC hdc = GetDC(hWnd);
    SelectFont(hdc, data->draw_info.hFonts[0][0][0]);
    GetTextMetrics(hdc, &data->draw_info.tm);
    VERIFY(!(data->draw_info.tm.tmPitchAndFamily & TMPF_FIXED_PITCH));
    ReleaseDC(hWnd, hdc);

    RECT r = Rect({ 0, 0 }, GetScreenPos(&data->draw_info, rtc->szCon));
    const DWORD style = GetWindowStyle(hWnd);
    const DWORD exstyle = GetWindowExStyle(hWnd);
    if (style & WS_VSCROLL)
        r.right += GetSystemMetrics(SM_CXVSCROLL);
    CHECK(AdjustWindowRectEx(&r, style, GetMenu(hWnd) != NULL, exstyle), FALSE);
    CHECK(SetWindowPos(hWnd, 0, r.left, r.top, r.right - r.left, r.bottom - r.top, SWP_NOMOVE | SWP_NOZORDER), FALSE);

    HICON hIconLarge = NULL, hIconSmall = NULL;
    UINT count = GetIcon(&data->spd, &hIconLarge, &hIconSmall);
    if (count > 0)
    {
        SendMessage(hWnd, WM_SETICON, ICON_BIG, (LPARAM) hIconLarge);
        SendMessage(hWnd, WM_SETICON, ICON_SMALL, (LPARAM) hIconSmall);
    }

    CHECK(SetTimer(hWnd, 2, 500, nullptr), FALSE);

    return TRUE;
}

void RadTerminalWindowOnDestroy(HWND hWnd)
{
    FORWARD_WM_DESTROY(hWnd, MyDefWindowProc);
    const RadTerminalData* const data = (RadTerminalData*) GetWindowLongPtr(hWnd, GWLP_USERDATA);

    CleanupSubProcess(&data->spd);

    for (int b = 0; b < 2; ++b)
        for (int i = 0; i < 2; ++i)
            for (int u = 0; u < 2; ++u)
                DeleteFont(data->draw_info.hFonts[b][i][u]);

    tsm_vte_unref(data->vte);
    tsm_screen_unref(data->screen);

    if (!IsMDIChild(hWnd) || CountChildWindows(GetParent(hWnd)) == 1)
        PostQuitMessage(0);

    delete data;
    SetWindowLongPtr(hWnd, GWLP_USERDATA, (LONG_PTR) nullptr);
}

void RadTerminalWindowOnPaint(HWND hWnd)
{
    const RadTerminalData* const data = (RadTerminalData*) GetWindowLongPtr(hWnd, GWLP_USERDATA);
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(hWnd, &ps);

    HBRUSH hBrush = (HBRUSH) GetClassLongPtr(hWnd, GCLP_HBRBACKGROUND);
    if (hBrush != NULL)
        FillRect(hdc, &ps.rcPaint, hBrush);

    tsm_screen_draw_data draw = {};
    draw.hdc = hdc;
    draw.info = &data->draw_info;
    draw.pos.y = -1;
    draw.cur_pos = { (SHORT) tsm_screen_get_cursor_x(data->screen), (SHORT) (tsm_screen_get_cursor_y(data->screen) + tsm_screen_sb_depth(data->screen)) };
    draw.flags = tsm_screen_get_flags(data->screen);
    tsm_age_t age = tsm_screen_draw(data->screen, tsm_screen_draw, (void*) &draw);
    Flush(&draw);

    HWND hActive = MyGetActiveWnd(hWnd);
    if (hActive == hWnd)
        DrawCursor(hdc, data);

    EndPaint(hWnd, &ps);
}

void RadTerminalWindowSendKey(HWND hWnd, UINT vk, UINT scan, bool extended)
{
    const RadTerminalData* const data = (RadTerminalData*) GetWindowLongPtr(hWnd, GWLP_USERDATA);
    BYTE KeyState[256];
    GetKeyboardState(KeyState);

    uint32_t keysym = XKB_KEY_NoSymbol;
    uint32_t ascii = TSM_VTE_INVALID;
    uint32_t unicode = TSM_VTE_INVALID;

    unsigned int mods = 0;
    if (KeyState[VK_SHIFT] & 0x80)      mods |= TSM_SHIFT_MASK;
    if (KeyState[VK_SCROLL] & 0x80)     mods |= TSM_LOCK_MASK;
    if (KeyState[VK_CONTROL] & 0x80)    mods |= TSM_CONTROL_MASK;
    if (KeyState[VK_MENU] & 0x80)       mods |= TSM_ALT_MASK;
    if (KeyState[VK_LWIN] & 0x80)       mods |= TSM_LOGO_MASK;

    WORD charsAscii[2] = {};
    if (ToAscii(vk, scan, KeyState, charsAscii, 0) > 0)
    {
        ascii = charsAscii[0];
        keysym = ascii;
    }
    WCHAR charsUnicode[4] = {};
    if (ToUnicode(vk, scan, KeyState, charsUnicode, ARRAYSIZE(charsUnicode), 0) > 0)
    {
        unicode = charsUnicode[0];
        keysym = ascii;
    }

    switch (vk)
    {
    case VK_BACK: keysym = XKB_KEY_BackSpace; break;
    case VK_TAB: keysym = XKB_KEY_Tab; break;
    //case VK_: keysym = XKB_KEY_Linefeed; break;
    case VK_CLEAR: keysym = XKB_KEY_Clear; break;
    case VK_RETURN: keysym = XKB_KEY_Return; break;
    case VK_PAUSE: keysym = XKB_KEY_Pause; break;
    case VK_SCROLL: keysym = XKB_KEY_Scroll_Lock; break;
    //case VK_: keysym = XKB_KEY_Sys_Req; break;
    case VK_ESCAPE: keysym = XKB_KEY_Escape; break;
    case VK_DELETE: keysym = XKB_KEY_Delete; break;

    case VK_SHIFT: keysym = extended ? XKB_KEY_Shift_R : XKB_KEY_Shift_L; break;
    case VK_CONTROL: keysym = extended ? XKB_KEY_Control_R : XKB_KEY_Control_L; break;
    //case VK_: keysym = XKB_KEY_Caps_Lock; break;
    //case VK_: keysym = XKB_KEY_Shift_Lock; break;

    //case VK_: keysym = extended ? XKB_KEY_Meta_R : XKB_KEY_Meta_L; break;
    case VK_MENU: keysym = extended ? XKB_KEY_Alt_R : XKB_KEY_Alt_L; break;
    //case VK_: keysym = extended ? XKB_KEY_Super_R : XKB_KEY_Super_L; break;
    //case VK_: keysym = extended ? XKB_KEY_Hyper_R : XKB_KEY_Hyper_L; break;

    case VK_HOME: keysym = XKB_KEY_Home; break;
    case VK_LEFT: keysym = XKB_KEY_Left; break;
    case VK_UP: keysym = XKB_KEY_Up; break;
    case VK_RIGHT: keysym = XKB_KEY_Right; break;
    case VK_DOWN: keysym = XKB_KEY_Down; break;
    //case VK_PRIOR: keysym = XKB_KEY_Prior; break;
    case VK_PRIOR: keysym = XKB_KEY_Page_Up; break;
    //case VK_NEXT: keysym = XKB_KEY_Next; break;
    case VK_NEXT: keysym = XKB_KEY_Page_Down; break;
    case VK_END: keysym = XKB_KEY_End; break;
    //case VK_: keysym = XKB_KEY_Begin; break;

    case VK_SELECT: keysym = XKB_KEY_Select; break;
    case VK_PRINT: keysym = XKB_KEY_Print; break;
    case VK_EXECUTE: keysym = XKB_KEY_Execute; break;
    case VK_INSERT: keysym = XKB_KEY_Insert; break;
    //case VK_: keysym = XKB_KEY_Undo; break;
    //case VK_: keysym = XKB_KEY_Redo; break;
    //case VK_: keysym = XKB_KEY_Menu; break;
    //case VK_: keysym = XKB_KEY_Find; break;
    case VK_CANCEL: keysym = XKB_KEY_Cancel; break;
    case VK_HELP: keysym = XKB_KEY_Help; break;
    //case VK_: keysym = XKB_KEY_Break; break;
    //case VK_: keysym = XKB_KEY_Mode_switch; break;
    //case VK_: keysym = XKB_KEY_script_switch; break;
    case VK_NUMLOCK: keysym = XKB_KEY_Num_Lock; break;

    //case VK_: keysym = XKB_KEY_KP_Space; break;
    //case VK_: keysym = XKB_KEY_KP_Tab; break;
    //case VK_: keysym = XKB_KEY_KP_Enter; break;
    //case VK_: keysym = XKB_KEY_KP_F1; break;
    //case VK_: keysym = XKB_KEY_KP_F2; break;
    //case VK_: keysym = XKB_KEY_KP_F3; break;
    //case VK_: keysym = XKB_KEY_KP_F4; break;
    //case VK_: keysym = XKB_KEY_KP_Home; break;
    //case VK_: keysym = XKB_KEY_KP_Left; break;
    //case VK_: keysym = XKB_KEY_KP_Up; break;
    //case VK_: keysym = XKB_KEY_KP_Right; break;
    //case VK_: keysym = XKB_KEY_KP_Down; break;
    //case VK_: keysym = XKB_KEY_KP_Prior; break;
    //case VK_: keysym = XKB_KEY_KP_Page_Up; break;
    //case VK_: keysym = XKB_KEY_KP_Next; break;
    //case VK_: keysym = XKB_KEY_KP_Page_Down; break;
    //case VK_: keysym = XKB_KEY_KP_End                        0xff9c
    //case VK_: keysym = XKB_KEY_KP_Begin                      0xff9d
    //case VK_: keysym = XKB_KEY_KP_Insert                     0xff9e
    //case VK_: keysym = XKB_KEY_KP_Delete                     0xff9f
    //case VK_: keysym = XKB_KEY_KP_Equal                      0xffbd  /* Equals */
    //case VK_: keysym = XKB_KEY_KP_Multiply                   0xffaa
    //case VK_: keysym = XKB_KEY_KP_Add                        0xffab
    //case VK_: keysym = XKB_KEY_KP_Separator                  0xffac  /* Separator, often comma */
    //case VK_: keysym = XKB_KEY_KP_Subtract                   0xffad
    //case VK_: keysym = XKB_KEY_KP_Decimal                    0xffae
    //case VK_: keysym = XKB_KEY_KP_Divide                     0xffaf

    case VK_NUMPAD0: keysym = XKB_KEY_KP_0; break;
    case VK_NUMPAD1: keysym = XKB_KEY_KP_1; break;
    case VK_NUMPAD2: keysym = XKB_KEY_KP_2; break;
    case VK_NUMPAD3: keysym = XKB_KEY_KP_3; break;
    case VK_NUMPAD4: keysym = XKB_KEY_KP_4; break;
    case VK_NUMPAD5: keysym = XKB_KEY_KP_5; break;
    case VK_NUMPAD6: keysym = XKB_KEY_KP_6; break;
    case VK_NUMPAD7: keysym = XKB_KEY_KP_7; break;
    case VK_NUMPAD8: keysym = XKB_KEY_KP_8; break;
    case VK_NUMPAD9: keysym = XKB_KEY_KP_9; break;

    case VK_F1: keysym = XKB_KEY_F1; break;
    case VK_F2: keysym = XKB_KEY_F2; break;
    case VK_F3: keysym = XKB_KEY_F3; break;
    case VK_F4: keysym = XKB_KEY_F4; break;
    case VK_F5: keysym = XKB_KEY_F5; break;
    case VK_F6: keysym = XKB_KEY_F6; break;
    case VK_F7: keysym = XKB_KEY_F7; break;
    case VK_F8: keysym = XKB_KEY_F8; break;
    case VK_F9: keysym = XKB_KEY_F9; break;
    case VK_F10: keysym = XKB_KEY_F10; break;
    case VK_F11: keysym = XKB_KEY_F11; break;
    //case VK_: keysym = XKB_KEY_L1; break;
    case VK_F12: keysym = XKB_KEY_F12; break;
    //case VK_: keysym = XKB_KEY_L2; break;
    case VK_F13: keysym = XKB_KEY_F13; break;
    //case VK_: keysym = XKB_KEY_L3; break;
    case VK_F14: keysym = XKB_KEY_F14; break;
    //case VK_: keysym = XKB_KEY_L4; break;
    case VK_F15: keysym = XKB_KEY_F15; break;
    //case VK_: keysym = XKB_KEY_L5; break;
    case VK_F16: keysym = XKB_KEY_F16; break;
    //case VK_: keysym = XKB_KEY_L6; break;
    case VK_F17: keysym = XKB_KEY_F17; break;
    //case VK_: keysym = XKB_KEY_L7; break;
    case VK_F18: keysym = XKB_KEY_F18; break;
    //case VK_: keysym = XKB_KEY_L8; break;
    case VK_F19: keysym = XKB_KEY_F19; break;
    //case VK_: keysym = XKB_KEY_L9; break;
    case VK_F20: keysym = XKB_KEY_F20; break;
    //case VK_: keysym = XKB_KEY_L10; break;
    case VK_F21: keysym = XKB_KEY_F21; break;
    //case VK_: keysym = XKB_KEY_R1; break;
    case VK_F22: keysym = XKB_KEY_F22; break;
    //case VK_: keysym = XKB_KEY_R2; break;
    case VK_F23: keysym = XKB_KEY_F23; break;
    //case VK_: keysym = XKB_KEY_R3; break;
    case VK_F24: keysym = XKB_KEY_F24; break;
    //case VK_: keysym = XKB_KEY_R4; break;
    //case VK_: keysym = XKB_KEY_F25; break;
    //case VK_: keysym = XKB_KEY_R5; break;
    //case VK_: keysym = XKB_KEY_F26; break;
    //case VK_: keysym = XKB_KEY_R6; break;
    //case VK_: keysym = XKB_KEY_F27; break;
    //case VK_: keysym = XKB_KEY_R7; break;
    //case VK_: keysym = XKB_KEY_F28; break;
    //case VK_: keysym = XKB_KEY_R8; break;
    //case VK_: keysym = XKB_KEY_F29; break;
    //case VK_: keysym = XKB_KEY_R9; break;
    //case VK_: keysym = XKB_KEY_F30; break;
    //case VK_: keysym = XKB_KEY_R10; break;
    //case VK_: keysym = XKB_KEY_F31; break;
    //case VK_: keysym = XKB_KEY_R11; break;
    //case VK_: keysym = XKB_KEY_F32; break;
    //case VK_: keysym = XKB_KEY_R12; break;
    //case VK_: keysym = XKB_KEY_F33; break;
    //case VK_: keysym = XKB_KEY_R13; break;
    //case VK_: keysym = XKB_KEY_F34; break;
    //case VK_: keysym = XKB_KEY_R14; break;
    //case VK_: keysym = XKB_KEY_F35; break;
    //case VK_: keysym = XKB_KEY_R15; break;
    }

    tsm_screen_selection_reset(data->screen);
    if (vk != VK_SHIFT && vk != VK_CONTROL && vk != VK_MENU)
        tsm_screen_sb_reset(data->screen);
    if (keysym != XKB_KEY_NoSymbol)
        tsm_vte_handle_keyboard(data->vte, keysym, ascii, mods, unicode);
    InvalidateRect(hWnd, nullptr, TRUE);
}

void RadTerminalWindowOnKeyDown(HWND hWnd, UINT vk, BOOL fDown, int cRepeat, UINT flags)
{
    FORWARD_WM_KEYDOWN(hWnd, vk, cRepeat, flags, MyDefWindowProc);

    BYTE KeyState[256];
    GetKeyboardState(KeyState);

    bool bPassOn = true;
    switch (vk)
    {
    case VK_ESCAPE: bPassOn = ActionClearSelection(hWnd) < 0; break;
    case VK_RETURN: bPassOn = ActionCopyToClipboard(hWnd) < 0; break;
    }

    if (bPassOn)
        RadTerminalWindowSendKey(hWnd, vk, flags & 0xFF, (flags & 0x100) != 0);
}

void RadTerminalWindowOnSysKeyDown(HWND hWnd, UINT vk, BOOL fDown, int cRepeat, UINT flags)
{
    FORWARD_WM_SYSKEYDOWN(hWnd, vk, cRepeat, flags, MyDefWindowProc);
    RadTerminalWindowSendKey(hWnd, vk, flags & 0xFF, (flags & 0x100) != 0);
}

int RadTerminalWindowOnMouseActivate(HWND hWnd, HWND hwndTopLevel, UINT codeHitTest, UINT msg)
{
    int result = FORWARD_WM_MOUSEACTIVATE(hWnd, hwndTopLevel, codeHitTest, msg, MyDefWindowProc);
    static HWND s_hWnd = NULL;  // MDI Windows get a WM_MOUSE_ACTIVATE on every mouse click, not just the first to make it active
    if (s_hWnd != hWnd && result == MA_ACTIVATE && codeHitTest == HTCLIENT)
    {
        s_hWnd = hWnd;
        return MA_ACTIVATEANDEAT;
    }
    return result;
}

void RadTerminalWindowOnMouseMove(HWND hWnd, int x, int y, UINT keyFlags)
{
    FORWARD_WM_MOUSEMOVE(hWnd, x, y, keyFlags, MyDefWindowProc);
    const RadTerminalData* const data = (RadTerminalData*) GetWindowLongPtr(hWnd, GWLP_USERDATA);
    if (GetCapture() == hWnd)
    {
        const COORD pos = GetCellPos(&data->draw_info, { x, y });
        tsm_screen_selection_target(data->screen, pos.X, pos.Y);
        InvalidateRect(hWnd, nullptr, TRUE);
    }
}

void RadTerminalWindowOnLButtonDown(HWND hWnd, BOOL fDoubleClick, int x, int y, UINT keyFlags)
{
    FORWARD_WM_LBUTTONDOWN(hWnd, fDoubleClick, x, y, keyFlags, MyDefWindowProc);
    const RadTerminalData* const data = (RadTerminalData*) GetWindowLongPtr(hWnd, GWLP_USERDATA);
    if (MyGetActiveWnd(hWnd) == hWnd)
    {
        // TODO Handle fDoubleClick to select word
        SetCapture(hWnd);
        COORD pos = GetCellPos(&data->draw_info, { x, y });
        tsm_screen_selection_start(data->screen, pos.X, pos.Y);
        InvalidateRect(hWnd, nullptr, TRUE);
    }
}

void RadTerminalWindowOnLButtonUp(HWND hWnd, int x, int y, UINT keyFlags)
{
    FORWARD_WM_LBUTTONUP(hWnd, x, y, keyFlags, MyDefWindowProc);
    const RadTerminalData* const data = (RadTerminalData*) GetWindowLongPtr(hWnd, GWLP_USERDATA);
    if (GetCapture() == hWnd)
        ReleaseCapture();
}

void RadTerminalWindowOnRButtonDown(HWND hWnd, BOOL fDoubleClick, int x, int y, UINT keyFlags)
{
    FORWARD_WM_RBUTTONDOWN(hWnd, fDoubleClick, x, y, keyFlags, MyDefWindowProc);
    if (ActionCopyToClipboard(hWnd) < 0)
        ActionPasteFromClipboard(hWnd);
}

void RadTerminalWindowOnTimer(HWND hWnd, UINT id)
{
    const RadTerminalData* const data = (RadTerminalData*) GetWindowLongPtr(hWnd, GWLP_USERDATA);
    switch (id)
    {
    case 2:
        {
            HWND hActive = MyGetActiveWnd(hWnd);
            if (hActive == hWnd)
            {
                HDC hdc = GetDC(hWnd);
                DrawCursor(hdc, data);
                ReleaseDC(hWnd, hdc);
            }
        }
        break;
    }
}

void RadTerminalWindowOnSize(HWND hWnd, UINT state, int cx, int cy)
{
    if (state == SIZE_RESTORED || state == SIZE_MAXIMIZED)
    {
        const RadTerminalData* const data = (RadTerminalData*) GetWindowLongPtr(hWnd, GWLP_USERDATA);
        SIZE sz = GetCellSize(&data->draw_info);
        COORD size = { (SHORT) (cx / sz.cx), (SHORT) (cy / sz.cy) };
        int e = tsm_screen_resize(data->screen, size.X, size.Y);
        ResizePseudoConsole(data->spd.hPC, size);

        FixScrollbar(hWnd);
    }
    FORWARD_WM_SIZE(hWnd, state, cx, cy, MyDefWindowProc);
}

void RadTerminalWindowOnSizing(HWND hWnd, UINT edge, LPRECT prRect)
{
    FORWARD_WM_SIZING(hWnd, edge, prRect, MyDefWindowProc);
    const RadTerminalData* const data = (RadTerminalData*) GetWindowLongPtr(hWnd, GWLP_USERDATA);
    const DWORD style = GetWindowStyle(hWnd);
    const DWORD exstyle = GetWindowExStyle(hWnd);
    const BOOL fMenu = GetMenu(hWnd) != NULL;
    CHECK_ONLY(UnadjustWindowRectEx(prRect, style, fMenu, exstyle));
    if (style & WS_VSCROLL)
        prRect->right -= GetSystemMetrics(SM_CXVSCROLL);
    SIZE sz = GetCellSize(&data->draw_info);
    COORD size = { (SHORT) ((prRect->right - prRect->left) / sz.cx), (SHORT) ((prRect->bottom - prRect->top) / sz.cy) };

    switch (edge)
    {
    case WMSZ_LEFT: case WMSZ_TOPLEFT: case WMSZ_BOTTOMLEFT:
        prRect->left = prRect->right - size.X * sz.cx;
        break;

    case WMSZ_RIGHT: case WMSZ_TOPRIGHT: case WMSZ_BOTTOMRIGHT:
        prRect->right = prRect->left + size.X * sz.cx;
        break;
    }

    switch (edge)
    {
    case WMSZ_TOP: case WMSZ_TOPLEFT: case WMSZ_TOPRIGHT:
        prRect->top = prRect->bottom - size.Y * sz.cy;
        break;

    case WMSZ_BOTTOM: case WMSZ_BOTTOMLEFT: case WMSZ_BOTTOMRIGHT:
        prRect->bottom = prRect->top + size.Y * sz.cy;
        break;
    }

    if (style & WS_VSCROLL)
        prRect->right += GetSystemMetrics(SM_CXVSCROLL);
    CHECK_ONLY(AdjustWindowRectEx(prRect, style, fMenu, exstyle));
    //prRect->right += 100;
}

void RadTerminalWindowOnVScroll(HWND hWnd, HWND hWndCtl, UINT code, int pos)
{
    FORWARD_WM_VSCROLL(hWnd, hWndCtl, code, pos, MyDefWindowProc);
    const RadTerminalData* const data = (RadTerminalData*) GetWindowLongPtr(hWnd, GWLP_USERDATA);
    const int op = GetScrollPos(hWnd, SB_VERT);
    int d = 0;
    switch (code)
    {
    case SB_LINEUP:
        d -= 1;
        break;

    case SB_LINEDOWN:
        d += 1;
        break;

    case SB_PAGEUP:
        d -= tsm_screen_get_height(data->screen) - 1;
        break;

    case SB_PAGEDOWN:
        d += tsm_screen_get_height(data->screen) - 1;
        break;

    case SB_THUMBTRACK:
        d = pos - (tsm_screen_sb_count(data->screen) - tsm_screen_sb_depth(data->screen));
        break;
    }

    if (d != 0)
    {
        if (d < 0)
            tsm_screen_sb_up(data->screen, -d);
        else
            tsm_screen_sb_down(data->screen, d);
        InvalidateRect(hWnd, nullptr, TRUE);
        SetScrollPos(hWnd, SB_VERT, op + d, TRUE);
        VERIFY(CheckScrollBar(hWnd));
    }
}

void RadTerminalWindowOnDropFiles(HWND hWnd, HDROP hDrop)
{
    const RadTerminalData* const data = (RadTerminalData*) GetWindowLongPtr(hWnd, GWLP_USERDATA);
    const int count = DragQueryFile(hDrop, 0xFFFFFFFF, nullptr, 0);
    for (int i = 0; i < count; ++i)
    {
        TCHAR buf[MAX_PATH];
        DragQueryFile(hDrop, i, buf, ARRAYSIZE(buf));

        const uint32_t keysym = XKB_KEY_NoSymbol;

        if (i != 0)
            tsm_vte_paste(data->vte, _T(" "));
        tsm_vte_paste(data->vte, buf);
    }
    DragFinish(hDrop);
}

void RadTerminalWindowOnCommand(HWND hWnd, int id, HWND hWndCtl, UINT codeNotify)
{
    switch (id)
    {
    case ID_EDIT_COPY: ActionCopyToClipboard(hWnd);  break;
    case ID_EDIT_PASTE: ActionPasteFromClipboard(hWnd);  break;
    case ID_VIEW_SCROLL_UP: ActionScrollbackUp(hWnd); break;
    case ID_VIEW_SCROLL_DOWN: ActionScrollbackDown(hWnd); break;
    case ID_FILE_CLOSE: PostMessage(hWnd, WM_CLOSE, 0, 0);  break;
    default: FORWARD_WM_COMMAND(hWnd, id, hWndCtl, codeNotify, MyDefWindowProc); break;
    }
}

LRESULT RadTerminalWindowOnWatch(HWND hWnd, WPARAM wParam, LPARAM lParam)
{
    const RadTerminalData* const data = (RadTerminalData*) GetWindowLongPtr(hWnd, GWLP_USERDATA);
    HANDLE h = (HANDLE) wParam;
    if (h == data->spd.pi.hProcess)
    {
        PostMessage(hWnd, WM_CLOSE, 0, 0);
    }
    return 0;
}

LRESULT RadTerminalWindowOnRead(HWND hWnd, WPARAM wParam, LPARAM lParam)
{
    const RadTerminalData* const data = (RadTerminalData*) GetWindowLongPtr(hWnd, GWLP_USERDATA);
    const char* buf = (const char*) wParam;
    DWORD len = (DWORD) lParam;
    tsm_vte_input(data->vte, buf, len);
    FixScrollbar(hWnd);
    InvalidateRect(hWnd, nullptr, TRUE);
    return 0;
}

/* LRESULT Cls_StdMessage(HWND hWnd, WPARAM wParam, LPARAM lParam) */
#define HANDLE_STD_MSG(hwnd, message, fn)    \
    case (message): return fn((hwnd), (wParam), (lParam))

LRESULT CALLBACK RadTerminalWindowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    //return MyDefWindowProc(hWnd, uMsg, wParam, lParam);
    switch (uMsg)
    {
        HANDLE_MSG(hWnd, WM_CREATE, RadTerminalWindowOnCreate);
        HANDLE_MSG(hWnd, WM_DESTROY, RadTerminalWindowOnDestroy);
        HANDLE_MSG(hWnd, WM_PAINT, RadTerminalWindowOnPaint);
        HANDLE_MSG(hWnd, WM_KEYDOWN, RadTerminalWindowOnKeyDown);
        HANDLE_MSG(hWnd, WM_SYSKEYDOWN, RadTerminalWindowOnSysKeyDown);
        HANDLE_MSG(hWnd, WM_MOUSEACTIVATE, RadTerminalWindowOnMouseActivate);
        HANDLE_MSG(hWnd, WM_MOUSEMOVE, RadTerminalWindowOnMouseMove);
        HANDLE_MSG(hWnd, WM_LBUTTONDOWN, RadTerminalWindowOnLButtonDown);
        HANDLE_MSG(hWnd, WM_LBUTTONUP, RadTerminalWindowOnLButtonUp);
        HANDLE_MSG(hWnd, WM_RBUTTONDOWN, RadTerminalWindowOnRButtonDown);
        HANDLE_MSG(hWnd, WM_TIMER, RadTerminalWindowOnTimer);
        HANDLE_MSG(hWnd, WM_SIZE, RadTerminalWindowOnSize);
        HANDLE_MSG(hWnd, WM_SIZING, RadTerminalWindowOnSizing);
        HANDLE_MSG(hWnd, WM_VSCROLL, RadTerminalWindowOnVScroll);
        HANDLE_MSG(hWnd, WM_DROPFILES, RadTerminalWindowOnDropFiles);
        HANDLE_MSG(hWnd, WM_COMMAND, RadTerminalWindowOnCommand);
        HANDLE_STD_MSG(hWnd, WM_WATCH, RadTerminalWindowOnWatch);
        HANDLE_STD_MSG(hWnd, WM_READ, RadTerminalWindowOnRead);
    default: return MyDefWindowProc(hWnd, uMsg, wParam, lParam);
    }
}
