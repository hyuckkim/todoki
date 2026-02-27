#include "window.h"
#include <windows.h>
#include <ini.h>
#include <string>
#include <functional>

static int IniHandler(
	void* user,
	const char* section,
	const char* name,
	const char* value) {
	auto* cfg = reinterpret_cast<WindowConfig*>(user);
	if (std::string(section) == "Window") {
		if (std::string(name) == "Width")
			cfg->width = std::stoi(value);
		else if (std::string(name) == "Height")
			cfg->height = std::stoi(value);
		else if (std::string(name) == "Fullscreen")
			cfg->fullscreen = (std::stoi(value) != 0);
		else if (std::string(name) == "Transparent")
			cfg->transparent = (std::stoi(value) != 0);
		else if (std::string(name) == "Title")
			cfg->title = std::wstring(value, value + strlen(value));
		else if (std::string(name) == "VSync")
            cfg->vSync = (std::stoi(value) != 0);
        else if (std::string(name) == "FPS")
			cfg->fps = std::stoi(value);
	} return 1;
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
	switch (msg) {
	case WM_DESTROY:
	case WM_QUERYENDSESSION:
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

    if (cfg.transparent)
    {
        style = WS_POPUP;
        exStyle = WS_EX_NOREDIRECTIONBITMAP;
    }
    else
    {
        style = WS_OVERLAPPEDWINDOW;
        exStyle = 0;
    }

    hwnd = CreateWindowEx(
        exStyle,
        wc.lpszClassName,
        cfg.title.c_str(),
        style,
        200, 200,
        cfg.width,
        cfg.height,
        nullptr, nullptr,
        hInstance,
        nullptr);

    if (!hwnd)
        return false;

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

            if (!config.vSync) {
                double targetMs = 1000.0 / config.fps;
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