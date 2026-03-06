#include "App.h"
#include <cstdio>

App::App() : m_needReload(false) {
    // 생성자에서는 가벼운 초기화만 수행합니다.
}

App::~App() {
    m_engine.Release();
}

bool App::Init(HINSTANCE hInstance, int nCmdShow, const char* entryPath) {
    // store entry path
    if (entryPath && entryPath[0] != '\0') m_entryPath = entryPath;

    // If an entry path was provided, change current directory to its folder so
    // resources and relative loads resolve from the same directory as the entry.
    if (!m_entryPath.empty()) {
        char full[MAX_PATH];
        if (GetFullPathNameA(m_entryPath.c_str(), MAX_PATH, full, nullptr) > 0) {
            std::string sfull(full);
            size_t pos = sfull.find_last_of("\\/");
            if (pos != std::string::npos) {
                std::string dir = sfull.substr(0, pos);
                SetCurrentDirectoryA(dir.c_str());
            }
        }
    }

    WindowConfig cfg = Window::LoadConfig(L"config.ini");

    if (!m_window.Create(hInstance, nCmdShow, cfg)) {
        MessageBoxA(NULL, "Window creation failed. See console/log for details.", "Init Error", MB_OK | MB_ICONERROR);
        return false;
    }

    if (!m_engine.Init(m_window.GetHandle(), cfg)) {
        MessageBoxA(NULL, "Graphic engine initialization failed.", "Init Error", MB_OK | MB_ICONERROR);
        return false;
    }
    m_sound.Init();

    // 4. 입력 콜백 설정
    SetupCallbacks();

    // 5. 루아 바인딩 및 초기화
    SetupBindings();

    if (!m_lua.Init(m_entryPath.empty() ? "main.lua" : m_entryPath.c_str(), m_systems)) {
        MessageBoxA(NULL, "Lua initialization failed. Check script path and errors.", "Init Error", MB_OK | MB_ICONERROR);
        return false;
    }
#ifdef _DEBUG
    m_lua.WriteDefinitions();
#endif

    m_lua.Call("Init");
    return true;
}

void App::SetupBindings() {
#define BIND(obj, func, name) \
        LuaBinding{[&](LuaBindContext& ctx) { (obj).func(ctx); }, name}

    m_systems = {
        BIND(m_engine, BindToLua, "g"),
        BIND(ResourceHub::Instance(), BindLua, "res"),
        BIND(m_window, BindToLuaInput, "is"),
        BIND(m_window, BindToLuaSys, "sys"),
        BIND(m_sound, BindToLua, "snd")
    };

#undef BIND

    // C++ -> Lua 콜백 시그니처 명시적 등록
    // Called when the mouse moves. id: device handle, dx/dy: movement delta.
    m_lua.RegisterLuaCallable<void, uintptr_t, int, int>(
        "OnMouseMove", {"id", "dx", "dy"},
        "Called when the mouse moves. id: device handle, dx/dy: movement delta."
    );

    // Called when a mouse button is pressed. button: 0=left, 1=right, id: device handle.
    m_lua.RegisterLuaCallable<void, int, uintptr_t>(
        "OnMouseDown", {"button", "id"},
        "Called when a mouse button is pressed. button: 0=left, 1=right, id: device handle."
    );

    // Called when a mouse button is released. button: 0=left, 1=right, id: device handle.
    m_lua.RegisterLuaCallable<void, int, uintptr_t>(
        "OnMouseUp", {"button", "id"},
        "Called when a mouse button is released. button: 0=left, 1=right, id: device handle."
    );

    // Called when the mouse wheel is scrolled. wheelDelta: amount, id: device handle, dx/dy: movement delta.
    m_lua.RegisterLuaCallable<void, int, uintptr_t, int, int>(
        "OnMouseWheel", {"wheelDelta", "id", "dx", "dy"},
        "Called when the mouse wheel is scrolled. wheelDelta: amount, id: device handle, dx/dy: movement delta."
    );

    // Called when a key is pressed. key: virtual key code.
    m_lua.RegisterLuaCallable<void, int>(
        "OnKeyDown", {"key"},
        "Called when a key is pressed. key: virtual key code."
    );

    // Called when a key is released. key: virtual key code.
    m_lua.RegisterLuaCallable<void, int>(
        "OnKeyUp", {"key"},
        "Called when a key is released. key: virtual key code."
    );

    // Called when the window becomes inactive (loses focus).
    m_lua.RegisterLuaCallable<void>(
        "OnInactive", {},
        "Called when the window becomes inactive (loses focus)."
    );

    // Called when the window becomes active (gains focus).
    m_lua.RegisterLuaCallable<void>(
        "OnActive", {},
        "Called when the window becomes active (gains focus)."
    );

    // Used for per-pixel hit testing. Returns true if (x, y) is clickable, false for transparent (mouse passthrough).
    m_lua.RegisterLuaCallable<bool, double, double>(
        "CheckHit", {"x", "y"},
        "Used for per-pixel hit testing. Returns true if (x, y) is clickable, false for transparent (mouse passthrough)."
    );

    // Called once at Lua startup for initialization.
    m_lua.RegisterLuaCallable<void>(
        "Init", {},
        "Called once at Lua startup for initialization."
    );

    // Called every frame to update game logic. dtMs: elapsed time in milliseconds.
    m_lua.RegisterLuaCallable<void, double>(
        "Update", {"dtMs"},
        "Called every frame to update game logic. dtMs: elapsed time in milliseconds."
    );

    // Called every frame to render the scene.
    m_lua.RegisterLuaCallable<void>(
        "Draw", {},
        "Called every frame to render the scene."
    );
}

void App::SetupCallbacks() {
    m_window.SetMessageCallback([this](UINT msg, WPARAM wParam, LPARAM lParam) {
        switch (msg) {
            case WM_INPUT: {
                UINT dwSize = sizeof(RAWINPUT);
                static BYTE lpb[sizeof(RAWINPUT)];
                GetRawInputData((HRAWINPUT)lParam, RID_INPUT, lpb, &dwSize, sizeof(RAWINPUTHEADER));
                RAWINPUT* raw = (RAWINPUT*)lpb;

                if (raw->header.dwType == RIM_TYPEMOUSE) {
                    uintptr_t id = (uintptr_t)raw->header.hDevice;
                    int dx = raw->data.mouse.lLastX;
                    int dy = raw->data.mouse.lLastY;
                    USHORT btn = raw->data.mouse.usButtonFlags;

                    // 1. 이동 처리 (dx, dy가 0이 아닐 때만)
                    if (dx != 0 || dy != 0) {
                        m_lua.Call("OnMouseMove", id, dx, dy);
                    }

                    // 2. 버튼 처리 (Raw Input 플래그를 체크)
                    if (btn & RI_MOUSE_LEFT_BUTTON_DOWN)   m_lua.Call("OnMouseDown", 0, id);
                    if (btn & RI_MOUSE_LEFT_BUTTON_UP)     m_lua.Call("OnMouseUp", 0, id);
                    if (btn & RI_MOUSE_RIGHT_BUTTON_DOWN)  m_lua.Call("OnMouseDown", 1, id);
                    if (btn & RI_MOUSE_RIGHT_BUTTON_UP)    m_lua.Call("OnMouseUp", 1, id);

                    // 3. 휠 처리
                    if (btn & RI_MOUSE_WHEEL) {
                        short wheelDelta = (short)raw->data.mouse.usButtonData;
                        m_lua.Call("OnMouseWheel", (int)wheelDelta, id, dx, dy);
                    }
                }
                break;
            }

        case WM_KEYDOWN:
            m_lua.Call("OnKeyDown", (int)wParam);
#ifdef _DEBUG
            if (wParam == VK_F5) m_needReload = true;
#endif
            break;

        case WM_KEYUP: m_lua.Call("OnKeyUp", (int)wParam); break;

        case WM_ACTIVATE:
            if (LOWORD(wParam) == WA_INACTIVE) m_lua.Call("OnInactive");
            else m_lua.Call("OnActive");
            break;
        }
        });

    // Notify engine when window size is changed via Lua sys.size
    m_window.SetSizeCallback([this](int w, int h) {
        m_engine.Resize(w, h);
    });

    // Provide hit test callback to window so we can control per-point click-through
    m_window.SetHitTestCallback([this](int x, int y) -> bool {
        auto res = m_lua.Call<bool>("CheckHit", (double)x, (double)y);
        return res.value_or(true);
    });
}

void App::Reload() {
    printf("[App] Reloading Scripts...\n");

    // 리소스 정리가 필요하다면 여기서 수행
    // ResourceHub::Instance().Clear(); 

    // 루아 다시 로드
    // Ensure current directory is the entry's directory before reloading
    if (!m_entryPath.empty()) {
        char full[MAX_PATH];
        if (GetFullPathNameA(m_entryPath.c_str(), MAX_PATH, full, nullptr) > 0) {
            std::string sfull(full);
            size_t pos = sfull.find_last_of("\\/");
            if (pos != std::string::npos) {
                std::string dir = sfull.substr(0, pos);
                SetCurrentDirectoryA(dir.c_str());
            }
        }
    }

    if (m_lua.Init(m_entryPath.empty() ? "main.lua" : m_entryPath.c_str(), m_systems)) {
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
        if (m_window.GetConfig().getTransparent()) {
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
    bool is_hit = res.value_or(true);

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
        last_hit_state = is_hit ? 1 : 0;
    }
}
