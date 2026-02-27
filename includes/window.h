#pragma once
#include <windows.h>
#include <string>

struct WindowConfig {
    int width = 800;
    int height = 600;
    bool fullscreen = false;
	bool transparent = false;
	std::wstring title = L"Window";
};

class Window {
public:
    static WindowConfig LoadConfig(const wchar_t* path);

    bool Create(HINSTANCE hInstance,
        int nCmdShow,
        const WindowConfig& cfg);

private:
    static LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
    HWND hwnd = nullptr;
    WindowConfig config;
};