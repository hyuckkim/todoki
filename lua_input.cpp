#include "lua_engine.h"

int l_isKeyDown(lua_State* L) {
    int vkey = (int)luaL_checkinteger(L, 1);

    // 최상위 비트가 1이면 눌려 있는 상태입니다.
    short state = GetAsyncKeyState(vkey);
    lua_pushboolean(L, (state & 0x8000) != 0);
    return 1;
}
// x, y, left, right = in.mouse()
int l_getMouse(lua_State* L) {
    POINT pt;
    GetCursorPos(&pt); // 화면 전체 기준 좌표
    ScreenToClient(g_hwnd, &pt); // 우리 창 내부 기준 좌표로 변환

    lua_pushinteger(L, pt.x);
    lua_pushinteger(L, pt.y);
    lua_pushboolean(L, (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0);
    lua_pushboolean(L, (GetAsyncKeyState(VK_RBUTTON) & 0x8000) != 0);
    return 4; // 값 4개 반환
}

void register_input(lua_State* L, const char* name) {
    lua_newtable(L);
    REG_METHOD(L, "key", l_isKeyDown);
    REG_METHOD(L, "mouse", l_getMouse);
    lua_setglobal(L, name);
}