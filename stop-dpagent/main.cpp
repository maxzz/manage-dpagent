#include <windows.h>
#include <shellapi.h>
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

    struct CliConfig
    {
        bool silentMode = true;
    };

    CliConfig BuildCliConfig()
    {
        CliConfig config;

        int argc = 0;
        LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
        if (argv == nullptr)
        {
            return config;
        }

        for (int i = 1; i < argc; ++i)
        {
            const std::wstring argument = argv[i];

            if (argument == L"--silent" || argument == L"/silent")
            {
                config.silentMode = true;
            }
            else if (argument == L"--no-silent" || argument == L"/no-silent")
            {
                config.silentMode = false;
            }
        }

        LocalFree(argv);
        return config;
    }

    std::vector<DWORD> GetDpAgentProcessIds()
    {
        std::vector<DWORD> processIds;

        DWORD currentSessionId = 0;
        if (!ProcessIdToSessionId(GetCurrentProcessId(), &currentSessionId))
        {
            return processIds;
        }

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
                    DWORD processSessionId = 0;
                    if (ProcessIdToSessionId(entry.th32ProcessID, &processSessionId) &&
                        processSessionId == currentSessionId)
                    {
                        processIds.push_back(entry.th32ProcessID);
                    }
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

		// Somehow there can be multiple DPAgent.exe processes running at the same time, so we need to loop through all of them and try to close them one by one
		// One ghost DpAgent.exe that cannot be closed (don't know why), and one x64 and one x86 DpAgent.exe that can be closed successfully. 
        // The ghost process is not visible in Task Manager, but it is visible in Process Explorer and it cannot be closed by any means, 
        // but it also doesn't cause any issues, so we can just ignore it and close the other two processes successfully.

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
    const CliConfig cliConfig = BuildCliConfig();
    const bool killResult = KillDpAgents();

    if (!killResult && IsDpAgentRunning())
    {
        if (!cliConfig.silentMode)
        {
            ShowFailureConsole();
        }

        return 1;
    }

    return 0;
}
