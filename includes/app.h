#pragma once
#include "window.h"
#include "graphicengine.h"
#include "luacargo.h"
#include "resourcehub.h"
#include "soundsystem.h"
#include <vector>

class App {
public:
    App();
    ~App();

    bool Init(HINSTANCE hInstance, int nCmdShow, const char* entryPath);
    void Run();

private:
    Window m_window;
    GraphicEngine m_engine;
    LuaCargo m_lua;
    SoundSystem m_sound;

    bool m_needReload = false;
	std::vector<LuaBinding> m_systems;
    std::string m_entryPath;

    void SetupBindings();
    void SetupCallbacks();
    void Reload();
    void UpdateMousePassthrough();
};