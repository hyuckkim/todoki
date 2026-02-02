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


void InitLuaEngine() {
    // 1. 기존 상태가 있다면 종료
    if (g_L) {
        unregisterLuaFunctions(g_L); // 등록 해제
        lua_close(g_L);
    }

    // 2. 새로운 Lua 상태 생성
    g_L = luaL_newstate();
    luaL_openlibs(g_L);

    // 3. API 등록 (sys, is, g, res 등)
    register_sys(g_L, "sys");
    register_input(g_L, "is");
    register_draw(g_L, "g");
    register_res(g_L, "res");

    // 4. 스크립트 로드
    if (luaL_dofile(g_L, "main.lua") != LUA_OK) {
        printf("[LUA ERROR] %s\n", lua_tostring(g_L, -1));
        lua_pop(g_L, 1);
        return;
    }
    printf("Lua Engine Initialized / Reloaded.\n");
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
        if (wParam == VK_F5) {
#ifdef _DEBUG
            printf("Reloading Script...\n");
            InitLuaEngine(); // 엔진을 껐다 켜서 모든 루아 상태 초기화

            lua_getglobal(g_L, "init");
            if (lua_isfunction(g_L, -1)) {
                if (lua_pcall(g_L, 0, 0, 0) != LUA_OK) {
                    printf("[INIT ERROR] %s\n", lua_tostring(g_L, -1));
                    lua_pop(g_L, 1);
                }
            }
            else {
                lua_pop(g_L, 1);
            }
#endif
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
    g_pfc = new PrivateFontCollection();

#ifdef _DEBUG
    if (AllocConsole()) {
        FILE* fp;
        freopen_s(&fp, "CONOUT$", "w", stdout);
        freopen_s(&fp, "CONOUT$", "w", stderr);
        printf("Debug Console Opened\n");
    }
#endif

    // 1. 루아 엔진 최초 실행
    InitLuaEngine();

    // 2. 루아로부터 받아온 설정값으로 윈도우 생성 (g_L이 준비되었으므로 안전)
    std::string myTitleString = "Default Title";
    LUA_GET_INT(g_L, "screenWidth", gDrawW);
    LUA_GET_INT(g_L, "screenHeight", gDrawH);
    LUA_GET_STR(g_L, "windowTitle", myTitleString);
    STR_TO_WCHAR(myTitleString, titleW);

    // 윈도우 클래스 등록 및 생성
    WNDCLASS wc = { 0 };
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = L"LayeredImageWindow";
    RegisterClass(&wc);

    g_hwnd = CreateWindowEx(WS_EX_LAYERED | WS_EX_TOPMOST, wc.lpszClassName, titleW,
        WS_POPUP, 200, 200, gDrawW, gDrawH, nullptr, nullptr, hInstance, nullptr);

    lastTick = GetTickCount64();
    ShowWindow(g_hwnd, nCmdShow);
    SetTimer(g_hwnd, 1, 1, nullptr);


    lua_getglobal(g_L, "init");
    if (lua_isfunction(g_L, -1)) {
        if (lua_pcall(g_L, 0, 0, 0) != LUA_OK) {
            printf("[INIT ERROR] %s\n", lua_tostring(g_L, -1));
            lua_pop(g_L, 1);
        }
    }
    else {
        lua_pop(g_L, 1);
    }
    // 3. 메시지 루프
    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    // 종료 처리
    if (g_L) lua_close(g_L);
    GdiplusShutdown(gdiplusToken);
    return 0;
}