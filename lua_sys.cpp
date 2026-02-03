#include "lua_engine.h"
#include <tuple>

void register_sys(sol::state& lua, const char* name) {
    auto s = lua.create_named_table(name);

    // 1. 윈도우 크기 설정
    s["setSize"] = [](int w, int h) {
        if (g_hwnd) {
            SetWindowPos(g_hwnd, NULL, 0, 0, w, h, SWP_NOMOVE | SWP_NOZORDER);
            gDrawW = w;
            gDrawH = h;
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

    // 7. 엔진 종료
    s["quit"] = []() {
        PostQuitMessage(0);
        };
}