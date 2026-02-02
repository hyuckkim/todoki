#include "lua_engine.h"

// setWindowSize(w, h)
int l_setWindowSize(lua_State* L) {
    int w = (int)luaL_checkinteger(L, 1);
    int h = (int)luaL_checkinteger(L, 2);

    if (g_hwnd) {
        // SWP_NOMOVE: 위치는 건드리지 마라
        // SWP_NOZORDER: 앞뒤 순서(Z-Order)는 건드리지 마라
        SetWindowPos(g_hwnd, NULL, 0, 0, w, h, SWP_NOMOVE | SWP_NOZORDER);

        // 중요: 엔진 내부에서 사용하는 드로잉 크기 변수도 갱신해줘야 함
        gDrawW = w;
        gDrawH = h;
    }
    return 0;
}

// setWindowPos(x, y)
int l_setWindowPos(lua_State* L) {
    int x = (int)luaL_checkinteger(L, 1);
    int y = (int)luaL_checkinteger(L, 2);

    if (g_hwnd) {
        // SWP_NOSIZE: 크기는 건드리지 마라
        // SWP_NOZORDER: 앞뒤 순서(Z-Order)는 건드리지 마라
        SetWindowPos(g_hwnd, NULL, x, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
    }
    return 0;
}

// x, y = sys.getPos()
int l_getPos(lua_State* L) {
    if (g_hwnd) {
        RECT rc;
        GetWindowRect(g_hwnd, &rc);
        lua_pushinteger(L, rc.left); // x
        lua_pushinteger(L, rc.top);  // y
        return 2; // 두 개의 값을 반환
    }
    return 0;
}

// w, h = sys.getSize()
int l_getSize(lua_State* L) {
    if (g_hwnd) {
        RECT rc;
        GetWindowRect(g_hwnd, &rc);
        lua_pushinteger(L, rc.right - rc.left); // width
        lua_pushinteger(L, rc.bottom - rc.top); // height
        return 2;
    }
    return 0;
}

// sw, sh = sys.getScreenSize()
int l_getScreenSize(lua_State* L) {
    int sw = GetSystemMetrics(SM_CXSCREEN);
    int sh = GetSystemMetrics(SM_CYSCREEN);
    lua_pushinteger(L, sw);
    lua_pushinteger(L, sh);
    return 2;
}

// aw, ah = sys.getWorkArea()
int l_getWorkArea(lua_State* L) {
    RECT rc;
    // SPI_GETWORKAREA는 작업 표시줄을 제외한 영역을 반환합니다.
    SystemParametersInfo(SPI_GETWORKAREA, 0, &rc, 0);

    int width = rc.right - rc.left;
    int height = rc.bottom - rc.top;

    lua_pushinteger(L, width);
    lua_pushinteger(L, height);
    return 2;
}

// sys.showCursor(true/false)
int l_showCursor(lua_State* L) {
    bool show = lua_toboolean(L, 1);
    // ShowCursor는 카운트 방식이라 여러 번 호출하면 꼬일 수 있으니 주의!
    ShowCursor(show);
    return 0;
}

// sys.setCursor(type)
int l_setCursor(lua_State* L) {
    int type = (int)luaL_optinteger(L, 1, 32512); // IDC_ARROW 기본값
    /*
        IDC_ARROW: 32512
        IDC_IBEAM: 32513
        IDC_WAIT:  32514
        IDC_HAND:  32649
    */
    HCURSOR hCursor = LoadCursor(NULL, MAKEINTRESOURCE(type));
    SetCursor(hCursor);
    return 0;
}

static int l_sysQuit(lua_State* L) {
    // 윈도우 메세지 큐에 WM_QUIT를 보냅니다.
    // 그러면 main의 GetMessage 루프가 종료됩니다.
    PostQuitMessage(0);
    return 0;
}

void register_sys(lua_State* L, const char* name) {
    lua_newtable(L);
    REG_METHOD(L, "setSize", l_setWindowSize);
    REG_METHOD(L, "setPos", l_setWindowPos);
    REG_METHOD(L, "getSize", l_getSize);
    REG_METHOD(L, "getPos", l_getPos);
    REG_METHOD(L, "getScreenSize", l_getScreenSize);
    REG_METHOD(L, "getWorkArea", l_getWorkArea);
    REG_METHOD(L, "showCursor", l_showCursor);
    REG_METHOD(L, "setCursor", l_setCursor);
	REG_METHOD(L, "quit", l_sysQuit);
    lua_setglobal(L, name);
}