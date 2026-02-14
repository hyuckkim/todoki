#include "util.h"
#include "engine_log.h"
#include "engine_lua.h"
#include "engine_graphic.h"
#include "entry.h"

#include <unordered_set>
#pragma comment(lib, "winmm.lib")

ULONGLONG lastTick = 0;

bool needReload = false;
bool g_isDrawing = false;

int g_targetFPS = 60;
bool g_vSync = false;

LARGE_INTEGER g_frequency;
LARGE_INTEGER g_lastTime;

void Reload() {
    Call("Quit");
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

    // 현재 시간 구하기
    LARGE_INTEGER currentTime;
    QueryPerformanceCounter(&currentTime);

    // Delta Time 계산 (초 단위 혹은 밀리초 단위)
    // 밀리초(ms) 단위를 원하시면 1000.0을 곱합니다.
    double dt = (double)(currentTime.QuadPart - g_lastTime.QuadPart) * 1000.0 / g_frequency.QuadPart;
    g_lastTime = currentTime;

    UpdateMousePassthrough();
    Call("Update", dt); // 이제 Lua에 16.666... 같은 정밀한 값이 전달됩니다.

    PreDraw();
    Call("Draw");
    PostDraw();

    lua.step_gc(10);
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_DESTROY:
    case WM_QUERYENDSESSION:
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

    LARGE_INTEGER frequency;
    QueryPerformanceFrequency(&frequency);

    double targetFrameTime = 1000.0 / g_targetFPS;
    ShowWindow(g_hwnd, nCmdShow);

    QueryPerformanceFrequency(&g_frequency);
    QueryPerformanceCounter(&g_lastTime);

    RegisterLuaLibs();
    Call("Init");
    MSG msg;
    timeBeginPeriod(1); // Sleep의 최소 단위를 1ms로 강제

    while (true) {
        if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) break;
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        else {
            // 프레임 시작 시간 기록
            LARGE_INTEGER frameStart;
            QueryPerformanceCounter(&frameStart);

            drawing();
            FlushLogs();

            // --- 프레임 제어 로직 ---
            if (!g_vSync) {
                double targetMs = 1000.0 / g_targetFPS;

                LARGE_INTEGER frameEnd;
                QueryPerformanceCounter(&frameEnd);
                double elapsedMs = (double)(frameEnd.QuadPart - frameStart.QuadPart) * 1000.0 / g_frequency.QuadPart;

                if (elapsedMs < targetMs) {
                    // 남은 시간만큼 Sleep (정밀도를 위해 1ms 정도 여유를 두고 쉼)
                    DWORD sleepTime = (DWORD)(targetMs - elapsedMs);
                    if (sleepTime > 0) Sleep(sleepTime);
                }
            }
        }
    }
    timeEndPeriod(1);
    Call("Quit");
    lua.collect_garbage();
    lua = sol::state();
    unregisterLuaFunctions();
    ReleaseGraphic();
    return 0;
}