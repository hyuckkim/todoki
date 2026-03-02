#pragma once
#include <windows.h>
#include <string>
#include <functional>
#include <map>
#include "includesol.h"

struct WindowConfig {
    // Section -> Key -> Value mapping (all stored as strings)
    std::map<std::string, std::map<std::string, std::string>> data;
    
    WindowConfig() {
        // Set default values in Window section (all lowercase keys)
        data["Window"]["width"] = "800";
        data["Window"]["height"] = "600";
        data["Window"]["transparent"] = "0";
        data["Window"]["title"] = "Window";
        data["Window"]["vSync"] = "0";
        data["Window"]["fps"] = "60";
        data["Window"]["alwaysTop"] = "0";
        data["Window"]["alwaysReactive"] = "0";
        data["Window"]["posX"] = "200";
        data["Window"]["posY"] = "200";
    }
    
    // Helper methods for backward compatibility with existing code
    int getWidth() const { 
        auto it = data.find("Window");
        if (it != data.end()) {
            auto v = it->second.find("width");
            if (v != it->second.end()) return std::stoi(v->second);
        }
        return 800;
    }
    
    int getHeight() const {
        auto it = data.find("Window");
        if (it != data.end()) {
            auto v = it->second.find("height");
            if (v != it->second.end()) return std::stoi(v->second);
        }
        return 600;
    }
    
    bool getTransparent() const {
        auto it = data.find("Window");
        if (it != data.end()) {
            auto v = it->second.find("transparent");
            if (v != it->second.end()) return std::stoi(v->second) != 0;
        }
        return false;
    }
    
    bool getAlwaysTop() const {
        auto it = data.find("Window");
        if (it != data.end()) {
            auto v = it->second.find("alwaysTop");
            if (v != it->second.end()) return std::stoi(v->second) != 0;
        }
        return false;
    }
    
    bool getAlwaysReactive() const {
        auto it = data.find("Window");
        if (it != data.end()) {
            auto v = it->second.find("alwaysReactive");
            if (v != it->second.end()) return std::stoi(v->second) != 0;
        }
        return false;
    }
    
    std::wstring getTitle() const {
        auto it = data.find("Window");
        if (it != data.end()) {
            auto v = it->second.find("title");
            if (v != it->second.end()) {
                std::string s = v->second;
                return std::wstring(s.begin(), s.end());
            }
        }
        return L"Window";
    }
    
    bool getVSync() const {
        auto it = data.find("Window");
        if (it != data.end()) {
            auto v = it->second.find("vSync");
            if (v != it->second.end()) return std::stoi(v->second) != 0;
        }
        return false;
    }
    
    int getFPS() const {
        auto it = data.find("Window");
        if (it != data.end()) {
            auto v = it->second.find("fps");
            if (v != it->second.end()) return std::stoi(v->second);
        }
        return 60;
    }
    
    int getPosX() const {
        auto it = data.find("Window");
        if (it != data.end()) {
            auto v = it->second.find("posX");
            if (v != it->second.end()) return std::stoi(v->second);
        }
        return 200;
    }
    
    int getPosY() const {
        auto it = data.find("Window");
        if (it != data.end()) {
            auto v = it->second.find("posY");
            if (v != it->second.end()) return std::stoi(v->second);
        }
        return 200;
    }
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