#include "engine_graphic.h"
#include "sol.h"
#include <tuple>

void register_sys(sol::state& lua, const char* name) {
    auto s = lua.create_named_table(name);

    // 1. 윈도우 크기 설정
    s["size"] = [](int w, int h) {
        if (g_hwnd) {
            SetWindowPos(g_hwnd, NULL, 0, 0, w, h, SWP_NOMOVE | SWP_NOZORDER);
            gDrawW = w;
            gDrawH = h;
            ResizeWindow(w, h);
        }
    };

    // 2. 윈도우 위치 설정
    s["pos"] = [](int x, int y) {
        if (g_hwnd) {
            SetWindowPos(g_hwnd, NULL, x, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
        }
        };

    // 6. 커서 제어
    s["showCursor"] = [](bool show) {
        ShowCursor(show);
        };

    s["cursor"] = [](sol::optional<int> type) {
        // 기본값 IDC_ARROW (32512)
        HCURSOR hCursor = LoadCursor(NULL, MAKEINTRESOURCE(type.value_or(32512)));
        SetCursor(hCursor);
        };

    s["topmost"] = [](bool topmost) {
        if (g_hwnd) {
            // HWND_TOPMOST: 항상 위 (-1)
            // HWND_NOTOPMOST: 항상 위 해제 (-2)
            HWND hWndInsertAfter = topmost ? HWND_TOPMOST : HWND_NOTOPMOST;

            SetWindowPos(g_hwnd, hWndInsertAfter, 0, 0, 0, 0,
                SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
        }
        };


    // VSync 설정 (true: 켬, false: 끔)
    s["vsync"] = [](bool enable) {
        g_vSync = enable;
        };

    // 타겟 프레임 설정 (30, 60 등)
    s["targetFPS"] = [](int fps) {
        if (fps > 0) {
            g_targetFPS = fps;
        }
        };

    // 7. 엔진 종료
    s["quit"] = []() {
        PostQuitMessage(0);
        };
}