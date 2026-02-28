#include "App.h"
#include <cstdio>

App::App() : m_needReload(false) {
    // 생성자에서는 가벼운 초기화만 수행합니다.
}

App::~App() {
    m_engine.Release();
}

bool App::Init(HINSTANCE hInstance, int nCmdShow) {
    WindowConfig cfg = Window::LoadConfig(L"config.ini");

    if (!m_window.Create(hInstance, nCmdShow, cfg)) {
        return false;
    }

    if (!m_engine.Init(m_window.GetHandle(), cfg)) {
        return false;
    }

    // 4. 입력 콜백 설정
    SetupCallbacks();

    // 5. 루아 바인딩 및 초기화
    SetupBindings();

    if (!m_lua.Init("main.lua", m_systems)) {
        return false;
    }
    m_lua.Call("Init");
    return true;
}

void App::SetupBindings() {
#define BIND(obj, func, name) \
        LuaBinding([&](sol::state& s, const char* n) { (obj).func(s, n); }, name)

    m_systems = {
        BIND(m_engine, BindToLua, "g"),
        BIND(ResourceHub::Instance(), BindLua, "res"),
		BIND(m_window, BindToLua, "is"),
        // 나중에 추가될 시스템들:
        // BIND(m_sound, BindToLua, "s"),
    };

#undef BIND
}

void App::SetupCallbacks() {
    m_window.SetMessageCallback([this](UINT msg, WPARAM wParam, LPARAM lParam) {
        switch (msg) {
        case WM_KEYDOWN:
            m_lua.Call("OnKeyDown", (int)wParam);
#ifdef _DEBUG
            if (wParam == VK_F5) m_needReload = true;
#endif
            break;

        case WM_KEYUP: m_lua.Call("OnKeyUp", (int)wParam); break;
        case WM_MOUSEMOVE: m_lua.Call("OnMouseMove", (int)LOWORD(lParam), (int)HIWORD(lParam)); break;
        case WM_LBUTTONDOWN: m_lua.Call("OnMouseDown", (int)LOWORD(lParam), (int)HIWORD(lParam)); break;
        case WM_LBUTTONUP: m_lua.Call("OnMouseUp", (int)LOWORD(lParam), (int)HIWORD(lParam)); break;
        case WM_RBUTTONDOWN: m_lua.Call("OnRightMouseDown", (int)LOWORD(lParam), (int)HIWORD(lParam)); break;
        case WM_RBUTTONUP: m_lua.Call("OnRightMouseUp", (int)LOWORD(lParam), (int)HIWORD(lParam)); break;
        case WM_MOUSEWHEEL: m_lua.Call("OnMouseWheel", (int)GET_WHEEL_DELTA_WPARAM(wParam)); break;

        case WM_ACTIVATE:
            if (LOWORD(wParam) == WA_INACTIVE) m_lua.Call("OnInactive");
            else m_lua.Call("OnActive");
            break;
        }
        });

    // Provide hit test callback to window so we can control per-point click-through
    m_window.SetHitTestCallback([this](int x, int y) -> bool {
        auto res = m_lua.Call<bool>("CheckHit", (double)x, (double)y);
        return res.value_or(false);
    });
}

void App::Reload() {
    printf("[App] Reloading Scripts...\n");

    // 리소스 정리가 필요하다면 여기서 수행
    // ResourceHub::Instance().Clear(); 

    // 루아 다시 로드
    if (m_lua.Init("main.lua", m_systems)) {
        m_lua.Call("Init");
        printf("[App] Reload Complete!\n");
    }

    m_needReload = false;
}

void App::Run() {
    m_window.RunGameLoop([this](double dtMs) {
        if (m_needReload) {
            Reload();
        }
        // Update mouse passthrough state before drawing (only if transparency enabled)
        if (m_window.GetConfig().transparent) {
            UpdateMousePassthrough();
        }

        m_engine.PreDraw();
        m_lua.Call("Update", dtMs);
        m_lua.Call("Draw");
        m_engine.PostDraw();
        });
}

void App::UpdateMousePassthrough() {
    static POINT lastPt = { -1, -1 };
    POINT currPt;
    if (!GetCursorPos(&currPt)) return;
    if (currPt.x == lastPt.x && currPt.y == lastPt.y) return;
    lastPt = currPt;

    HWND hwnd = m_window.GetHandle();
    if (!hwnd) return;

    POINT clientPt = currPt;
    if (!ScreenToClient(hwnd, &clientPt)) return;

    auto res = m_lua.Call<bool>("CheckHit", (double)clientPt.x, (double)clientPt.y);
    bool is_hit = res.value_or(false);

    static int last_hit_state = -1; // -1 = uninitialized, 0 = not clickable, 1 = clickable

    LONG_PTR exStyle = GetWindowLongPtr(hwnd, GWL_EXSTYLE);
    bool current_clickable = (exStyle & WS_EX_TRANSPARENT) == 0;
    if (last_hit_state == -1) {
        // initialize to actual window state
        last_hit_state = current_clickable ? 1 : 0;
    }

    if (is_hit != (last_hit_state != 0)) {
        if (is_hit) {
            SetWindowLongPtr(hwnd, GWL_EXSTYLE, exStyle & ~WS_EX_TRANSPARENT);
        }
        else {
            SetWindowLongPtr(hwnd, GWL_EXSTYLE, exStyle | WS_EX_TRANSPARENT);
        }
        // apply style change
        SetWindowPos(hwnd, nullptr, 0, 0, 0, 0,
            SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);
        last_hit_state = is_hit ? 1 : 0;
    }
}
