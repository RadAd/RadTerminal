#include <windows.h>
#include <windowsx.h>
#include <vector>
#include "WinUtils.h"
#include "resource.h"

#define PROJ_NAME TEXT("RadTerminal")
#define PROJ_CODE TEXT("RadTerminal")
#define REG_BASE  TEXT("Software\\RadSoft\\") PROJ_CODE

HWND ActionNewWindow(HWND hWnd, bool bParseCmdLine, const std::tstring& profile);
LRESULT CALLBACK RadTerminalMDIFrameProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

HWND CreateRadTerminalFrame(HINSTANCE hInstance)
{
    WNDCLASS wcMDIFrame = {};

    wcMDIFrame.lpfnWndProc = RadTerminalMDIFrameProc;
    wcMDIFrame.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_ICON1));
    wcMDIFrame.hCursor = LoadCursor(NULL, IDC_ARROW);
    //wcMDIFrame.hbrBackground = GetSysColorBrush(COLOR_WINDOW);
    wcMDIFrame.hInstance = hInstance;
    wcMDIFrame.lpszClassName = TEXT("MDIRadTerminal");

    ATOM atomFrame = RegisterClass(&wcMDIFrame);
    if (atomFrame == NULL)
        return NULL;

    HWND hFrame = CreateWindow(
        MAKEINTATOM(atomFrame),
        PROJ_NAME,
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
        NULL,       // Parent window
        LoadMenu(hInstance, MAKEINTRESOURCE(IDR_MENU1)),       // Menu
        hInstance,
        nullptr
    );
    if (hFrame == NULL)
        return NULL;

    return hFrame;
}

inline LRESULT MyDefFrameWindowProc(_In_ HWND hWnd, _In_ UINT Msg, _In_ WPARAM wParam, _In_ LPARAM lParam)
{
    return DefFrameProc(hWnd, GetMDIClient(hWnd), Msg, wParam, lParam);
}

struct RadTerminalFrameData
{
    std::vector<std::tstring> profiles;
};

BOOL RadTerminalFrameOnCreate(HWND hWnd, LPCREATESTRUCT lpCreateStruct)
{
    FORWARD_WM_CREATE(hWnd, lpCreateStruct, MyDefFrameWindowProc);
    const HINSTANCE hInst = GetWindowInstance(hWnd);

    RadTerminalFrameData* const data = new RadTerminalFrameData;
    SetWindowLongPtr(hWnd, GWLP_USERDATA, (LONG_PTR) data);

    CLIENTCREATESTRUCT ccs = {};
    HMENU hMenu = GetMenu(hWnd);
    ccs.hWindowMenu = GetSubMenu(hMenu, GetMenuItemCount(hMenu) - 1);
    ccs.idFirstChild = IDM_WINDOWCHILD;

    {
        HKEY hMainKey = NULL;
        if (RegOpenKey(HKEY_CURRENT_USER, REG_BASE TEXT("\\Profiles"), &hMainKey) == ERROR_SUCCESS)
        {
            const std::tstring strDefault = RegGetString(HKEY_CURRENT_USER, REG_BASE, TEXT("Profile"), TEXT("Cmd"));
            DWORD i = 0;
            std::tstring strName;
            for (int i = 0; RegEnumKeyEx(hMainKey, i, strName); ++i)
            {
                data->profiles.push_back(strName);
                if (strName != TEXT("Default"))
                {
                    MENUITEMINFO mii = {};
                    mii.cbSize = sizeof(mii);
                    mii.fMask = MIIM_ID | MIIM_STRING | MIIM_STATE;
                    mii.fType = MFT_STRING;
                    if (strName == strDefault)
                        mii.fState |= MFS_DEFAULT;
                    mii.wID = ID_NEW_PROFILE_1 + i;
                    mii.dwTypeData = (LPTSTR) strName.c_str();
                    InsertMenuItem(hMenu, ID_NEW_PLACEHOLDER, FALSE, &mii);
                }
            }
            DeleteMenu(hMenu, ID_NEW_PLACEHOLDER, MF_BYCOMMAND);
            RegCloseKey(hMainKey);
        }
    }

    HWND hWndMDIClient = CreateWindow(TEXT("MDICLIENT"), (LPCTSTR) NULL,
        WS_CHILD | WS_CLIPCHILDREN | WS_VSCROLL | WS_HSCROLL,
        0, 0, 0, 0, hWnd, (HMENU) 0, hInst, (LPSTR) &ccs);
    if (hWndMDIClient == NULL)
        return FALSE;

    ShowWindow(hWndMDIClient, SW_SHOW);

    return TRUE;
}

void RadTerminalFrameOnDestroy(HWND hWnd)
{
    FORWARD_WM_DESTROY(hWnd, MyDefFrameWindowProc);
    const RadTerminalFrameData* const data = (RadTerminalFrameData*) GetWindowLongPtr(hWnd, GWLP_USERDATA);
    PostQuitMessage(0);
    delete data;
    SetWindowLongPtr(hWnd, GWLP_USERDATA, (LONG_PTR) nullptr);
}

void RadTerminalFrameOnSizing(HWND hWnd, UINT edge, LPRECT prRect)
{
    FORWARD_WM_SIZING(hWnd, edge, prRect, MyDefFrameWindowProc);
    HWND hWndMDIClient = GetMDIClient(hWnd);
    BOOL bMaximized = FALSE;
    HWND hActive = GetMDIActive(hWndMDIClient, &bMaximized);
    if (hActive != NULL && bMaximized)
    {
        FORWARD_WM_SIZING(hActive, edge, prRect, SendMessage);
        UnadjustWindowRectEx(prRect, GetWindowStyle(hActive), GetMenu(hActive) != NULL, GetWindowExStyle(hActive));
        AdjustWindowRectEx(prRect, GetWindowStyle(hWnd), GetMenu(hWnd) != NULL, GetWindowExStyle(hWnd));
    }
}

void RadTerminalFrameOnCommand(HWND hWnd, int id, HWND hWndCtl, UINT codeNotify)
{
    const HWND hWndMDIClient = GetMDIClient(hWnd);
    const RadTerminalFrameData* const data = (RadTerminalFrameData*) GetWindowLongPtr(hWnd, GWLP_USERDATA);

    switch (id)
    {
    case ID_FILE_NEW:   ActionNewWindow(hWnd, false, TEXT(""));  break;
    case ID_NEW_PROFILE_1: case ID_NEW_PROFILE_2: case ID_NEW_PROFILE_3:
    case ID_NEW_PROFILE_4: case ID_NEW_PROFILE_5: case ID_NEW_PROFILE_6:
    case ID_NEW_PROFILE_7: case ID_NEW_PROFILE_8: case ID_NEW_PROFILE_9:
        ActionNewWindow(hWnd, false, data->profiles[id - ID_NEW_PROFILE_1]);  break;
    case ID_FILE_EXIT:  PostMessage(hWnd, WM_CLOSE, 0, 0);  break;
    case ID_WINDOW_CASCADE:  CascadeWindows(hWndMDIClient, 0, nullptr, 0, nullptr);  break;
    case ID_WINDOW_TILEHORIZONTALLY:  TileWindows(hWndMDIClient, MDITILE_HORIZONTAL, nullptr, 0, nullptr);  break;
    case ID_WINDOW_TILEVERTICALLY:  TileWindows(hWndMDIClient, MDITILE_VERTICAL, nullptr, 0, nullptr);  break;

    case ID_FILE_CLOSE: FORWARD_WM_COMMAND(GetMDIActive(hWndMDIClient), id, hWndCtl, codeNotify, SendMessage);  break;

    default:    FORWARD_WM_COMMAND(hWnd, id, hWndCtl, codeNotify, MyDefFrameWindowProc); break;
    }
}

LRESULT CALLBACK RadTerminalMDIFrameProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
        HANDLE_MSG(hWnd, WM_CREATE, RadTerminalFrameOnCreate);
        HANDLE_MSG(hWnd, WM_DESTROY, RadTerminalFrameOnDestroy);
        HANDLE_MSG(hWnd, WM_SIZING, RadTerminalFrameOnSizing);
        HANDLE_MSG(hWnd, WM_COMMAND, RadTerminalFrameOnCommand);
    default: return MyDefFrameWindowProc(hWnd, uMsg, wParam, lParam);
    }
}
