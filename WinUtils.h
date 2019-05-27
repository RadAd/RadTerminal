#pragma once
#include <windows.h>

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

std::string RegGetString(HKEY hKey, LPCSTR sValue, const std::string& strDef)
{
    CHAR buf[1024];
    DWORD len = (ARRAYSIZE(buf) - 1) * sizeof(CHAR);
    if (RegGetValueA(hKey, nullptr, sValue, RRF_RT_REG_SZ, nullptr, buf, &len) == ERROR_SUCCESS)
    {
        buf[len / sizeof(CHAR)] = _T('\0');
        return buf;
    }
    else
        return strDef;
}

std::wstring RegGetString(HKEY hKey, LPCWSTR sValue, const std::wstring& strDef)
{
    WCHAR buf[1024];
    DWORD len = (ARRAYSIZE(buf) - 1) * sizeof(WCHAR);
    if (RegGetValueW(hKey, nullptr, sValue, RRF_RT_REG_SZ, nullptr, buf, &len) == ERROR_SUCCESS)
    {
        buf[len / sizeof(WCHAR)] = _T('\0');
        return buf;
    }
    else
        return strDef;
}

DWORD RegGetDWORD(HKEY hKey, LPCTSTR sValue, DWORD dwDef)
{
    DWORD data = 0;
    DWORD len = sizeof(data);
    if (RegGetValue(hKey, nullptr, sValue, RRF_RT_REG_DWORD, nullptr, &data, &len) == ERROR_SUCCESS)
        return data;
    else
        return dwDef;
}
