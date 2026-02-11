#include "engine_graphic.h"
#include "sol.h"
#include <tuple>

void register_sys(sol::state& lua, const char* name) {
    auto s = lua.create_named_table(name);

    // 1. 윈도우 크기 설정
    s["setSize"] = [](int w, int h) {
        if (g_hwnd) {
            SetWindowPos(g_hwnd, NULL, 0, 0, w, h, SWP_NOMOVE | SWP_NOZORDER);
            gDrawW = w;
            gDrawH = h;
            ResizeWindow(w, h);
        }
    };

    // 2. 윈도우 위치 설정
    s["setPos"] = [](int x, int y) {
        if (g_hwnd) {
            SetWindowPos(g_hwnd, NULL, x, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
        }
        };

    // 3. 윈도우 현재 위치 (x, y 반환)
    s["getPos"] = []() {
        if (g_hwnd) {
            RECT rc;
            GetWindowRect(g_hwnd, &rc);
            return std::make_tuple(rc.left, rc.top);
        }
        return std::make_tuple((LONG)0, (LONG)0);
        };

    // 4. 윈도우 현재 크기 (w, h 반환)
    s["getSize"] = []() {
        if (g_hwnd) {
            RECT rc;
            GetWindowRect(g_hwnd, &rc);
            return std::make_tuple((int)(rc.right - rc.left), (int)(rc.bottom - rc.top));
        }
        return std::make_tuple(0, 0);
        };

    // 5. 전체 화면 및 작업 영역 크기
    s["getScreenSize"] = []() {
        return std::make_tuple(GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN));
        };

    s["getWorkArea"] = []() {
        RECT rc;
        SystemParametersInfo(SPI_GETWORKAREA, 0, &rc, 0);
        return std::make_tuple((int)(rc.right - rc.left), (int)(rc.bottom - rc.top));
        };

    // 6. 커서 제어
    s["showCursor"] = [](bool show) {
        ShowCursor(show);
        };

    s["setCursor"] = [](sol::optional<int> type) {
        // 기본값 IDC_ARROW (32512)
        HCURSOR hCursor = LoadCursor(NULL, MAKEINTRESOURCE(type.value_or(32512)));
        SetCursor(hCursor);
        };

    s["setTopmost"] = [](bool topmost) {
        if (g_hwnd) {
            // HWND_TOPMOST: 항상 위 (-1)
            // HWND_NOTOPMOST: 항상 위 해제 (-2)
            HWND hWndInsertAfter = topmost ? HWND_TOPMOST : HWND_NOTOPMOST;

            SetWindowPos(g_hwnd, hWndInsertAfter, 0, 0, 0, 0,
                SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
        }
        };

    s["getMonitors"] = [](sol::this_state ts) {
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
    // 7. 엔진 종료
    s["quit"] = []() {
        PostQuitMessage(0);
        };
}