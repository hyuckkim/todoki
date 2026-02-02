#include "lua_engine.h"

lua_State* g_L;
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

// 정수 가져오기 매크로: int
#define LUA_GET_INT(L, name, var) \
    lua_getglobal(L, name); \
    if (lua_isnumber(L, -1)) var = (int)lua_tonumber(L, -1); \
    lua_pop(L, 1);

// 문자열 가져오기 매크로: std::string
#define LUA_GET_STR(L, name, var) \
    lua_getglobal(L, name); \
    if (lua_isstring(L, -1)) var = lua_tostring(L, -1); \
    lua_pop(L, 1);

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
    g.SetSmoothingMode(SmoothingModeAntiAlias); // 깔끔한 선을 위해 추가
    g.Clear(Color(0, 0, 0, 0));

	// 3. Lua Update / Draw 호출
    g_currentGraphics = &g;
    lua_getglobal(g_L, "update");
    if (lua_isfunction(g_L, -1)) {
        lua_pushnumber(g_L, dt); // dt 전달
        lua_pcall(g_L, 1, 0, 0);
    }
    else { lua_pop(g_L, 1); }

    // 4. Lua Draw 호출
    lua_getglobal(g_L, "draw");
    if (lua_isfunction(g_L, -1)) {
        lua_pcall(g_L, 0, 0, 0);
    }
    else { lua_pop(g_L, 1); }
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
        // 루아의 OnKeyDown(vkCode) 함수 호출
        lua_getglobal(g_L, "OnKeyDown");
        if (lua_isfunction(g_L, -1)) {
            lua_pushinteger(g_L, (int)wParam);
            lua_pcall(g_L, 1, 0, 0);
        }
        else {
            lua_pop(g_L, 1);
        }
        break;

    case WM_KEYUP:
        // 루아의 OnKeyUp(vkCode) 함수 호출
        lua_getglobal(g_L, "OnKeyUp");
        if (lua_isfunction(g_L, -1)) {
            lua_pushinteger(g_L, (int)wParam);
            lua_pcall(g_L, 1, 0, 0);
        }
        else {
            lua_pop(g_L, 1);
        }
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

#ifdef _DEBUG
    if (AllocConsole()) {
        FILE* fp;
        freopen_s(&fp, "CONOUT$", "w", stdout); // printf를 콘솔로 연결
        freopen_s(&fp, "CONOUT$", "w", stderr); // 에러 메시지 연결
        printf("Debug Console Opened\n");
    }
#endif

	// LUA 초기화
    g_L = luaL_newstate(); // Lua 엔진 생성
    luaL_openlibs(g_L);               // Lua 기본 함수들(print, math 등) 로드

    if (luaL_dofile(g_L, "main.lua") != LUA_OK) {
        const char* error = lua_tostring(g_L, -1);
        OutputDebugStringA("--- LUA ERROR ---\n");
        OutputDebugStringA(error);
        OutputDebugStringA("\n-----------------\n");
        lua_pop(g_L, 1);
    }

    std::string myTitleString = "Default Title";

    LUA_GET_INT(g_L, "screenWidth", gDrawW);
    LUA_GET_INT(g_L, "screenHeight", gDrawH);
    LUA_GET_STR(g_L, "windowTitle", myTitleString);

	STR_TO_WCHAR(myTitleString, titleW);

    // 윈도우 클래스 등록
    WNDCLASS wc = {};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = L"LayeredImageWindow";
    RegisterClass(&wc);

    g_pfc = new PrivateFontCollection();

    // 위치 결정
    int screenW = GetSystemMetrics(SM_CXSCREEN);
    int screenH = GetSystemMetrics(SM_CYSCREEN);
    HWND hwnd = CreateWindowEx(
        WS_EX_LAYERED | WS_EX_TOPMOST,
        wc.lpszClassName, titleW,
        WS_POPUP,
        200, 200, gDrawW, gDrawH,
        nullptr, nullptr, hInstance, nullptr);
    g_hwnd = hwnd;

    lastTick = GetTickCount64();

    ShowWindow(hwnd, nCmdShow);
    SetTimer(hwnd, 1, 1, nullptr); // 최소 주기


    register_sys(g_L, "sys");
    register_input(g_L, "is");
    register_draw(g_L, "g");
    register_res(g_L, "res");

	lua_getglobal(g_L, "init");
    if (lua_isfunction(g_L, -1)) {
        lua_pcall(g_L, 0, 0, 0);
    }
    else {
        lua_pop(g_L, 1);
    }
    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    unregisterLuaFunctions(g_L);
    GdiplusShutdown(gdiplusToken);
    return 0;
}
