#include "util.h"
#include "engine_log.h"
#include "engine_lua.h"
#include "engine_graphic.h"
#include "entry.h"

#include <unordered_set>

ULONGLONG lastTick = 0;

bool needReload = false;
bool g_isDrawing = false;


void Reload() {
    printf("[Win] Reloading Script...\n");

    lua.collect_garbage();
    unregisterLuaFunctions();

    ReportDXGILiveObjects();

    InitLuaEngine(GetEntryFile());
    RegisterLuaLibs();

    Call("Init");
    needReload = false;
}

void drawing() {
    if (needReload) {
        Reload();
        needReload = false;
    }
    ULONGLONG now = GetTickCount64();
    double dt = double(now - lastTick);
    lastTick = now;
    UpdateMousePassthrough();
    Call("Update", dt);

    PreDraw();
    Call("Draw");
    PostDraw();

    lua.step_gc(10);
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_DESTROY:
        PostQuitMessage(0);
        break;
    case WM_KEYDOWN:
        Call("OnKeyDown", (int)wParam);

#ifdef _DEBUG
        if (wParam == VK_F5) {
            needReload = true;
        }
#endif
        break;
    case WM_KEYUP:
        Call("OnKeyUp", (int)wParam);
        break;

    case WM_LBUTTONDOWN:
        Call("OnMouseDown", (int)LOWORD(lParam), (int)HIWORD(lParam));
        break;

    case WM_LBUTTONUP:
        Call("OnMouseUp", (int)LOWORD(lParam), (int)HIWORD(lParam));
        break;

    case WM_RBUTTONDOWN:
        Call("OnRightMouseDown", (int)LOWORD(lParam), (int)HIWORD(lParam));
        break;

    case WM_RBUTTONUP:
        Call("OnRightMouseUp", (int)LOWORD(lParam), (int)HIWORD(lParam));
        break;
    case WM_ACTIVATE:
        if ((BOOL)wParam) Call("OnActive");
        else Call("OnInactive");
        break;

    default:
        return DefWindowProc(hwnd, msg, wParam, lParam);
    }
    return 0;
}

int APIENTRY wWinMain(
    _In_ HINSTANCE hInstance,
    _In_opt_ HINSTANCE hPrevInstance,
    _In_ LPWSTR    lpCmdLine,
    _In_ int       nCmdShow) {
    InitCom();
#ifdef _DEBUG
    if (AllocConsole()) {
        SetConsoleOutputCP(CP_UTF8);
        FILE* fp;
        freopen_s(&fp, "CONOUT$", "w", stdout);
        freopen_s(&fp, "CONOUT$", "w", stderr);
        printf("Debug Console Opened\n");
    }
#endif

    HRESULT res = ReadEnteryFromArgs();
    if (res != S_OK) {
        return -1;
    }
    ResetLuaLogState();
    InitLuaEngine(GetEntryFile());
    LuaConfig config = LoadLuaConfig();

    gDrawW = config.width;
    gDrawH = config.height;
    std::wstring titleW = to_wstring(config.title);

    InitWindow(WndProc, hInstance, titleW.c_str());
    InitD2D();

    const int TARGET_FPS = 60;
    const int FRAME_DELAY = 1000 / TARGET_FPS;

    lastTick = GetTickCount64();
    ShowWindow(g_hwnd, nCmdShow);

    RegisterLuaLibs();
    Call("Init");
    MSG msg;
    while (true) {
        ULONGLONG frameStart = GetTickCount64();
        if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) break;

            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        else {
            drawing();
            FlushLogs();

            // 프레임 제어
            ULONGLONG frameTime = GetTickCount64() - frameStart;
            if (frameTime < FRAME_DELAY) {
                Sleep(FRAME_DELAY - (DWORD)frameTime);
            }
        }
    }
    lua.collect_garbage();
    lua = sol::state();
    unregisterLuaFunctions();
    ReleaseGraphic();
    return 0;
}