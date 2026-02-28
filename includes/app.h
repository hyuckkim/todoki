#pragma once
#include "window.h"
#include "graphicengine.h"
#include "luacargo.h"
#include <vector>

class App {
public:
    App();
    ~App();

    bool Init(HINSTANCE hInstance, int nCmdShow);
    void Run();

private:
    Window m_window;
    GraphicEngine m_engine;
    LuaCargo m_lua;

    bool m_needReload = false;
	std::vector<LuaBinding> m_systems;

    void SetupBindings();
    void SetupCallbacks();
    void Reload();
};