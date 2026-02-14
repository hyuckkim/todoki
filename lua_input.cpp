#include "engine_graphic.h"
#include "sol.h"
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
    i["pos"] = []() {
        RECT rc;
        GetWindowRect(g_hwnd, &rc);
        return std::make_tuple(rc.left, rc.top);
        };

    i["size"] = []() {
        RECT rc;
        GetWindowRect(g_hwnd, &rc);
        return std::make_tuple((int)(rc.right - rc.left), (int)(rc.bottom - rc.top));
        };

    i["workArea"] = []() {
        RECT rc;
        SystemParametersInfo(SPI_GETWORKAREA, 0, &rc, 0);
        return std::make_tuple((int)(rc.right - rc.left), (int)(rc.bottom - rc.top));
        };
    i["screenSize"] = []() {
        return std::make_tuple(GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN));
        };

    i["monitors"] = [](sol::this_state ts) {
        sol::state_view lua(ts);
        sol::table monitorList = lua.create_table();

        // 모니터를 순회하며 정보를 수집할 콜백 구조체/람다
        auto callback = [](HMONITOR hMonitor, HDC hdcMonitor, LPRECT lprcMonitor, LPARAM dwData) -> BOOL {
            auto& list = *reinterpret_cast<sol::table*>(dwData);

            MONITORINFO mi = { sizeof(mi) };
            if (GetMonitorInfo(hMonitor, &mi)) {
                sol::state_view lua = list.lua_state();
                sol::table info = lua.create_table();

                // rcMonitor: 모니터 전체 해상도
                // rcWork: 작업 표시줄을 제외한 실제 사용 가능 영역
                info["x"] = mi.rcMonitor.left;
                info["y"] = mi.rcMonitor.top;
                info["w"] = mi.rcMonitor.right - mi.rcMonitor.left;
                info["h"] = mi.rcMonitor.bottom - mi.rcMonitor.top;

                info["workX"] = mi.rcWork.left;
                info["workY"] = mi.rcWork.top;
                info["workW"] = mi.rcWork.right - mi.rcWork.left;
                info["workH"] = mi.rcWork.bottom - mi.rcWork.top;

                list.add(info);
            }
            return TRUE;
            };

        EnumDisplayMonitors(NULL, NULL, (MONITORENUMPROC)+callback, (LPARAM)&monitorList);

        return monitorList;
        };

    // VSync/FPS 상태도 여기서 확인하면 좋음
    i["fpsMode"] = []() {
        return std::make_tuple(g_targetFPS, g_vSync);
        };
}