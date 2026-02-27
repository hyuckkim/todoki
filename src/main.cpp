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

    LuaCargo lua;
	lua.Init("main.lua");

    GraphicEngine engine(window.GetHandle(), cfg.width, cfg.height);
    engine.Init();

    window.RunGameLoop();
    engine.Release();

    return 0;
}