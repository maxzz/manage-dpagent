#include <windows.h>
#include <shellapi.h>

#include "dpagent_processes.h"

#include <cstdio>
#include <iostream>
#include <string>

namespace
{
    constexpr DWORD WaitTimeoutMs = 10000;
    constexpr DWORD SuccessDelayMs = 1000;
    constexpr DWORD FailureDelayMs = 3000;

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

    bool IsDpAgentRunning()
    {
        return !GetDpAgentProcessIds().empty();
    }

    bool InitializeConsole()
    {
        if (!AllocConsole())
        {
            return false;
        }

        FILE* stream = nullptr;
        freopen_s(&stream, "CONIN$", "r", stdin);
        freopen_s(&stream, "CONOUT$", "w", stdout);
        freopen_s(&stream, "CONOUT$", "w", stderr);

        return true;
    }

    void ShowMessageBox(const wchar_t* message, const wchar_t* title, UINT type)
    {
        MessageBoxW(nullptr, message, title, type);
    }

    void WriteConsoleLine(const wchar_t* line)
    {
        std::wcerr << line << std::endl;
    }

    void ShowFailureConsole()
    {
        if (!InitializeConsole())
        {
            ShowMessageBox(L"Failed to initialize console.", L"Error", MB_OK | MB_ICONERROR);
            return;
        }

        WriteConsoleLine(L"DpAgent.exe is still running and cannot be closed after the attempt.");
        WriteConsoleLine(L"Press Enter to exit...");

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

        // One DpAgent.exe is running in session 1 and this DpAgent.exe cannot be and should not be terminated.
        // The other two DpAgent.exe processes are one x64 and one x86 DpAgent.exe are running in the current session 
        // and can be closed successfully.

        std::vector<HANDLE> processHandles;
        processHandles.reserve(processIds.size());

        for (DWORD processId : processIds)
        {
            HANDLE processHandle = OpenProcess(SYNCHRONIZE, FALSE, processId);
            if (processHandle == nullptr)
            {
                continue;
            }

            processHandles.push_back(processHandle);
        }

        const auto windowHandles = GetDpAgentWindowHandles(processIds);
        if (windowHandles.empty())
        {
            for (HANDLE processHandle : processHandles)
            {
                CloseHandle(processHandle);
            }

            return false;
        }

        for (HWND hWnd : windowHandles)
        {
            PostMessageW(hWnd, WM_CLOSE, 0, 0);
        }

        const ULONGLONG waitUntil = GetTickCount64() + WaitTimeoutMs;
        while (!processHandles.empty() && GetTickCount64() < waitUntil)
        {
            for (auto it = processHandles.begin(); it != processHandles.end();)
            {
                if (WaitForSingleObject(*it, 0) == WAIT_OBJECT_0)
                {
                    CloseHandle(*it);
                    it = processHandles.erase(it);
                }
                else
                {
                    ++it;
                }
            }

            if (!processHandles.empty())
            {
                Sleep(100);
            }
        }

        for (HANDLE processHandle : processHandles)
        {
            CloseHandle(processHandle);
        }

        return GetDpAgentProcessIds().empty();
    }
}

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int)
{
    const CliConfig cliConfig = BuildCliConfig();
    const bool killedAll = KillDpAgents();

    if (!killedAll)
    {
        const auto pids = GetDpAgentProcessIds();
        std::wstring msg;
        if (pids.empty())
        {
            msg = L"DpAgent.exe is still running, but no process IDs were found.";
        }
        else
        {
            msg = L"DpAgent.exe is still running (PIDs):\n";
            for (DWORD pid : pids)
            {
                msg += std::to_wstring(pid) + L"\n";
            }
        }

        ShowMessageBox(msg.c_str(), L"DpAgent running", MB_OK | MB_ICONWARNING);

        return 1;
    }

    return 0;
}
