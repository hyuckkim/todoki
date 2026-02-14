#include "util.h"
#include "engine_log.h"
#include "engine_lua.h"
#include "engine_graphic.h"
#include "engine_sound.h"
#include "entry.h"

#include <unordered_set>
#include "packManager.cpp"
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

void drawing(LARGE_INTEGER currentTime) {
    if (needReload) {
        Reload();
        needReload = false;
    }

    // Delta Time 계산
    double dt = (double)(currentTime.QuadPart - g_lastTime.QuadPart) * 1000.0 / g_frequency.QuadPart;
    g_lastTime = currentTime;

    // 만약 dt가 너무 크게 튀는 것을 방지 (예: 창 드래그 시)
    if (dt > 100.0) dt = 16.66;

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

    PackManager::Instance().Init("resources.pak", "assets/");
    HRESULT res = ReadEnteryFromArgs();
    if (res != S_OK) {
        return -1;
    }
    ResetLuaLogState();
    EngineSound::Instance().Initialize();

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
        // 1. 프레임 시작 시간은 루프의 가장 꼭대기에서!
        LARGE_INTEGER frameStart;
        QueryPerformanceCounter(&frameStart);

        if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) break;
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        else {
            // 2. 그리기 및 로직 실행
            drawing(frameStart);
            FlushLogs();

            // 3. 프레임 제어
            if (!g_vSync) {
                double targetMs = 1000.0 / g_targetFPS;

                LARGE_INTEGER frameEnd;
                QueryPerformanceCounter(&frameEnd);
                double elapsedMs = (double)(frameEnd.QuadPart - frameStart.QuadPart) * 1000.0 / g_frequency.QuadPart;

                if (elapsedMs < targetMs) {
                    // 루프 없이 그냥 한 번에 쉽니다. 
                    // 윈도우 스케줄러가 스레드를 완전히 잠재우게 하여 전력을 아낍니다.
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
    EngineSound::Instance().Shutdown();
    return 0;
}