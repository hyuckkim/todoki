#pragma once
#include <windows.h>
#include <string>
#include <functional>
#include "includesol.h"

struct WindowConfig {
    int width = 800;
    int height = 600;
    bool fullscreen = false;
	bool transparent = false;
	std::wstring title = L"Window";
    bool vSync = false;
    int fps = 60;
    int posX = 200;
    int posY = 200;
};

class Window {
public:
    static WindowConfig LoadConfig(const wchar_t* path);

    bool Create(HINSTANCE hInstance,
        int nCmdShow,
        const WindowConfig& cfg);
    void RunGameLoop(std::function<void(double dtMs)> onUpdate);
	HWND GetHandle() const { return hwnd; }
    const WindowConfig& GetConfig() const { return config; }

    using MessageCallback = std::function<void(UINT msg, WPARAM wp, LPARAM lp)>;
    void SetMessageCallback(MessageCallback cb) { messageCallback = cb; }
    using HitTestCallback = std::function<bool(int x, int y)>;
    void SetHitTestCallback(HitTestCallback cb) { hitTestCallback = cb; }
    using SizeCallback = std::function<void(int w, int h)>;
    void SetSizeCallback(SizeCallback cb) { sizeCallback = cb; }
    // Bind read-only input/state into Lua (name e.g. "is")
    void BindToLuaInput(sol::state& lua, const char* name);
    // Bind controllable window/system functions into Lua (name e.g. "sys")
    void BindToLuaSys(sol::state& lua, const char* name);

private:
    MessageCallback messageCallback;
    HitTestCallback hitTestCallback;
    SizeCallback sizeCallback;
    static LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
    HWND hwnd = nullptr;
    WindowConfig config;
};