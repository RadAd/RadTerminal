#include <windows.h>
#include <windowsx.h>
#include "WinUtils.h"
#include "resource.h"

#define PROJ_NAME TEXT("RadTerminal")

HWND ActionNewWindow(HWND hWnd, bool bParseCmdLine);
LRESULT CALLBACK RadTerminalMDIFrameProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

HWND CreateRadTerminalFrame(HINSTANCE hInstance, int nCmdShow)
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

    ShowWindow(hFrame, nCmdShow);

    return hFrame;
}

inline LRESULT MyDefFrameWindowProc(_In_ HWND hWnd, _In_ UINT Msg, _In_ WPARAM wParam, _In_ LPARAM lParam)
{
    return DefFrameProc(hWnd, GetMDIClient(hWnd), Msg, wParam, lParam);
}

BOOL RadTerminalFrameOnCreate(HWND hWnd, LPCREATESTRUCT lpCreateStruct)
{
    FORWARD_WM_CREATE(hWnd, lpCreateStruct, MyDefFrameWindowProc);
    HINSTANCE hInst = GetWindowInstance(hWnd);

    CLIENTCREATESTRUCT ccs = {};
    HMENU hMenu = GetMenu(hWnd);
    ccs.hWindowMenu = GetSubMenu(hMenu, GetMenuItemCount(hMenu) - 1);
    ccs.idFirstChild = IDM_WINDOWCHILD;

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
    PostQuitMessage(0);
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
    HWND hWndMDIClient = GetMDIClient(hWnd);

    switch (id)
    {
    case ID_FILE_NEW:   ActionNewWindow(hWnd, false);  break;
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
