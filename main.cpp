#include "lua_engine.h"
#include "engine_log.h"
#include <filesystem>
#include <unordered_set>
namespace fs = std::filesystem;

sol::state lua;
ULONGLONG lastTick = 0;
int gDrawW = 0, gDrawH = 0;
static std::unordered_set<std::string> g_printedMessages;

ID2D1Factory* g_pD2DFactory = nullptr;
ID2D1DCRenderTarget* g_pDCRT = nullptr;
IDWriteFactory* g_pDWriteFactory = nullptr;
IWICImagingFactory* g_pWICFactory = nullptr;
HWND    g_hwnd = nullptr;
HDC     g_hdcScreen = nullptr;
HDC     g_hdcMem = nullptr;
HBITMAP g_hBmp = nullptr;
HBITMAP g_hBmpOld = nullptr;
int     g_bufW = 0;
int     g_bufH = 0;
std::string entryFile = "main.lua";

#define SOL_ALL_SAFETIES_ON 1

static std::string g_last_lua_error = "";
template<typename... Args>
void CallLuaFunc(sol::state& lua, const std::string& func_name, Args&&... args) {
    sol::protected_function f = lua[func_name];

    if (!f.valid()) {
        // 함수가 존재하지 않을 때의 처리 (선택 사항)
        return;
    }

    auto result = f(std::forward<Args>(args)...);

    if (!result.valid()) {
        try {
            sol::error err = result;
            std::string current_error = err.what();

            if (g_last_lua_error != current_error) {
                printf("[LUA ERROR] %s: %s\n", func_name.c_str(), current_error.c_str());
                g_last_lua_error = current_error;
            }
        } catch (const std::exception& e) {
            printf("[CRITICAL ERROR] Failed to parse Lua error: %s\n", e.what());
        } catch (...) {
            printf("[CRITICAL ERROR] Unknown exception during Lua error handling\n");
        }
    }
}

std::string to_string(const std::wstring& wstr) {
    if (wstr.empty()) return "";
    int size_needed = WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), NULL, 0, NULL, NULL);
    std::string strTo(size_needed, 0);
    WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), &strTo[0], size_needed, NULL, NULL);
    return strTo;
}

void InitLuaEngine(const char* main) {
    ResetLuaLogState();

    lua = sol::state();
    lua.open_libraries(
        sol::lib::base,
        sol::lib::package,
        sol::lib::table,
        sol::lib::string,
        sol::lib::math,
        sol::lib::debug,
        sol::lib::utf8,
        sol::lib::coroutine
    );
    BindLuaLogging(lua);

    register_sys(lua, "sys");
    register_input(lua, "is");
    register_draw(lua, "g");
    register_res(lua, "res");

    auto load_result = lua.script_file(main, sol::script_pass_on_error);
    if (!load_result.valid()) {
        sol::error err = load_result;
        printf("[LUA ERROR] %s\n", err.what());
        return;
    }
    printf("Lua Engine Initialized / Reloaded via sol2.\n");
}

void InitD2D() {
    D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &g_pD2DFactory);

    // DC 렌더 타겟의 속성 설정 (투명도 지원 필수)
    D2D1_RENDER_TARGET_PROPERTIES props = D2D1::RenderTargetProperties(
        D2D1_RENDER_TARGET_TYPE_DEFAULT,
        D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED),
        0, 0, D2D1_RENDER_TARGET_USAGE_NONE, D2D1_FEATURE_LEVEL_DEFAULT
    );

    // DCRT 생성 (실제 사용은 BindDC에서 함)
    g_pD2DFactory->CreateDCRenderTarget(&props, &g_pDCRT);
    RebuildAllBitmaps();
}
void refreshBackBuffer(int w, int h) {
    if (g_hBmp) {
        SelectObject(g_hdcMem, g_hBmpOld);
        DeleteObject(g_hBmp);
        g_hBmp = nullptr;
    }

    if (!g_hdcScreen)
        g_hdcScreen = GetDC(g_hwnd);

    if (!g_hdcMem)
        g_hdcMem = CreateCompatibleDC(g_hdcScreen);

    BITMAPINFO bmi = {};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = w;
    bmi.bmiHeader.biHeight = -h; // top-down
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    void* bits = nullptr;
    g_hBmp = CreateDIBSection(
        g_hdcScreen,
        &bmi,
        DIB_RGB_COLORS,
        &bits,
        nullptr,
        0
    );

    if (!g_hBmp) return;

    g_hBmpOld = (HBITMAP)SelectObject(g_hdcMem, g_hBmp);

    g_bufW = w;
    g_bufH = h;
    if (g_pDCRT) {
        RECT rc = { 0, 0, w, h };
        g_pDCRT->BindDC(g_hdcMem, &rc);
    }
}
void drawing() {
    ULONGLONG now = GetTickCount64();
    double dt = double(now - lastTick);
    lastTick = now;

    int w = gDrawW;
    int h = gDrawH;
    if (w <= 0 || h <= 0) return;

    // 1. 백버퍼 생성/재생성 (기존 로직 유지)
    if (!g_hBmp || w != g_bufW || h != g_bufH) {
        refreshBackBuffer(w, h);
    }
    g_pDCRT->BeginDraw();
    g_pDCRT->SetTransform(D2D1::Matrix3x2F::Identity());
    g_pDCRT->Clear(D2D1::ColorF(0, 0, 0, 0)); // GPU 가속 클리어


    // 3. Lua Update / Draw 호출
    CallLuaFunc(lua, "Update", dt);
    CallLuaFunc(lua, "Draw");

    HRESULT hr = g_pDCRT->EndDraw();
    if (hr != S_OK) {
        if (hr == D2DERR_RECREATE_TARGET) {
            SafeRelease(&g_pDCRT);
            InitD2D();
            g_bufW = 0;
            g_bufH = 0;
            return;
        }
    }

    // 5. 레이어드 윈도우 갱신 (기존 GDI 로직 그대로 사용)
    RECT winRc; GetWindowRect(g_hwnd, &winRc);
    POINT ptWinPos = { winRc.left, winRc.top };
    SIZE sizeWin = { w, h };
    POINT ptSrc = { 0, 0 };

    BLENDFUNCTION blend = { AC_SRC_OVER, 0, 255, AC_SRC_ALPHA };

    BOOL ok = UpdateLayeredWindow(
        g_hwnd, g_hdcScreen, &ptWinPos, &sizeWin,
        g_hdcMem, &ptSrc, 0, &blend, ULW_ALPHA
    );
    if (!ok) {
        DWORD err = GetLastError(); printf("UpdateLayeredWindow failed, error=%lu\n", err);
    }
}

bool needReload = false;
LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_DESTROY:
        PostQuitMessage(0);
        break;
    case WM_KEYDOWN:
        CallLuaFunc(lua, "OnKeyDown", (int)wParam);

#ifdef _DEBUG
        if (wParam == VK_F5) {
            needReload = true;
        }
#endif
        break;

    case WM_KEYUP:
        CallLuaFunc(lua, "OnKeyUp", (int)wParam);
        break;

    case WM_LBUTTONDOWN:
        CallLuaFunc(lua, "OnMouseDown", (int)LOWORD(lParam), (int)HIWORD(lParam));
        break;

    case WM_LBUTTONUP:
        CallLuaFunc(lua, "OnMouseUp", (int)LOWORD(lParam), (int)HIWORD(lParam));
        break;

    case WM_RBUTTONDOWN:
        CallLuaFunc(lua, "OnRightMouseDown", (int)LOWORD(lParam), (int)HIWORD(lParam));
        break;

    case WM_RBUTTONUP:
        CallLuaFunc(lua, "OnRightMouseUp", (int)LOWORD(lParam), (int)HIWORD(lParam));
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
    // GDI+ 초기화
    GdiplusStartupInput gdiplusStartupInput;
    ULONG_PTR gdiplusToken;
    GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, nullptr);

    HRESULT hr = CoInitialize(NULL); // WIC와 COM 사용을 위해 필수
    hr = DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory), reinterpret_cast<IUnknown**>(&g_pDWriteFactory));
    hr = CoCreateInstance(CLSID_WICImagingFactory, NULL, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&g_pWICFactory));
#ifdef _DEBUG
    if (AllocConsole()) {
        SetConsoleOutputCP(CP_UTF8);
        FILE* fp;
        freopen_s(&fp, "CONOUT$", "w", stdout);
        freopen_s(&fp, "CONOUT$", "w", stderr);
        printf("Debug Console Opened\n");
    }
#endif

    int argc;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);

    if (argv && argc > 1) {
        // 인자가 있다면 첫 번째 인자를 entryFile로 설정 (argv[0]은 실행파일 경로)
        entryFile = to_string(argv[1]);
        printf("[Engine] Entry script changed to: %s\n", entryFile.c_str());
    }

    if (!fs::exists(entryFile)) {
        printf("[Engine Error] Entry script '%s' not found!\n", entryFile.c_str());
        return -1;
    }

    InitD2D();
    InitLuaEngine(entryFile.c_str());

    // 2. 루아로부터 받아온 설정값으로 윈도우 생성 (g_L이 준비되었으므로 안전)
    gDrawW = lua.get_or("ScreenWidth", 800);
    gDrawH = lua.get_or("ScreenHeight", 600);
    std::string title = lua.get_or<std::string>("WindowTitle", "Default");
    std::wstring titleW = to_wstring(title);

    const int TARGET_FPS = 60;
    const int FRAME_DELAY = 1000 / TARGET_FPS;

    // 윈도우 클래스 등록 및 생성
    WNDCLASS wc = { 0 };
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = L"LayeredImageWindow";
    RegisterClass(&wc);

    g_hwnd = CreateWindowEx(WS_EX_LAYERED | WS_EX_TOPMOST, wc.lpszClassName, titleW.c_str(),
        WS_POPUP, 200, 200, gDrawW, gDrawH, nullptr, nullptr, hInstance, nullptr);
    g_hdcScreen = GetDC(g_hwnd);

    lastTick = GetTickCount64();
    ShowWindow(g_hwnd, nCmdShow);

    CallLuaFunc(lua, "Init");
    MSG msg;
    while (true) {
        ULONGLONG frameStart = GetTickCount64(); // 시작 시간 기록
        if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) break;

            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        else {
            drawing();
            FlushLogs();
            if (needReload) {
                printf("[Win] Reloading Script...\n");

                if (g_pDCRT) { g_pDCRT->Release(); g_pDCRT = nullptr; }

                InitD2D();
                g_bufW = 0;
                g_bufH = 0;

                InitLuaEngine(entryFile.c_str());
                CallLuaFunc(lua, "Init");
                needReload = false;
            }
            // 프레임 제어
            ULONGLONG frameTime = GetTickCount64() - frameStart;
            if (frameTime < FRAME_DELAY) {
                Sleep(FRAME_DELAY - (DWORD)frameTime);
            }
        }
    }

    if (g_pDCRT) g_pDCRT->Release();

    if (g_pWICFactory) g_pWICFactory->Release();
    if (g_pDWriteFactory) g_pDWriteFactory->Release();
    if (g_pD2DFactory) g_pD2DFactory->Release();
    CoUninitialize();
    ReleaseDC(g_hwnd, g_hdcScreen);
    GdiplusShutdown(gdiplusToken);
    return 0;
}