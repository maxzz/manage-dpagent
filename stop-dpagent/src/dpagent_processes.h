#pragma once

#include <windows.h>

#include <vector>

std::vector<DWORD> GetDpAgentProcessIds();
std::vector<HWND> GetDpAgentWindowHandles(const std::vector<DWORD>& processIds);
