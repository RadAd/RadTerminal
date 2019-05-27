#include <windows.h>
#include <windowsx.h>
#include <tchar.h>
#include <string>
#include "ProcessUtils.h"
#include "WinUtils.h"
#include "libtsm\src\tsm\libtsm.h"
#include "libtsm\external\xkbcommon\xkbcommon-keysyms.h"

// TODO
// https://stackoverflow.com/questions/5966903/how-to-get-mousemove-and-mouseclick-in-bash
// default settings
// scrollbar
// remove polling
// unicode/emoji
// drop files
// status bar
// flash window on updates
// dynamically change font
// transparency
// check for monospace font

#ifdef _UNICODE
#define tstring wstring
#else
#define tstring string
#endif

#define PROJ_NAME TEXT("RadTerminal")

template <class T>
bool MemEqual(const T& a, const T& b)
{
    return memcmp(&a, &b, sizeof(T)) == 0;
}

void ShowError(HWND hWnd, LPCTSTR msg, HRESULT hr)
{
    TCHAR fullmsg[1024];
    _stprintf_s(fullmsg, _T("%s: 0x%x"), msg, hr);
    MessageBox(hWnd, fullmsg, PROJ_NAME, MB_ICONERROR);
}

#define CHECK(x, r) \
    if (!(x)) \
    { \
        ShowError(hWnd, _T(#x), HRESULT_FROM_WIN32(GetLastError())); \
        return (r); \
    }

#define CHECK_ONLY(x) \
    if (!(x)) \
    { \
        ShowError(hWnd, _T(#x), HRESULT_FROM_WIN32(GetLastError())); \
    }

LRESULT CALLBACK RadTerminalWindowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

struct RadTerminalCreate
{
    int iFontHeight;
    LPTSTR strFontFace;
    LPTSTR strScheme;
    COORD szCon;
    int sb;
    std::tstring strCmd;
};

int WINAPI _tWinMain(HINSTANCE hInstance, HINSTANCE, PTSTR pCmdLine, int nCmdShow)
{
    HWND hWnd = NULL;

    WNDCLASS wc = {};

    wc.lpfnWndProc = RadTerminalWindowProc;
    wc.hIcon = LoadIcon(NULL, IDI_APPLICATION);
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    //wc.hbrBackground = GetSysColorBrush(COLOR_WINDOW);
    wc.hInstance = hInstance;
    wc.lpszClassName = TEXT("RadTerminal");

    ATOM atom = NULL;
    CHECK(atom = RegisterClass(&wc), EXIT_FAILURE);

    RadTerminalCreate rtc = {};
    rtc.iFontHeight = 16;
    rtc.strFontFace = _T("Consolas");
    //rtc.strScheme = _T("solarized");
    rtc.szCon = { 80, 25 };
    rtc.sb = 1000;
    //rtc.strCmd = _T("%COMSPEC%");
    rtc.strCmd = _T("cmd");

    bool command = false;
    for (int i = 1; i < __argc; ++i)
    {
        LPCTSTR arg = __targv[i];
        if (command)
        {
            rtc.strCmd += ' ';
            rtc.strCmd += arg;
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
            rtc.strCmd = arg;
            command = true;
        }
    }

    HWND hChildWnd = CreateWindowEx(
        0,
        MAKEINTATOM(atom),
        PROJ_NAME,
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
        NULL,       // Parent window
        NULL,       // Menu
        hInstance,
        &rtc
    );
    CHECK(hChildWnd, EXIT_FAILURE);

    ShowWindow(hChildWnd, nCmdShow);

    MSG msg = {};
    while (GetMessage(&msg, NULL, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return EXIT_SUCCESS;
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
    sprintf_s(buf, "tsm_log: %s:%d %s %s %d\n", strrchr(file, '\\'), line, func, subs, sev);
    OutputDebugStringA(buf);
    OutputDebugStringA("tsm_log: ");
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

SIZE GetCellSize(const tsm_screen_draw_info* di)
{
    return { di->tm.tmAveCharWidth, di->tm.tmHeight };
}

POINT GetScreenPos(const tsm_screen_draw_info* di, COORD pos)
{
    SIZE sz = GetCellSize(di);
    return { pos.X * sz.cx, pos.Y * sz.cy };
}

COORD GetCellPos(const tsm_screen_draw_info* di, POINT pos)
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

bool tsm_vte_read(struct tsm_vte *vte, HANDLE hOutput)
{
    DWORD avail = 0;
    if (PeekNamedPipe(hOutput, nullptr, 0, nullptr, &avail, nullptr) && avail > 0)
    {
        while (avail > 0)
        {
            char buf[1024];
            const DWORD toread = (avail> ARRAYSIZE(buf)) ? ARRAYSIZE(buf) : avail;
            DWORD read = 0;
            ReadFile(hOutput, buf, toread, &read, nullptr);
            tsm_vte_input(vte, buf, read);
            avail -= read;
        }
        return true;
    }
    else
        return false;
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

int ActionCopyToClipboard(HWND hWnd)
{
    const RadTerminalData* const data = (RadTerminalData*) GetWindowLongPtr(hWnd, GWLP_USERDATA);
    char* buf = nullptr;
    int len = tsm_screen_selection_copy(data->screen, &buf);
    if (len > 0)
    {
        {   // trim empty space from end of lines
            char* lastnonnull = nullptr;
            for (int i = 0; i < len; ++i)
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
                        strncpy_s(lastnonnull, len - (lastnonnull - buf), buf + i, len - i);  // TODO len should be len - (lastnonnull - buf) I think
                        int cut = (int) ((buf + i) - lastnonnull);
                        len -= cut;
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
                int cut = (int) ((buf + len) - lastnonnull);
                len -= cut;
            }
        }

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
        HANDLE hData = GetClipboardData(CF_TEXT);
        if (hData != NULL)
        {
            LPCSTR lptstr = (LPCSTR) GlobalLock(hData);
            while (*lptstr != '\0')
            {
                uint32_t keysym = XKB_KEY_NoSymbol;
                uint32_t ascii = *lptstr;
                uint32_t unicode = ascii;
                unsigned int mods = 0; // TODO Should capital letters be faked with a Shift?
                tsm_vte_handle_keyboard(data->vte, keysym, ascii, mods, unicode);
                ++lptstr;
            }
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
        InvalidateRect(hWnd, nullptr, TRUE);
    }
    return 0;
}

BOOL RadTerminalWindowOnCreate(HWND hWnd, LPCREATESTRUCT lpCreateStruct)
{
    const RadTerminalCreate* const rtc = (RadTerminalCreate*) lpCreateStruct->lpCreateParams;

    RadTerminalData* const data = new RadTerminalData;
    ZeroMemory(data, sizeof(RadTerminalData));
    SetWindowLongPtr(hWnd, GWLP_USERDATA, (LONG_PTR) data);

    data->spd = CreateSubProcess(rtc->strCmd.c_str(), rtc->szCon, true);
    if (data->spd.hr != S_OK)
    {
        ShowError(hWnd, _T("CreateSubProcess"), data->spd.hr);
        return FALSE;
    }

    // TODO Report error
    int e = 0;
    e = tsm_screen_new(&data->screen, tsm_log, nullptr);
    e = tsm_screen_resize(data->screen, rtc->szCon.X, rtc->szCon.Y);
    if (rtc->sb > 0)
        tsm_screen_set_max_sb(data->screen, rtc->sb);
    e = tsm_vte_new(&data->vte, data->screen, tsm_vte_write, data->spd.hInput, tsm_log, nullptr);
    tsm_vte_set_osc_cb(data->vte, tsm_vte_osc, hWnd);
    if (rtc->strScheme != nullptr)
    {
        char scheme[1024];
        WideCharToMultiByte(CP_UTF8, 0, rtc->strScheme, -1, scheme, ARRAYSIZE(scheme), nullptr, nullptr);
        e = tsm_vte_set_palette(data->vte, scheme);
    }

    for (int b = 0; b < 2; ++b)
        for (int i = 0; i < 2; ++i)
            for (int u = 0; u < 2; ++u)
                CHECK(data->draw_info.hFonts[b][i][u] = CreateFont(rtc->strFontFace, rtc->iFontHeight, b == 0 ? FW_NORMAL : FW_BOLD, i, u), FALSE);

    HDC hdc = GetDC(hWnd);
    SelectFont(hdc, data->draw_info.hFonts[0][0][0]);
    GetTextMetrics(hdc, &data->draw_info.tm);
    ReleaseDC(hWnd, hdc);

    RECT r = Rect({ 0, 0 }, GetScreenPos(&data->draw_info, rtc->szCon));
    const DWORD style = GetWindowStyle(hWnd);
    CHECK(AdjustWindowRect(&r, style, FALSE), FALSE);
    CHECK(SetWindowPos(hWnd, 0, r.left, r.top, r.right - r.left, r.bottom - r.top, SWP_NOMOVE | SWP_NOZORDER), FALSE);

    HICON hIconLarge = NULL, hIconSmall = NULL;
    UINT count = GetIcon(&data->spd, &hIconLarge, &hIconSmall);
    if (count > 0)
    {
        SendMessage(hWnd, WM_SETICON, ICON_BIG, (LPARAM) hIconLarge);
        SendMessage(hWnd, WM_SETICON, ICON_SMALL, (LPARAM) hIconSmall);
    }

    CHECK(SetTimer(hWnd, 1, 10, nullptr), FALSE);
    CHECK(SetTimer(hWnd, 2, 500, nullptr), FALSE);

    return TRUE;
}

void RadTerminalWindowOnDestroy(HWND hWnd)
{
    const RadTerminalData* const data = (RadTerminalData*) GetWindowLongPtr(hWnd, GWLP_USERDATA);

    CleanupSubProcess(&data->spd);

    for (int b = 0; b < 2; ++b)
        for (int i = 0; i < 2; ++i)
            for (int u = 0; u < 2; ++u)
                DeleteFont(data->draw_info.hFonts[b][i][u]);

    tsm_vte_unref(data->vte);
    tsm_screen_unref(data->screen);

    delete data;
    SetWindowLongPtr(hWnd, GWLP_USERDATA, (LONG_PTR) nullptr);
    PostQuitMessage(0);
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

    HWND hActive = GetActiveWindow();
    if (hActive == hWnd)
        DrawCursor(hdc, data);

    EndPaint(hWnd, &ps);
}

void RadTerminalWindowOnKeyDown(HWND hWnd, UINT vk, BOOL fDown, int cRepeat, UINT flags)
{
    const RadTerminalData* const data = (RadTerminalData*) GetWindowLongPtr(hWnd, GWLP_USERDATA);
    //uint32_t ascii = MapVirtualKeyA(vk, MAPVK_VK_TO_CHAR);
    BYTE KeyState[256];
    GetKeyboardState(KeyState);

    bool bPassOn = true;
    switch (vk)
    {
    case VK_ESCAPE: bPassOn = ActionClearSelection(hWnd) < 0; break;
    case VK_RETURN: bPassOn = ActionCopyToClipboard(hWnd) < 0; break;
    case 'V': if (KeyState[VK_CONTROL] & 0x80) bPassOn = ActionPasteFromClipboard(hWnd) < 0; break;
    case VK_UP: if (KeyState[VK_CONTROL] & 0x80) bPassOn = ActionScrollbackUp(hWnd) < 0; break;
    case VK_DOWN: if (KeyState[VK_CONTROL] & 0x80) bPassOn = ActionScrollbackDown(hWnd) < 0; break;
    }

    if (bPassOn)
    {
        uint32_t keysym = XKB_KEY_NoSymbol;
        uint32_t ascii = 0;
        uint32_t unicode = ascii;

        unsigned int mods = 0;
        if (KeyState[VK_SHIFT] & 0x80)
            mods |= TSM_SHIFT_MASK;
        if (KeyState[VK_SCROLL] & 0x80)
            mods |= TSM_LOCK_MASK;
        if (KeyState[VK_CONTROL] & 0x80)
            mods |= TSM_CONTROL_MASK;
        if (KeyState[VK_MENU] & 0x80)
            mods |= TSM_ALT_MASK;
        if (KeyState[VK_LWIN] & 0x80)
            mods |= TSM_LOGO_MASK;

        UINT scan = (cRepeat >> 8);
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
        if (ascii != 0 || unicode != 0)
            tsm_screen_sb_reset(data->screen);
        bool b = tsm_vte_handle_keyboard(data->vte, keysym, ascii, mods, unicode);
        InvalidateRect(hWnd, nullptr, TRUE);
    }

    FORWARD_WM_KEYDOWN(hWnd, vk, cRepeat, flags, DefWindowProc);
}

int RadTerminalWindowOnMouseActivate(HWND hWnd, HWND hwndTopLevel, UINT codeHitTest, UINT msg)
{
    int result = FORWARD_WM_MOUSEACTIVATE(hWnd, hwndTopLevel, codeHitTest, msg, DefWindowProc);
    if (result == MA_ACTIVATE)
        return MA_ACTIVATEANDEAT;
    return result;
}

void RadTerminalWindowOnMouseMove(HWND hWnd, int x, int y, UINT keyFlags)
{
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
    const RadTerminalData* const data = (RadTerminalData*) GetWindowLongPtr(hWnd, GWLP_USERDATA);
    if (GetActiveWindow() == hWnd)
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
    const RadTerminalData* const data = (RadTerminalData*) GetWindowLongPtr(hWnd, GWLP_USERDATA);
    if (GetCapture() == hWnd)
        ReleaseCapture();
}

void RadTerminalWindowOnRButtonDown(HWND hWnd, BOOL fDoubleClick, int x, int y, UINT keyFlags)
{
    if (ActionCopyToClipboard(hWnd) < 0)
        ActionPasteFromClipboard(hWnd);
}

void RadTerminalWindowOnTimer(HWND hWnd, UINT id)
{
    const RadTerminalData* const data = (RadTerminalData*) GetWindowLongPtr(hWnd, GWLP_USERDATA);
    switch (id)
    {
    case 1:
        {
            if (tsm_vte_read(data->vte, data->spd.hOutput))
                InvalidateRect(hWnd, nullptr, TRUE);

            DWORD exitcode = 0;
            if (GetExitCodeProcess(data->spd.pi.hProcess, &exitcode) && exitcode != STILL_ACTIVE)
            {
                DestroyWindow(hWnd);
            }
        }
        break;

    case 2:
        {
            HWND hActive = GetActiveWindow();
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
    }
}

void RadTerminalWindowOnSizing(HWND hWnd, UINT edge, LPRECT prRect)
{
    const RadTerminalData* const data = (RadTerminalData*) GetWindowLongPtr(hWnd, GWLP_USERDATA);
    const DWORD style = GetWindowStyle(hWnd);
    UnadjustWindowRect(prRect, style, FALSE);
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

    AdjustWindowRect(prRect, style, FALSE);
}

void RadTerminalWindowOnActivate(HWND hWnd, UINT state, HWND hwndActDeact, BOOL fMinimized)
{
    if (state == WA_INACTIVE)
        InvalidateRect(hWnd, nullptr, TRUE);
}

/* void Cls_OnSizing(HWND hwnd, UINT edge, LPRECT prRect) */
#define HANDLE_WM_SIZING(hwnd, wParam, lParam, fn) \
    ((fn)((hwnd), (UINT)(wParam), (LPRECT)lParam), TRUE)

LRESULT CALLBACK RadTerminalWindowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
        HANDLE_MSG(hWnd, WM_CREATE, RadTerminalWindowOnCreate);
        HANDLE_MSG(hWnd, WM_DESTROY, RadTerminalWindowOnDestroy);
        HANDLE_MSG(hWnd, WM_PAINT, RadTerminalWindowOnPaint);
        HANDLE_MSG(hWnd, WM_KEYDOWN, RadTerminalWindowOnKeyDown);
        HANDLE_MSG(hWnd, WM_MOUSEACTIVATE, RadTerminalWindowOnMouseActivate);
        HANDLE_MSG(hWnd, WM_MOUSEMOVE, RadTerminalWindowOnMouseMove);
        HANDLE_MSG(hWnd, WM_LBUTTONDOWN, RadTerminalWindowOnLButtonDown);
        HANDLE_MSG(hWnd, WM_LBUTTONUP, RadTerminalWindowOnLButtonUp);
        HANDLE_MSG(hWnd, WM_RBUTTONDOWN, RadTerminalWindowOnRButtonDown);
        HANDLE_MSG(hWnd, WM_TIMER, RadTerminalWindowOnTimer);
        HANDLE_MSG(hWnd, WM_SIZE, RadTerminalWindowOnSize);
        HANDLE_MSG(hWnd, WM_SIZING, RadTerminalWindowOnSizing);
        HANDLE_MSG(hWnd, WM_ACTIVATE, RadTerminalWindowOnActivate);
        //case (WM_KEYDOWN): HANDLE_WM_KEYDOWN((hWnd), (wParam), (lParam), (RadTerminalWindowOnKeyDown)); return 1;
        //HANDLE_MSG(hWnd, WM_CHAR, RadTerminalWindowOnChar);
    default: return DefWindowProc(hWnd, uMsg, wParam, lParam);
    }
}
