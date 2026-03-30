#include <windows.h>
#include <tlhelp32.h>

#include <cstdio>
#include <cwchar>
#include <iostream>
#include <string>
#include <vector>

namespace
{
    constexpr DWORD WaitTimeoutMs = 10000;
    constexpr DWORD SuccessDelayMs = 1000;
    constexpr DWORD FailureDelayMs = 3000;
    constexpr wchar_t ProcessName[] = L"DPAgent.exe";
    constexpr wchar_t WindowClassName[] = L"DigitalPersona Pro5.x Agent Window Class";

    std::vector<DWORD> GetDpAgentProcessIds()
    {
        std::vector<DWORD> processIds;

        HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        if (snapshot == INVALID_HANDLE_VALUE)
        {
            return processIds;
        }

        PROCESSENTRY32W entry = {};
        entry.dwSize = sizeof(entry);

        if (Process32FirstW(snapshot, &entry))
        {
            do
            {
                if (_wcsicmp(entry.szExeFile, ProcessName) == 0)
                {
                    processIds.push_back(entry.th32ProcessID);
                }
            } while (Process32NextW(snapshot, &entry));
        }

        CloseHandle(snapshot);
        return processIds;
    }

    bool IsDpAgentRunning()
    {
        return !GetDpAgentProcessIds().empty();
    }

    void ShowFailureConsole()
    {
        if (!AllocConsole())
        {
            return;
        }

        FILE* stream = nullptr;
        freopen_s(&stream, "CONIN$", "r", stdin);
        freopen_s(&stream, "CONOUT$", "w", stdout);
        freopen_s(&stream, "CONOUT$", "w", stderr);

        std::wcerr << L"DpAgent.exe is still running and cannot be closed after the attempt." << std::endl;
        std::wcerr << L"Press Enter to exit..." << std::endl;

        std::wstring line;
        std::getline(std::wcin, line);
    }

    bool KillDpAgents()
    {
        const auto processIds = GetDpAgentProcessIds();
        if (processIds.empty())
        {
            return true;
        }

        for (DWORD processId : processIds)
        {
            HANDLE processHandle = OpenProcess(SYNCHRONIZE, FALSE, processId);
            if (processHandle == nullptr)
            {
                continue;
            }

            HWND hWnd = FindWindowW(WindowClassName, nullptr);
            if (hWnd != nullptr)
            {
                PostMessageW(hWnd, WM_CLOSE, 0, 0);

                DWORD waitResult = WaitForSingleObject(processHandle, WaitTimeoutMs);
                CloseHandle(processHandle);

                if (waitResult == WAIT_OBJECT_0)
                {
					//Sleep(SuccessDelayMs); //don't need to wait after successful close, we can check for the next process immediately
                    return true;
                }
            }
            else
            {
                CloseHandle(processHandle);
            }
        }

		//Sleep(FailureDelayMs); // if none of the processes were closed within the loop return false immediately without waiting for the timeout
        return false;
    }
}

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int)
{
    const bool killResult = KillDpAgents();

    if (!killResult && IsDpAgentRunning())
    {
        ShowFailureConsole();
        return 1;
    }

    return 0;
}
