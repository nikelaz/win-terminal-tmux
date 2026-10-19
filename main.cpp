/*
 * Windows Terminal Tmux Bindings Tray Program
 * Copyright Nikola Lazarov 2026
 *
 * Everything is under the MIT License
 */

// Prefix (Ctrl+B) then:
//   C        new tab                     -> Ctrl+Shift+T
//   %        split left|right (50/50)    -> Alt+Shift+Plus
//   1..9     switch to tab by index      -> Ctrl+Alt+1..9 (9 = last tab in WT)
//   O        toggle focus between panes  -> Ctrl+Alt+Left (previous pane)
//   X        close focused pane / tab    -> Ctrl+Shift+W

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <shellapi.h>
#include <wchar.h>
#include <initializer_list>

constexpr UINT kTrayMsg = WM_APP + 1;
constexpr UINT kCmdToggle = 1;
constexpr UINT kCmdExit = 2;
constexpr ULONGLONG kPrefixTimeoutMs = 2000;
constexpr wchar_t kMutexName[] = L"Local\\WtTmuxTrayMutex";
constexpr wchar_t kWindowClass[] = L"WtTmuxTrayWindow";
constexpr wchar_t kTerminalExe[] = L"WindowsTerminal.exe";

HHOOK g_hook = nullptr;
HWND g_hwnd = nullptr;
UINT g_taskbarCreated = 0;
bool g_enabled = true;
bool g_prefixActive = false;
ULONGLONG g_prefixStart = 0;
// Virtual keys whose keydown we consumed; their keyup (and autorepeat) are swallowed too.
bool g_swallowUp[256] = {};

bool IsDown(int vk) { return (GetAsyncKeyState(vk) & 0x8000) != 0; }

bool IsModifier(DWORD vk) {
    return vk == VK_SHIFT || vk == VK_CONTROL || vk == VK_MENU || vk == VK_LWIN ||
           vk == VK_RWIN || (vk >= VK_LSHIFT && vk <= VK_RMENU);
}

bool IsWindowsTerminalForeground() {
    HWND fg = GetForegroundWindow();
    DWORD pid = 0;
    if (!fg || !GetWindowThreadProcessId(fg, &pid)) return false;
    HANDLE proc = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!proc) return false;
    wchar_t path[MAX_PATH];
    DWORD len = MAX_PATH;
    bool result = false;
    if (QueryFullProcessImageNameW(proc, 0, path, &len)) {
        const wchar_t* name = wcsrchr(path, L'\\');
        result = _wcsicmp(name ? name + 1 : path, kTerminalExe) == 0;
    }
    CloseHandle(proc);
    return result;
}

// Sends a key chord to the focused window. Physically held modifiers are released first
// so that the chord Windows Terminal receives is exactly the one requested.
void SendChord(std::initializer_list<WORD> mods, WORD key) {
    INPUT in[32] = {};
    int n = 0;
    auto add = [&](WORD vk, DWORD flags) {
        in[n].type = INPUT_KEYBOARD;
        in[n].ki.wVk = vk;
        in[n].ki.dwFlags = flags;
        ++n;
    };
    for (WORD m : {static_cast<WORD>(VK_CONTROL), static_cast<WORD>(VK_SHIFT), static_cast<WORD>(VK_MENU)}) {
        if (IsDown(m)) add(m, KEYEVENTF_KEYUP);
    }
    for (WORD m : mods) add(m, 0);
    add(key, 0);
    add(key, KEYEVENTF_KEYUP);
    for (auto it = mods.end(); it != mods.begin();) {
        --it;
        add(*it, KEYEVENTF_KEYUP);
    }
    SendInput(static_cast<UINT>(n), in, sizeof(INPUT));
}

// Performs the action bound to a key pressed after the prefix. Returns false if unbound.
bool RunPrefixAction(DWORD vk, bool shift) {
    if (vk == 'C') {
        SendChord({VK_CONTROL, VK_SHIFT}, 'T');
    } else if (vk == '5' && shift) {
        SendChord({VK_MENU, VK_SHIFT}, VK_OEM_PLUS);
    } else if (vk == 'O') {
        SendChord({VK_CONTROL, VK_MENU}, VK_LEFT);
    } else if (vk == 'X') {
        SendChord({VK_CONTROL, VK_SHIFT}, 'W');
    } else if (vk >= '1' && vk <= '9') {
        SendChord({VK_CONTROL, VK_MENU}, static_cast<WORD>(vk));
    } else {
        return false;
    }
    return true;
}

LRESULT CALLBACK KeyboardProc(int code, WPARAM wParam, LPARAM lParam) {
    auto next = [&] { return CallNextHookEx(g_hook, code, wParam, lParam); };
    if (code != HC_ACTION) return next();

    const auto* k = reinterpret_cast<const KBDLLHOOKSTRUCT*>(lParam);
    if (k->flags & LLKHF_INJECTED) return next();  // ignore our own SendInput output

    const DWORD vk = k->vkCode;
    const bool down = wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN;
    const bool up = wParam == WM_KEYUP || wParam == WM_SYSKEYUP;
    if (!g_enabled || vk >= 256) return next();

    if (up) {
        if (g_swallowUp[vk]) {
            g_swallowUp[vk] = false;
            return 1;
        }
        return next();
    }
    if (!down) return next();

    if (g_swallowUp[vk]) return 1;  // autorepeat of a key we already consumed

    if (!g_prefixActive) {
        if (vk == 'B' && IsDown(VK_CONTROL) && !IsDown(VK_SHIFT) && !IsDown(VK_MENU) &&
            IsWindowsTerminalForeground()) {
            g_prefixActive = true;
            g_prefixStart = GetTickCount64();
            g_swallowUp[vk] = true;
            return 1;
        }
        return next();
    }

    // Prefix is armed: the next non-modifier key decides the action.
    if (IsModifier(vk)) return next();
    g_prefixActive = false;
    if (GetTickCount64() - g_prefixStart > kPrefixTimeoutMs || !IsWindowsTerminalForeground()) {
        return next();
    }
    if (RunPrefixAction(vk, IsDown(VK_SHIFT))) {
        g_swallowUp[vk] = true;
        return 1;
    }
    return next();  // unbound key: prefix is cancelled and the key passes through
}

void SetTray(DWORD message) {
    NOTIFYICONDATAW nid = {};
    nid.cbSize = sizeof(nid);
    nid.hWnd = g_hwnd;
    nid.uID = 1;
    nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    nid.uCallbackMessage = kTrayMsg;
    nid.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
    wcscpy_s(nid.szTip, g_enabled ? L"WT tmux keys: ON (Ctrl+B)" : L"WT tmux keys: OFF");
    Shell_NotifyIconW(message, &nid);
}

void ShowTrayMenu(HWND hwnd) {
    HMENU menu = CreatePopupMenu();
    AppendMenuW(menu, MF_STRING | (g_enabled ? MF_CHECKED : 0), kCmdToggle, L"Enable tmux keys (Ctrl+B)");
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING, kCmdExit, L"Exit");
    POINT pt;
    GetCursorPos(&pt);
    SetForegroundWindow(hwnd);
    TrackPopupMenu(menu, TPM_RIGHTBUTTON, pt.x, pt.y, 0, hwnd, nullptr);
    PostMessageW(hwnd, WM_NULL, 0, 0);
    DestroyMenu(menu);
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    if (msg == g_taskbarCreated && g_taskbarCreated != 0) {
        SetTray(NIM_ADD);  // Explorer restarted; re-register the tray icon
        return 0;
    }
    switch (msg) {
    case kTrayMsg:
        if (lp == WM_RBUTTONUP || lp == WM_CONTEXTMENU) ShowTrayMenu(hwnd);
        return 0;
    case WM_COMMAND:
        if (LOWORD(wp) == kCmdToggle) {
            g_enabled = !g_enabled;
            g_prefixActive = false;
            ZeroMemory(g_swallowUp, sizeof(g_swallowUp));
            SetTray(NIM_MODIFY);
        } else if (LOWORD(wp) == kCmdExit) {
            DestroyWindow(hwnd);
        }
        return 0;
    case WM_DESTROY:
        SetTray(NIM_DELETE);
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

int WINAPI wWinMain(HINSTANCE inst, HINSTANCE, LPWSTR, int) {
    HANDLE mutex = CreateMutexW(nullptr, TRUE, kMutexName);
    if (GetLastError() == ERROR_ALREADY_EXISTS) return 0;

    WNDCLASSW wc = {};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = inst;
    wc.lpszClassName = kWindowClass;
    RegisterClassW(&wc);
    g_taskbarCreated = RegisterWindowMessageW(L"TaskbarCreated");

    // Hidden top-level window (not message-only) so it receives the TaskbarCreated broadcast.
    g_hwnd = CreateWindowExW(0, kWindowClass, L"", WS_POPUP, 0, 0, 0, 0, nullptr, nullptr, inst, nullptr);
    if (!g_hwnd) return 1;

    SetTray(NIM_ADD);
    g_hook = SetWindowsHookExW(WH_KEYBOARD_LL, KeyboardProc, inst, 0);
    if (!g_hook) {
        MessageBoxW(nullptr, L"Failed to install keyboard hook.", L"WT tmux", MB_ICONERROR);
        SetTray(NIM_DELETE);
        DestroyWindow(g_hwnd);
        return 1;
    }

    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    UnhookWindowsHookEx(g_hook);
    ReleaseMutex(mutex);
    CloseHandle(mutex);
    return 0;
}
