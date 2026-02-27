#include <windows.h>
#include "window.h"


int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
	Window window;
	WindowConfig cfg = Window::LoadConfig(L"config.ini");
	if (!window.Create(hInstance, nCmdShow, cfg)) {
		return 1;
	}

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