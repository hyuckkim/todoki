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

	bool needReload = false;
    window.SetMessageCallback([&](UINT msg, WPARAM wParam, LPARAM lParam) {
        switch (msg) {
        case WM_KEYDOWN:
            lua.Call("OnKeyDown", (int)wParam);
#ifdef _DEBUG
            if (wParam == VK_F5) needReload = true;
#endif
            break;

        case WM_KEYUP:
            lua.Call("OnKeyUp", (int)wParam);
            break;

        case WM_MOUSEMOVE:
            lua.Call("OnMouseMove", (int)LOWORD(lParam), (int)HIWORD(lParam));
            break;

        case WM_LBUTTONDOWN:
            lua.Call("OnMouseDown", (int)LOWORD(lParam), (int)HIWORD(lParam));
            break;

        case WM_LBUTTONUP:
            lua.Call("OnMouseUp", (int)LOWORD(lParam), (int)HIWORD(lParam));
            break;

        case WM_RBUTTONDOWN:
            lua.Call("OnRightMouseDown", (int)LOWORD(lParam), (int)HIWORD(lParam));
            break;

        case WM_RBUTTONUP:
            lua.Call("OnRightMouseUp", (int)LOWORD(lParam), (int)HIWORD(lParam));
            break;

        case WM_MOUSEWHEEL:
            lua.Call("OnMouseWheel", (int)GET_WHEEL_DELTA_WPARAM(wParam));
            break;

        case WM_ACTIVATE:
            if (LOWORD(wParam) == WA_INACTIVE) {
                lua.Call("OnInactive");
            }
            else {
                lua.Call("OnActive");
            }
            break;
        }
        });

    window.RunGameLoop([&](double dtMs) {
        if (needReload) {
            // ResourceHub::Instance().Clear();

            lua.Init("main.lua", systems);
            lua.Call("Init");

            needReload = false;
            printf("Reload Complete!\n");
        }

        engine.PreDraw();
		lua.Call("Update", dtMs);
		lua.Call("Draw");
        engine.PostDraw();
    });
    engine.Release();

    return 0;
}