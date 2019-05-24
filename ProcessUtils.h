#pragma once
#include <Windows.h>

struct SubProcessData
{
    HRESULT hr;
    HPCON hPC;
    PROCESS_INFORMATION pi;
    HANDLE hInput;
    HANDLE hOutput;
};

SubProcessData CreateSubProcess(LPCTSTR cmd, COORD zsCon, bool bUseConPty);
void CleanupSubProcess(const SubProcessData* spd);
UINT GetIcon(const SubProcessData* spd, HICON *phIconLarge, HICON *phIconSmall);
