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

    if (!m_engine.Init(m_window.GetHandle(), cfg.width, cfg.height)) {
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
        // 나중에 추가될 시스템들:
        // BIND(ResourceHub::Instance(), BindResources, "res"),
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

        m_engine.PreDraw();
        m_lua.Call("Update", dtMs);
        m_lua.Call("Draw");
        m_engine.PostDraw();
        });
}