#include "lua_engine.h"

sol::state lua;
ULONGLONG lastTick = 0;
Graphics* g_currentGraphics = nullptr;
PrivateFontCollection* g_pfc = nullptr;
int gDrawW = 0, gDrawH = 0;

HWND    g_hwnd = nullptr;
HDC     g_hdcScreen = nullptr;
HDC     g_hdcMem = nullptr;
HBITMAP g_hBmp = nullptr;
HBITMAP g_hBmpOld = nullptr;
int     g_bufW = 0;
int     g_bufH = 0;

#define CALL_LUA_FUNC(lua_state, func_name, ...) \
    { \
        sol::protected_function f = lua_state[func_name]; \
        if (f.valid()) { \
            auto result = f(__VA_ARGS__); \
            if (!result.valid()) { \
                sol::error err = result; \
                printf("[LUA ERROR] %s: %s\n", func_name, err.what()); \
            } \
        } \
    }

void InitLuaEngine() {
    // 1. sol::state는 새로 생성하거나 collect_garbage를 통해 정리 가능
    // 기존 g_L을 수동으로 닫던 복잡한 과정이 줄어듭니다.
    lua = sol::state();
    lua.open_libraries(
        sol::lib::base,
        sol::lib::package,
        sol::lib::table,
        sol::lib::string,
        sol::lib::math,
        sol::lib::debug
    );

    // 2. API 등록 (sol2 스타일로 변경된 함수들 호출)
    register_sys(lua, "sys");
    register_input(lua, "is");
    register_draw(lua, "g");
    register_res(lua, "res");

    // 3. 스크립트 로드 (sol::load_result 사용으로 안전하게)
    auto load_result = lua.script_file("main.lua", sol::script_pass_on_error);
    if (!load_result.valid()) {
        sol::error err = load_result;
        printf("[LUA ERROR] %s\n", err.what());
        return;
    }
    printf("Lua Engine Initialized / Reloaded via sol2.\n");
}

void drawing() {
    ULONGLONG now = GetTickCount64();
    double dt = double(now - lastTick);
    lastTick = now;

    int w = gDrawW;
    int h = gDrawH;
    if (w <= 0 || h <= 0) return;

    // 1. 백버퍼 생성/재생성
    if (!g_hBmp || w != g_bufW || h != g_bufH)
    {
        if (g_hBmp)
        {
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
    }
    // 2. DIBSection에 GDI+ 렌더링 시작
    Graphics g(g_hdcMem);
    g.SetCompositingMode(CompositingModeSourceOver);
    g.SetSmoothingMode(SmoothingModeAntiAlias);
    g.Clear(Color(0, 0, 0, 0));

    g_currentGraphics = &g;

    // 3. Lua Update / Draw 호출 (sol2의 마법)
    // pcall과 에러 처리를 한 줄로 끝냅니다.
	CALL_LUA_FUNC(lua, "Update", dt);
	CALL_LUA_FUNC(lua, "Draw");

    g_currentGraphics = nullptr;

    // 5. 레이어드 윈도우 갱신
    RECT rc; GetWindowRect(g_hwnd, &rc);
    POINT ptWinPos = { rc.left, rc.top };
    SIZE sizeWin = { w, h };
    POINT ptSrc = { 0, 0 };

    BLENDFUNCTION blend = {};
    blend.BlendOp = AC_SRC_OVER;
    blend.SourceConstantAlpha = 255;
    blend.AlphaFormat = AC_SRC_ALPHA;

    UpdateLayeredWindow(
        g_hwnd,
        g_hdcScreen,
        &ptWinPos,
        &sizeWin,
        g_hdcMem,
        &ptSrc,
        0,
        &blend,
        ULW_ALPHA
    );
}
LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_TIMER:
		drawing();
        break;

    case WM_DESTROY:
        PostQuitMessage(0);
        break;
    case WM_KEYDOWN:
		CALL_LUA_FUNC(lua, "OnKeyDown", (int)wParam);

        if (wParam == VK_F5) {
#ifdef _DEBUG
            printf("Reloading Script...\n");
            InitLuaEngine();
            CALL_LUA_FUNC(lua, "Init");
#endif
        }
        break;

    case WM_KEYUP:
		CALL_LUA_FUNC(lua, "OnKeyUp", (int)wParam);
        break;

    case WM_LBUTTONDOWN:
        CALL_LUA_FUNC(lua, "OnMouseDown", (int)LOWORD(lParam), (int)HIWORD(lParam));
        break;

    case WM_LBUTTONUP:
        CALL_LUA_FUNC(lua, "OnMouseUp", (int)LOWORD(lParam), (int)HIWORD(lParam));
        break;

    case WM_RBUTTONDOWN:
        CALL_LUA_FUNC(lua, "OnRightMouseDown", (int)LOWORD(lParam), (int)HIWORD(lParam));
        break;

    case WM_RBUTTONUP:
        CALL_LUA_FUNC(lua, "OnRightMouseUp", (int)LOWORD(lParam), (int)HIWORD(lParam));
        break;

    default:
        return DefWindowProc(hwnd, msg, wParam, lParam);
    }
    return 0;
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, LPWSTR lpCmdLine, int nCmdShow) {
    // GDI+ 초기화
    GdiplusStartupInput gdiplusStartupInput;
    ULONG_PTR gdiplusToken;
    GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, nullptr);
    g_pfc = new PrivateFontCollection();

#ifdef _DEBUG
    if (AllocConsole()) {
        FILE* fp;
        freopen_s(&fp, "CONOUT$", "w", stdout);
        freopen_s(&fp, "CONOUT$", "w", stderr);
        printf("Debug Console Opened\n");
    }
#endif
    InitLuaEngine();

    // 2. 루아로부터 받아온 설정값으로 윈도우 생성 (g_L이 준비되었으므로 안전)
    gDrawW = lua.get_or("ScreenWidth", 800);
    gDrawH = lua.get_or("ScreenHeight", 600);
    std::string title = lua.get_or<std::string>("WindowTitle", "Fantasy Wagon");
    std::wstring titleW = to_wstring(title);

    // 윈도우 클래스 등록 및 생성
    WNDCLASS wc = { 0 };
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = L"LayeredImageWindow";
    RegisterClass(&wc);

    g_hwnd = CreateWindowEx(WS_EX_LAYERED | WS_EX_TOPMOST, wc.lpszClassName, titleW.c_str(),
        WS_POPUP, 200, 200, gDrawW, gDrawH, nullptr, nullptr, hInstance, nullptr);

    lastTick = GetTickCount64();
    ShowWindow(g_hwnd, nCmdShow);
    SetTimer(g_hwnd, 1, 1, nullptr);

	CALL_LUA_FUNC(lua, "Init");
    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    GdiplusShutdown(gdiplusToken);
    return 0;
}