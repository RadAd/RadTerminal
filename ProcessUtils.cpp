#include "ProcessUtils.h"
#include <Windows.h>
#include <tchar.h>
#include <vector>

// TODO
// Support .lnk files

namespace {
    void Clear(SubProcessData* spd)
    {
        spd->hOutput = NULL;
        spd->hInput = NULL;
        spd->pi.hThread = NULL;
        spd->pi.hProcess = NULL;
        spd->hPC = NULL;
    }

    SubProcessData CreateSubProcess(LPCTSTR cmd, COORD size, HANDLE hInput, HANDLE hOutput, bool bUseConPty)
    {
        SubProcessData spd = {};

        DWORD dwCreationFlags = EXTENDED_STARTUPINFO_PRESENT;
        STARTUPINFOEX si = {};
        std::vector<BYTE> attrList;
        si.StartupInfo.cb = sizeof(si);

        BOOL bInheritHandles = FALSE;

        if (bUseConPty)
        {
            spd.hr = CreatePseudoConsole(
                size,
                hInput,
                hOutput,
                0,
                &spd.hPC);
            if (spd.hr != S_OK)
            {
                CleanupSubProcess(&spd);
                Clear(&spd);
                return spd;
            }

            SIZE_T size = 0;
            if (InitializeProcThreadAttributeList(NULL, 1, 0, &size))
            {
                spd.hr = HRESULT_FROM_WIN32(GetLastError());
                CleanupSubProcess(&spd);
                Clear(&spd);
                return spd;
            }

            attrList.resize(size);
            si.lpAttributeList = reinterpret_cast<PPROC_THREAD_ATTRIBUTE_LIST>(attrList.data());
            if (!InitializeProcThreadAttributeList(si.lpAttributeList, 1, 0, &size))
            {
                spd.hr = HRESULT_FROM_WIN32(GetLastError());
                CleanupSubProcess(&spd);
                Clear(&spd);
                return spd;
            }
            if (!UpdateProcThreadAttribute(
                si.lpAttributeList,
                0,
                PROC_THREAD_ATTRIBUTE_PSEUDOCONSOLE,
                spd.hPC, sizeof(HPCON),
                NULL,
                NULL))
            {
                spd.hr = HRESULT_FROM_WIN32(GetLastError());
                CleanupSubProcess(&spd);
                Clear(&spd);
                return spd;
            }

            si.StartupInfo.dwFlags = STARTF_USESTDHANDLES;
        }
        else
        {
            si.StartupInfo.dwXSize = size.X;
            si.StartupInfo.dwYSize = size.Y;
            si.StartupInfo.dwFlags = STARTF_USESIZE | STARTF_USESTDHANDLES;
            si.StartupInfo.hStdInput = hInput;
            si.StartupInfo.hStdOutput = hOutput;
            si.StartupInfo.hStdError = si.StartupInfo.hStdOutput;
            dwCreationFlags |= CREATE_NO_WINDOW;
            bInheritHandles = TRUE;
        }

        TCHAR localcmd[MAX_PATH];
        _tcscpy_s(localcmd, cmd);
        if (!CreateProcess(nullptr, localcmd, nullptr, nullptr, bInheritHandles, dwCreationFlags, nullptr, nullptr, &si.StartupInfo, &spd.pi))
        {
            spd.hr = HRESULT_FROM_WIN32(GetLastError());
            CleanupSubProcess(&spd);
            Clear(&spd);
            return spd;
        }

        spd.hr = S_OK;
        return spd;
    }
}

SubProcessData CreateSubProcess(LPCTSTR cmd, COORD size, bool bUseConPty)
{
    SubProcessData spd = {};

    SECURITY_ATTRIBUTES sa = {};
    sa.bInheritHandle = !bUseConPty;

    HANDLE hReadPipeInput = NULL;
    HANDLE hWritePipeInput = NULL;
    if (spd.hr == S_OK && !CreatePipe(&hReadPipeInput, &hWritePipeInput, &sa, 0))
        spd.hr = HRESULT_FROM_WIN32(GetLastError());

    HANDLE hWritePipeOutput = NULL;
    HANDLE hReadPipeOutput = NULL;
    if (spd.hr == S_OK && !CreatePipe(&hReadPipeOutput, &hWritePipeOutput, &sa, 0))
        spd.hr = HRESULT_FROM_WIN32(GetLastError());

    if (spd.hr == S_OK)
        spd = CreateSubProcess(cmd, size, hReadPipeInput, hWritePipeOutput, bUseConPty);

    if (spd.hr == S_OK)
    {
        spd.hInput = hWritePipeInput;
        spd.hOutput = hReadPipeOutput;
    }
    else
    {
        CloseHandle(hWritePipeInput);
        CloseHandle(hReadPipeOutput);
    }

    CloseHandle(hReadPipeInput);
    CloseHandle(hWritePipeOutput);

    return spd;
}

void CleanupSubProcess(const SubProcessData* spd)
{
    CloseHandle(spd->hOutput);
    CloseHandle(spd->hInput);

    TerminateProcess(spd->pi.hProcess, 0xFF);

    CloseHandle(spd->pi.hThread);
    CloseHandle(spd->pi.hProcess);
    ClosePseudoConsole(spd->hPC);
}

UINT GetIcon(const SubProcessData* spd, HICON *phIconLarge, HICON *phIconSmall)
{
    TCHAR exe[MAX_PATH];
    DWORD exelen = ARRAYSIZE(exe);
    QueryFullProcessImageName(spd->pi.hProcess, 0, exe, &exelen);

    WORD iIcon = 0;
    return ExtractIconEx(exe, 0, phIconLarge, phIconSmall, 1);
}
