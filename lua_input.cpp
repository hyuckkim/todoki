#include "lua_engine.h"
#include <tuple>

void register_input(sol::state& lua, const char* name) {
    auto i = lua.create_named_table(name);

    // 1. 키보드 입력 체크
    i["key"] = [](int vkey) -> bool {
        // short state = GetAsyncKeyState(vkey);
        // return (state & 0x8000) != 0;
        return (GetAsyncKeyState(vkey) & 0x8000) != 0;
    };

    // 2. 마우스 정보 (x, y, left, right) 반환
    i["mouse"] = []() {
        POINT pt;
        GetCursorPos(&pt);
        ScreenToClient(g_hwnd, &pt);

        bool left = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
        bool right = (GetAsyncKeyState(VK_RBUTTON) & 0x8000) != 0;

        return std::make_tuple(pt.x, pt.y, left, right);
    };
}