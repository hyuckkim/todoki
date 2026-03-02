#include "window.h"
#include <windows.h>
#include <windowsx.h>
#include <cstdio>
#include <ini.h>
#include <string>
#include <functional>
#include <algorithm>
#include "includesol.h"
#include <tuple>

static int IniHandler(
	void* user,
	const char* section,
	const char* name,
	const char* value) {
	auto* cfg = reinterpret_cast<WindowConfig*>(user);
	// Normalize keys to lowercase for consistency
	std::string keyName(name);
	std::transform(keyName.begin(), keyName.end(), keyName.begin(), ::tolower);
	cfg->data[section][keyName] = value;
	return 1;
}

void Window::BindToLuaInput(sol::state& lua, const char* name) {
    sol::table i = lua.create_named_table(name);

    // key state
    i["key"] = [](int vkey) -> bool {
        return (GetAsyncKeyState(vkey) & 0x8000) != 0;
    };

    // mouse info (x, y, left, right)
    i["mouse"] = [this]() {
        POINT pt;
        GetCursorPos(&pt);
        ScreenToClient(hwnd, &pt);

        bool left = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
        bool right = (GetAsyncKeyState(VK_RBUTTON) & 0x8000) != 0;

        return std::make_tuple(pt.x, pt.y, left, right);
    };

    i["pos"] = [this]() {
        RECT rc;
        GetWindowRect(hwnd, &rc);
        return std::make_tuple(rc.left, rc.top);
    };

    i["size"] = [this]() {
        RECT rc;
        GetWindowRect(hwnd, &rc);
        return std::make_tuple((int)(rc.right - rc.left), (int)(rc.bottom - rc.top));
    };

    i["workArea"] = []() {
        RECT rc;
        SystemParametersInfo(SPI_GETWORKAREA, 0, &rc, 0);
        return std::make_tuple((int)(rc.right - rc.left), (int)(rc.bottom - rc.top));
    };

    i["screenSize"] = []() {
        return std::make_tuple(GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN));
    };

    i["monitors"] = [](sol::this_state ts) {
        sol::state_view lua(ts);
        sol::table monitorList = lua.create_table();

        auto callback = [](HMONITOR hMonitor, HDC hdcMonitor, LPRECT lprcMonitor, LPARAM dwData) -> BOOL {
            auto& list = *reinterpret_cast<sol::table*>(dwData);

            MONITORINFO mi = { sizeof(mi) };
            if (GetMonitorInfo(hMonitor, &mi)) {
                sol::state_view lua = list.lua_state();
                sol::table info = lua.create_table();

                info["x"] = mi.rcMonitor.left;
                info["y"] = mi.rcMonitor.top;
                info["w"] = mi.rcMonitor.right - mi.rcMonitor.left;
                info["h"] = mi.rcMonitor.bottom - mi.rcMonitor.top;

                info["workX"] = mi.rcWork.left;
                info["workY"] = mi.rcWork.top;
                info["workW"] = mi.rcWork.right - mi.rcWork.left;
                info["workH"] = mi.rcWork.bottom - mi.rcWork.top;

                list.add(info);
            }
            return TRUE;
        };

        EnumDisplayMonitors(NULL, NULL, (MONITORENUMPROC)+callback, (LPARAM)&monitorList);

        return monitorList;
    };

    // fps/vsync info from window config (read-only)
    i["fpsMode"] = [this]() {
        return std::make_tuple(config.getFPS(), config.getVSync());
    };
    i["config"] = [this](sol::this_state ts) {
        sol::state_view lua(ts);
        sol::table root = lua.create_table();
        
        // Convert entire data map to nested Lua tables
        for (const auto& section : config.data) {
            sol::table sectionTable = lua.create_table();
            for (const auto& kv : section.second) {
                sectionTable[kv.first] = kv.second;
            }
            root[section.first] = sectionTable;
        }
        
        return root;
    };
    i["focus"] = [this]() -> bool {
        // 현재 윈도우 시스템에서 가장 앞에 나와 있는(포커스된) 창의 핸들을 가져옵니다.
        HWND foregroundHwnd = GetForegroundWindow();

        // 그 핸들이 현재 클래스가 받고 있는 hwnd와 일치하는지 반환합니다.
        return foregroundHwnd == hwnd;
        };
}

void Window::BindToLuaSys(sol::state& lua, const char* name) {
    sol::table s = lua.create_named_table(name);

    // set window size
    s["size"] = [this](int w, int h) {
        if (hwnd) {
            SetWindowPos(hwnd, NULL, 0, 0, w, h, SWP_NOMOVE | SWP_NOZORDER);
            config.data["Window"]["width"] = std::to_string(w);
            config.data["Window"]["height"] = std::to_string(h);
            if (sizeCallback) sizeCallback(w, h);
        }
    };

    // set window position
    s["pos"] = [this](int x, int y) {
        if (hwnd) {
            SetWindowPos(hwnd, NULL, x, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
        }
    };

    // cursor show/hide
    s["showCursor"] = [](bool show) {
        // ShowCursor uses a display counter, not a simple on/off state
        // Call it repeatedly until the cursor reaches the desired visibility
        if (show) {
            while (ShowCursor(TRUE) < 0);  // counter >= 0 means visible
        } else {
            while (ShowCursor(FALSE) >= 0); // counter < 0 means hidden
        }
    };

    // set cursor by resource id/name
    s["cursor"] = [](sol::optional<int> type) {
        HCURSOR hCursor = LoadCursor(NULL, MAKEINTRESOURCE(type.value_or(32512)));
        SetCursor(hCursor);
    };
    s["clip"] = [this](bool clip) {
        if (clip && hwnd) {
            RECT rect;
            GetClientRect(hwnd, &rect);
            ClientToScreen(hwnd, (LPPOINT)&rect.left);
            ClientToScreen(hwnd, (LPPOINT)&rect.right);
            ClipCursor(&rect);
        }
        else {
            ClipCursor(NULL);
        }
        };
    // topmost
    s["topmost"] = [this](bool topmost) {
        if (hwnd) {
            HWND hWndInsertAfter = topmost ? HWND_TOPMOST : HWND_NOTOPMOST;
            SetWindowPos(hwnd, hWndInsertAfter, 0, 0, 0, 0,
                SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
        }
    };

    // open URL
    s["openURL"] = [](const std::string& url) {
        if (!url.empty()) {
            ShellExecuteA(NULL, "open", url.c_str(), NULL, NULL, SW_SHOWNORMAL);
        }
    };

    // quit
    s["quit"] = []() {
        PostQuitMessage(0);
    };
}
WindowConfig Window::LoadConfig(const wchar_t* path) {
	WindowConfig cfg;
	int size = WideCharToMultiByte(CP_UTF8, 0, path, -1, nullptr, 0, nullptr, nullptr);
	if (size <= 0) return cfg; std::string utf8path(size, '\0');
	WideCharToMultiByte(CP_UTF8, 0, path, -1, &utf8path[0], size, nullptr, nullptr);
	utf8path.pop_back(); ini_parse(utf8path.c_str(), IniHandler, &cfg);
	return cfg;
}
LRESULT CALLBACK Window::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    Window* pThis = nullptr;

    if (msg == WM_NCCREATE) {
        CREATESTRUCT* pCreate = reinterpret_cast<CREATESTRUCT*>(lParam);
        pThis = reinterpret_cast<Window*>(pCreate->lpCreateParams);
        SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)pThis);
    }
    else {
        pThis = reinterpret_cast<Window*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
    }
    // Handle hit testing for click-through behavior if callback provided
    if (msg == WM_NCHITTEST && pThis && pThis->hitTestCallback) {
        // lParam contains screen coordinates
        POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
        // convert to client coordinates
        POINT client = pt;
        ScreenToClient(hwnd, &client);
        bool hit = false;
        try {
            hit = pThis->hitTestCallback((int)client.x, (int)client.y);
        }
        catch (...) { hit = true; }


        if (!hit) {
            return HTTRANSPARENT;
        }
        else {
            return HTCLIENT;
        }
    }

    // Debug mouse message logging (show messages actually received by the window)
    if (pThis) {
        switch (msg) {
        case WM_MOUSEMOVE: {

        } break;
        case WM_LBUTTONDOWN: {

        } break;
        case WM_LBUTTONUP: {

        } break;
        default: break;
        }

        // WM_INPUT should be dispatched conditionally because
        // raw-input registration may be controlled by config.alwaysReactive.
        if (msg != WM_INPUT) {
            if (pThis->messageCallback) {
                pThis->messageCallback(msg, wParam, lParam);
            }
        }
    }

	switch (msg) {
	case WM_INPUT: {
		// Always forward WM_INPUT; alwaysReactive only controls RIDEV_INPUTSINK flag
		// which determines whether input is received when window is not focused
		if (pThis && pThis->messageCallback) {
			pThis->messageCallback(msg, wParam, lParam);
		}
		break;
	}
	case WM_DESTROY:
		PostQuitMessage(0);
		return 0;
	}
	return DefWindowProc(hwnd, msg, wParam, lParam);
}
bool Window::Create(HINSTANCE hInstance,
    int nCmdShow,
    const WindowConfig& cfg)
{
    config = cfg;

    WNDCLASS wc = {};
    wc.lpfnWndProc = Window::WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = L"DCompWindow";
    RegisterClass(&wc);

    DWORD style = 0;
    DWORD exStyle = 0;

    if (cfg.getTransparent())
    {
        // Use layered window for per-pixel alpha and reliable click-through handling
        style = WS_POPUP;
        exStyle = WS_EX_LAYERED | WS_EX_NOREDIRECTIONBITMAP;
    }
    else
    {
        // Remove maximize button from title bar
        style = WS_OVERLAPPEDWINDOW & ~WS_MAXIMIZEBOX;
        exStyle = 0;
    }

    hwnd = CreateWindowEx(
        exStyle,
        wc.lpszClassName,
        cfg.getTitle().c_str(),
        style,
        cfg.getPosX(), 
        cfg.getPosY(),
        cfg.getWidth(),
        cfg.getHeight(),
        nullptr, nullptr,
        hInstance,
        this);

    if (!hwnd)
        return false;

    // For layered windows, ensure layered attributes are set (opaque by default)
    if (cfg.getTransparent()) {
        // Set full opacity; actual per-pixel alpha can be applied with UpdateLayeredWindow later
        SetLayeredWindowAttributes(hwnd, 0, 255, LWA_ALPHA);
    }

    RAWINPUTDEVICE rid;

    rid.usUsagePage = 0x01;          // 일반 데스크톱 장치 그룹
    rid.usUsage = 0x02;              // 마우스
    // If alwaysReactive is set, register as input sink so we receive raw input
    // even when the window is not focused. Otherwise register normally.
    rid.dwFlags = cfg.getAlwaysReactive() ? RIDEV_INPUTSINK : 0;
    rid.hwndTarget = hwnd;           // 메시지를 받을 윈도우 핸들

    if (!RegisterRawInputDevices(&rid, 1, sizeof(rid))) {
        // 등록 실패 처리 (필요시)
        printf("Raw Input 등록 실패!");
    }

    // Apply topmost if requested
    if (cfg.getAlwaysTop()) {
        SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0,
            SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
    }

    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    return true;
}
void Window::RunGameLoop(std::function<void(double dtMs)> tick) {
    MSG msg;
    LARGE_INTEGER freq, lastTime;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&lastTime);

    while (true) {
        LARGE_INTEGER currentTime;
        QueryPerformanceCounter(&currentTime);

        double dtMs = (double)(currentTime.QuadPart - lastTime.QuadPart) * 1000.0 / freq.QuadPart;

        if (dtMs > 0) {
            lastTime = currentTime;

            while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
                if (msg.message == WM_QUIT) return;
                TranslateMessage(&msg);
                DispatchMessage(&msg);
            }

            if (tick) {
                tick(dtMs);
            }

            if (!config.getVSync()) {
                double targetMs = 1000.0 / config.getFPS();
                LARGE_INTEGER frameEnd;
                QueryPerformanceCounter(&frameEnd);
                double elapsedMs = (double)(frameEnd.QuadPart - currentTime.QuadPart) * 1000.0 / freq.QuadPart;
                if (elapsedMs < targetMs) {
                    Sleep((DWORD)(targetMs - elapsedMs));
                }
            }
        }
    }
}