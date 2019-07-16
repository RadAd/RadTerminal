#pragma once
#include <windows.h>
#include <string>

#ifdef _UNICODE
#define tstring wstring
#else
#define tstring string
#endif

/* void Cls_OnSizing(HWND hwnd, UINT edge, LPRECT prRect) */
#define HANDLE_WM_SIZING(hwnd, wParam, lParam, fn) \
    ((fn)((hwnd), (UINT)(wParam), (LPRECT)lParam), TRUE)
#define FORWARD_WM_SIZING(hwnd, edge, prRect, fn) \
    (void)(fn)((hwnd), WM_SIZING, (WPARAM)edge, (LPARAM)prRect)

inline BOOL UnadjustWindowRect(
    LPRECT prc,
    DWORD dwStyle,
    BOOL fMenu)
{
    RECT rc;
    SetRectEmpty(&rc);
    BOOL fRc = AdjustWindowRect(&rc, dwStyle, fMenu);
    if (fRc) {
        prc->left -= rc.left;
        prc->top -= rc.top;
        prc->right -= rc.right;
        prc->bottom -= rc.bottom;
    }
    return fRc;
}

inline BOOL UnadjustWindowRectEx(
    LPRECT prc,
    DWORD dwStyle,
    BOOL fMenu,
    DWORD dwExStyle)
{
    RECT rc;
    SetRectEmpty(&rc);
    BOOL fRc = AdjustWindowRectEx(&rc, dwStyle, fMenu, dwExStyle);
    if (fRc) {
        prc->left -= rc.left;
        prc->top -= rc.top;
        prc->right -= rc.right;
        prc->bottom -= rc.bottom;
    }
    return fRc;
}

inline RECT Rect(POINT p1, POINT p2)
{
    return { p1.x, p1.y, p2.x, p2.y };
}

inline RECT Rect(POINT p1, SIZE s2)
{
    return { p1.x, p1.y, p1.x + s2.cx, p1.y + s2.cy };
}

inline HFONT CreateFont(LPCTSTR pFontFace, int iFontHeight, int cWeight, BOOL bItalic, BOOL bUnderline)
{
    return CreateFont(iFontHeight, 0, 0, 0, cWeight, bItalic, bUnderline, FALSE, ANSI_CHARSET,
        OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY,
        DEFAULT_PITCH | FF_DONTCARE, pFontFace);
}

inline std::string RegGetString(HKEY hKey, LPCSTR sValue, const std::string& strDef)
{
    CHAR buf[1024];
    DWORD len = (ARRAYSIZE(buf) - 1) * sizeof(CHAR);
    if (RegGetValueA(hKey, nullptr, sValue, RRF_RT_REG_SZ, nullptr, buf, &len) == ERROR_SUCCESS)
    {
        buf[len / sizeof(CHAR)] = TEXT('\0');
        return buf;
    }
    else
        return strDef;
}

inline std::string RegGetString(HKEY hKey, LPCSTR sSubKey, LPCSTR sValue, const std::string& strDef)
{
    CHAR buf[1024];
    DWORD len = (ARRAYSIZE(buf) - 1) * sizeof(CHAR);
    if (RegGetValueA(hKey, sSubKey, sValue, RRF_RT_REG_SZ, nullptr, buf, &len) == ERROR_SUCCESS)
    {
        buf[len / sizeof(CHAR)] = TEXT('\0');
        return buf;
    }
    else
        return strDef;
}

inline std::tstring RegGetString(HKEY hKey, LPCWSTR sValue, const std::tstring& strDef)
{
    WCHAR buf[1024];
    DWORD len = (ARRAYSIZE(buf) - 1) * sizeof(WCHAR);
    if (RegGetValueW(hKey, nullptr, sValue, RRF_RT_REG_SZ, nullptr, buf, &len) == ERROR_SUCCESS)
    {
        buf[len / sizeof(WCHAR)] = TEXT('\0');
        return buf;
    }
    else
        return strDef;
}

inline std::tstring RegGetString(HKEY hKey, LPCWSTR sSubKey, LPCWSTR sValue, const std::tstring& strDef)
{
    WCHAR buf[1024];
    DWORD len = (ARRAYSIZE(buf) - 1) * sizeof(WCHAR);
    if (RegGetValueW(hKey, sSubKey, sValue, RRF_RT_REG_SZ, nullptr, buf, &len) == ERROR_SUCCESS)
    {
        buf[len / sizeof(WCHAR)] = TEXT('\0');
        return buf;
    }
    else
        return strDef;
}

inline DWORD RegGetDWORD(HKEY hKey, LPCTSTR sValue, DWORD dwDef)
{
    DWORD data = 0;
    DWORD len = sizeof(data);
    if (RegGetValue(hKey, nullptr, sValue, RRF_RT_REG_DWORD, nullptr, &data, &len) == ERROR_SUCCESS)
        return data;
    else
        return dwDef;
}

inline DWORD RegGetDWORD(HKEY hKey, LPCTSTR sSubKey, LPCTSTR sValue, DWORD dwDef)
{
    DWORD data = 0;
    DWORD len = sizeof(data);
    if (RegGetValue(hKey, sSubKey, sValue, RRF_RT_REG_DWORD, nullptr, &data, &len) == ERROR_SUCCESS)
        return data;
    else
        return dwDef;
}

inline bool RegEnumKeyEx(_In_ HKEY hKey, _In_ DWORD dwIndex, std::tstring& strName)
{
    TCHAR name[256];
    DWORD len = ARRAYSIZE(name);
    if (RegEnumKeyEx(hKey, dwIndex, name, &len, nullptr, nullptr, nullptr, nullptr) == ERROR_SUCCESS)
    {
        strName = name;
        return true;
    }
    else
        return false;
}

inline HWND GetMDIClient(HWND hWnd)
{
    return FindWindowEx(hWnd, NULL, TEXT("MDICLIENT"), nullptr);
}

inline HWND GetMDIActive(HWND hWndMDIClient, BOOL* pbMaximized = nullptr)
{
    return (HWND) SendMessage(hWndMDIClient, WM_MDIGETACTIVE, 0, (LPARAM) pbMaximized);
}

inline bool IsMDIChild(HWND hWnd)
{
    return (GetWindowExStyle(hWnd) & WS_EX_MDICHILD) != 0;
}

inline HWND MyGetActiveWnd(HWND hWnd)
{
    HWND hActive = GetActiveWindow();
    if (hActive != NULL && IsMDIChild(hWnd))
        hActive = GetMDIActive(GetParent(hWnd));
    return hActive;
}

inline bool FindMenuPos(HMENU hBaseMenu, UINT myID, HMENU* pMenu, int* mpos)
{
    if (hBaseMenu == NULL)
    {
        *pMenu = NULL;
        *mpos = -1;
        return true;
    }

    for (int myPos = 0; myPos < GetMenuItemCount(hBaseMenu); ++myPos)
    {
        if (GetMenuItemID(hBaseMenu, myPos) == myID)
        {
            *pMenu = hBaseMenu;
            *mpos = myPos;
            return true;
        }

        HMENU hNewMenu = GetSubMenu(hBaseMenu, myPos);
        if (hNewMenu != NULL)
        {
            if (FindMenuPos(hNewMenu, myID, pMenu, mpos))
                return true;
        }
    }

    return false;
}
