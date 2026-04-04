#include "dpagent_processes.h"

#include <tlhelp32.h>

#include <cwchar>

namespace
{
    constexpr wchar_t ProcessName[] = L"DPAgent.exe";
    constexpr wchar_t WindowClassName[] = L"DigitalPersona Pro5.x Agent Window Class";

    struct WindowSearchContext
    {
        const std::vector<DWORD>* processIds = nullptr;
        std::vector<HWND>* windowHandles = nullptr;
    };
}

BOOL CALLBACK CollectDpAgentWindows(HWND hWnd, LPARAM lParam)
{
    wchar_t className[256] = {};
    if (GetClassNameW(hWnd, className, ARRAYSIZE(className)) == 0)
    {
        return TRUE;
    }

    if (_wcsicmp(className, WindowClassName) != 0)
    {
        return TRUE;
    }

    DWORD processId = 0;
    GetWindowThreadProcessId(hWnd, &processId);

    auto* context = reinterpret_cast<WindowSearchContext*>(lParam);
    for (DWORD candidateProcessId : *context->processIds)
    {
        if (candidateProcessId == processId)
        {
            context->windowHandles->push_back(hWnd);
            break;
        }
    }

    return TRUE;
}

std::vector<DWORD> GetDpAgentProcessIds()
{
    std::vector<DWORD> rv;

    DWORD currentSessionId = 0;
    if (!ProcessIdToSessionId(GetCurrentProcessId(), &currentSessionId))
    {
        return rv;
    }

    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE)
    {
        return rv;
    }

    PROCESSENTRY32W entry = {};
    entry.dwSize = sizeof(entry);

    if (Process32FirstW(snapshot, &entry))
    {
        do
        {
            if (_wcsicmp(entry.szExeFile, ProcessName) == 0)
            {
                DWORD processSessionId = 0;
                if (ProcessIdToSessionId(entry.th32ProcessID, &processSessionId) && processSessionId == currentSessionId)
                {
                    rv.push_back(entry.th32ProcessID);
                }
            }
        } while (Process32NextW(snapshot, &entry));
    }

    CloseHandle(snapshot);
    return rv;
}

std::vector<HWND> GetDpAgentWindowHandles(const std::vector<DWORD>& processIds)
{
    std::vector<HWND> rv;
    WindowSearchContext context = { &processIds, &rv };
    EnumWindows(CollectDpAgentWindows, reinterpret_cast<LPARAM>(&context));
    return rv;
}
