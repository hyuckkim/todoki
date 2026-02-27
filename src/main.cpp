#include <windows.h>
#include "window.h"
#include "luacargo.h"

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

    MSG msg;
    while (true) {
        if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT)
                break;
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        else {
            Sleep(17);
        }
    }

    return 0;
}