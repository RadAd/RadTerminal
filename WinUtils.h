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

inline RECT Rect(POINT p1, POINT p2)
{
    return { p1.x, p1.y, p2.x, p2.y };
}

inline RECT Rect(POINT p1, SIZE s2)
{
    return { p1.x, p1.y, p1.x + s2.cx, p1.y + s2.cy };
}

inline HFONT CreateFont(LPTSTR pFontFace, int iFontHeight, int cWeight, BOOL bItalic, BOOL bUnderline)
{
    return CreateFont(iFontHeight, 0, 0, 0, cWeight, bItalic, bUnderline, FALSE, ANSI_CHARSET,
        OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY,
        DEFAULT_PITCH | FF_DONTCARE, pFontFace);
}
