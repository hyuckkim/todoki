#include <windows.h>
#include "window.h"
#include "luacargo.h"
#include "graphicengine.h"

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
#ifdef _DEBUG
    if (AllocConsole()) {
        SetConsoleOutputCP(CP_UTF8);
        FILE* fp;
        freopen_s(&fp, "CONOUT$", "w", stdout);
        freopen_s(&fp, "CONOUT$", "w", stderr);
        printf("Debug Console Opened\n");
    }
#endif

    Window window;
    WindowConfig cfg = Window::LoadConfig(L"config.ini");
    if (!window.Create(hInstance, nCmdShow, cfg)) {
        return 1;
    }

    GraphicEngine engine(window.GetHandle(), cfg.width, cfg.height);
    engine.Init();

#define BIND(obj, func, name) \
    LuaBinding([&](sol::state& s, const char* n) { (obj).func(s, n); }, name)

    std::vector<LuaBinding> systems = {
        BIND(engine, BindToLua, "g"),
    };
#undef BIND

    LuaCargo lua;
    lua.Init("main.lua", systems);

     
    lua.Call("Init");
    window.RunGameLoop([&](double dtMs) {
        engine.PreDraw();
		lua.Call("Update", dtMs);
		lua.Call("Draw");
        engine.PostDraw();
    });
    engine.Release();

    return 0;
}