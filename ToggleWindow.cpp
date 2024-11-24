#include <windows.h>
#include <string>
#include <vector>
#include <iostream>
#include <algorithm>
#include <limits>
#include <unordered_map>
#include <cwctype>

struct WindowInfo {
    HWND hwnd;
    std::wstring title;
};

std::vector<WindowInfo> EnumerateWindows() {
    std::vector<WindowInfo> windowList;

    EnumWindows([](HWND hwnd, LPARAM lParam) -> BOOL {
        if (!IsWindowVisible(hwnd)) {
            return TRUE; // Skip invisible windows
        }

        int length = GetWindowTextLength(hwnd);
        if (length == 0) {
            return TRUE; // Skip windows without titles
        }

        std::wstring windowTitle(length + 1, L'\0');
        int copiedLength = GetWindowTextW(hwnd, &windowTitle[0], length + 1);
        windowTitle.resize(copiedLength); // Resize to actual text length

        // Trim whitespace from window title
        windowTitle.erase(windowTitle.begin(), std::find_if_not(windowTitle.begin(), windowTitle.end(), [](wchar_t ch) {
            return iswspace(ch);
            }));
        windowTitle.erase(std::find_if_not(windowTitle.rbegin(), windowTitle.rend(), [](wchar_t ch) {
            return iswspace(ch);
            }).base(), windowTitle.end());

        if (!windowTitle.empty()) {
            auto& windowList = *reinterpret_cast<std::vector<WindowInfo>*>(lParam);
            windowList.push_back({ hwnd, windowTitle });
        }

        return TRUE;
        }, reinterpret_cast<LPARAM>(&windowList));

    return windowList;
}

HWND SearchAndSelectWindow() {
    auto windows = EnumerateWindows();

    std::wcout << L"Enter part of the window title to search: ";
    std::wstring searchTerm;
    std::getline(std::wcin, searchTerm);

    std::transform(searchTerm.begin(), searchTerm.end(), searchTerm.begin(), towlower);

    std::vector<WindowInfo> matchingWindows;

    for (const auto& win : windows) {
        std::wstring titleLower = win.title;
        std::transform(titleLower.begin(), titleLower.end(), titleLower.begin(), towlower);

        if (titleLower.find(searchTerm) != std::wstring::npos) {
            matchingWindows.push_back(win);
        }
    }

    if (matchingWindows.empty()) {
        std::wcout << L"No windows found matching that title.\n";
        exit(1);
    }

    std::wcout << L"Matching windows:\n";
    for (size_t i = 0; i < matchingWindows.size(); ++i) {
        std::wcout << i + 1 << L": " << matchingWindows[i].title << L"\n";
    }

    size_t selection = 0;
    while (true) {
        std::wcout << L"Select the window number: ";
        std::wstring line;
        std::getline(std::wcin, line);
        try {
            selection = std::stoul(line);
            if (selection >= 1 && selection <= matchingWindows.size()) {
                break;
            }
            else {
                std::wcout << L"Invalid selection. Please enter a number between 1 and " << matchingWindows.size() << L".\n";
            }
        }
        catch (const std::exception&) {
            std::wcout << L"Invalid input. Please enter a number.\n";
        }
    }

    HWND selectedWindow = matchingWindows[selection - 1].hwnd;
    std::wcout << L"Selected window: " << matchingWindows[selection - 1].title << L"\n";

    return selectedWindow;
}

UINT GetModifierKey(const std::wstring& modStr) {
    static const std::unordered_map<std::wstring, UINT> modMap = {
        { L"Alt", MOD_ALT },
        { L"Ctrl", MOD_CONTROL },
        { L"Shift", MOD_SHIFT },
        { L"Win", MOD_WIN },
    };
    auto it = modMap.find(modStr);
    if (it != modMap.end()) {
        return it->second;
    }
    return 0;
}

UINT GetVirtualKey(const std::wstring& keyStr) {
    static const std::unordered_map<std::wstring, UINT> keyMap = {
        { L"Backspace", VK_BACK },
        { L"Tab", VK_TAB },
        { L"Enter", VK_RETURN },
        { L"Esc", VK_ESCAPE },
        { L"Space", VK_SPACE },
        { L"Left", VK_LEFT },
        { L"Up", VK_UP },
        { L"Right", VK_RIGHT },
        { L"Down", VK_DOWN },
        { L"Insert", VK_INSERT },
        { L"Delete", VK_DELETE },
        { L"Home", VK_HOME },
        { L"End", VK_END },
        { L"PageUp", VK_PRIOR },
        { L"PageDown", VK_NEXT },
        // Add more keys as needed
    };

    if (keyStr.length() == 1) {
        wchar_t ch = towupper(keyStr[0]);
        SHORT vkCode = VkKeyScan(ch);
        if (vkCode != -1) {
            return vkCode & 0xFF;
        }
    }

    auto it = keyMap.find(keyStr);
    if (it != keyMap.end()) {
        return it->second;
    }

    if (keyStr.length() > 1 && keyStr[0] == L'F') {
        try {
            int fKeyNum = std::stoi(keyStr.substr(1));
            if (fKeyNum >= 1 && fKeyNum <= 24) {
                return VK_F1 + fKeyNum - 1;
            }
        }
        catch (...) {
            // Invalid function key number
        }
    }

    return 0;
}

class HotkeyManager {
public:
    ~HotkeyManager() {
        UnregisterAll();
    }

    bool Register(UINT id, UINT fsModifiers, UINT vk) {
        if (::RegisterHotKey(nullptr, id, fsModifiers, vk)) {
            hotkeyIds.push_back(id);
            return true;
        }
        else {
            return false;
        }
    }

    void UnregisterAll() {
        for (int id : hotkeyIds) {
            ::UnregisterHotKey(nullptr, id);
        }
        hotkeyIds.clear();
    }

private:
    std::vector<int> hotkeyIds;
};

void MessageLoop(HWND targetWindow) {
    MSG msg = { 0 };
    std::wcout << L"Listening for hotkeys... Press Ctrl+C to exit.\n";
    while (GetMessage(&msg, nullptr, 0, 0)) {
        if (msg.message == WM_HOTKEY) {
            if (IsWindowVisible(targetWindow)) {
                ShowWindow(targetWindow, SW_HIDE);
            }
            else {
                ShowWindow(targetWindow, SW_SHOW);
                SetForegroundWindow(targetWindow);
            }
        }
    }
}

BOOL WINAPI ConsoleHandler(DWORD signal) {
    if (signal == CTRL_C_EVENT) {
        PostQuitMessage(0);
        return TRUE;
    }
    return FALSE;
}

int wmain() {
    // Set console control handler to handle Ctrl+C
    SetConsoleCtrlHandler(ConsoleHandler, TRUE);

    HWND targetWindow = SearchAndSelectWindow();
    if (!IsWindow(targetWindow)) {
        std::wcout << L"Invalid window handle.\n";
        return 1;
    }

    HotkeyManager hotkeyManager;
    int hotkeyId = 1;

    std::wcout << L"Enter key combinations to toggle the window (e.g., Ctrl+Shift+M).\n";
    std::wcout << L"Enter 'done' when finished.\n";

    while (true) {
        std::wcout << L"Enter key combination: ";
        std::wstring input;
        std::getline(std::wcin, input);
        if (input == L"done") {
            break;
        }

        // Remove spaces
        input.erase(std::remove_if(input.begin(), input.end(), [](wchar_t ch) { return iswspace(ch); }), input.end());

        UINT fsModifiers = 0;
        UINT vk = 0;

        size_t start = 0;
        size_t pos = 0;
        bool valid = true;
        std::vector<std::wstring> tokens;

        while ((pos = input.find(L'+', start)) != std::wstring::npos) {
            tokens.push_back(input.substr(start, pos - start));
            start = pos + 1;
        }
        tokens.push_back(input.substr(start));

        for (size_t i = 0; i < tokens.size(); ++i) {
            const std::wstring& token = tokens[i];
            UINT mod = GetModifierKey(token);
            if (mod != 0) {
                fsModifiers |= mod;
            }
            else if (vk == 0) {
                vk = GetVirtualKey(token);
                if (vk == 0) {
                    std::wcout << L"Invalid key or modifier: " << token << L"\n";
                    valid = false;
                    break;
                }
            }
            else {
                std::wcout << L"Multiple keys specified: " << token << L"\n";
                valid = false;
                break;
            }
        }

        if (!valid || vk == 0) {
            std::wcout << L"Invalid key combination. Please try again.\n";
            continue;
        }

        if (hotkeyManager.Register(hotkeyId++, fsModifiers, vk)) {
            std::wcout << L"Hotkey registered.\n";
        }
        else {
            std::wcout << L"Failed to register hotkey. It might be already in use.\n";
        }
    }

    if (hotkeyId == 1) {
        std::wcout << L"No hotkeys registered, exiting.\n";
        return 1;
    }

    MessageLoop(targetWindow);

    return 0;
}