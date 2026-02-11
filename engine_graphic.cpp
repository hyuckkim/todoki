#include "engine_graphic.h"
#include <d3d11_1.h>
#include <dxgi1_2.h>
#include <dcomp.h>
#include <wrl/client.h>
#include "engine_lua.h"

using Microsoft::WRL::ComPtr;

// --- 라이브러리 링크 ---
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d2d1.lib")
#pragma comment(lib, "dwrite.lib")
#pragma comment(lib, "dcomp.lib")
#pragma comment(lib, "windowscodecs.lib")

// --- 전역 자원 ---
ComPtr<ID2D1Factory1> g_pD2DFactory;
ComPtr<ID2D1DeviceContext> g_pD2DDC;
ComPtr<IDWriteFactory> g_pDWriteFactory;
ComPtr<IWICImagingFactory> g_pWICFactory;

ComPtr<ID3D11Device>    g_pD3D11Device;
ComPtr<IDXGISwapChain1> g_pSwapChain;
ComPtr<IDCompositionDevice> g_pDCompDevice;
ComPtr<IDCompositionTarget> g_pDCompTarget;
ComPtr<IDCompositionVisual> g_pDCompVisual;

HWND g_hwnd = nullptr;
int gDrawW = 800, gDrawH = 600;

// NVIDIA/AMD 고성능 GPU 강제 활성화 선언
extern "C" {
    _declspec(dllexport) DWORD NvOptimusEnablement = 0x00000001;
    _declspec(dllexport) int AmdPowerXpressRequestHighPerformance = 1;
}

void InitCom() {
    // COM 라이브러리 초기화
    HRESULT hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    if (FAILED(hr)) {
        printf("COM Initialization Failed: 0x%08X\n", hr);
        return;
    }

    // DirectWrite Factory 생성
    // .ReleaseAndGetAddressOf()를 사용하면 혹시 모를 기존 객체를 해제하고 주소를 가져옵니다.
    hr = DWriteCreateFactory(
        DWRITE_FACTORY_TYPE_SHARED,
        __uuidof(IDWriteFactory),
        reinterpret_cast<IUnknown**>(g_pDWriteFactory.ReleaseAndGetAddressOf())
    );
    if (FAILED(hr)) printf("DWrite Factory Creation Failed\n");

    // WIC Imaging Factory 생성
    hr = CoCreateInstance(
        CLSID_WICImagingFactory,
        NULL,
        CLSCTX_INPROC_SERVER,
        IID_PPV_ARGS(g_pWICFactory.ReleaseAndGetAddressOf())
    );
    if (FAILED(hr)) printf("WIC Factory Creation Failed\n");
}

extern bool g_isDrawing;
void InitD2D() {
    // 1. D2D Factory (ID2D1Factory1 필수)
    D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, __uuidof(ID2D1Factory1), (void**)&g_pD2DFactory);

    // 2. D3D11 Device 생성
    UINT creationFlags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
#ifdef _DEBUG
    creationFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif
    D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, creationFlags, nullptr, 0, D3D11_SDK_VERSION, &g_pD3D11Device, nullptr, nullptr);

    // 3. DXGI & D2D Device Context 설정
    ComPtr<IDXGIDevice> dxgiDevice;
    g_pD3D11Device.As(&dxgiDevice);

    ComPtr<ID2D1Device> d2dDevice;
    g_pD2DFactory->CreateDevice(dxgiDevice.Get(), &d2dDevice);
    d2dDevice->CreateDeviceContext(D2D1_DEVICE_CONTEXT_OPTIONS_NONE, &g_pD2DDC);

    // 4. SwapChain 생성 (투명도 지원 전용)
    DXGI_SWAP_CHAIN_DESC1 swapChainDesc = { 0 };
    swapChainDesc.Width = gDrawW;
    swapChainDesc.Height = gDrawH;
    swapChainDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    swapChainDesc.Stereo = FALSE;
    swapChainDesc.SampleDesc.Count = 1;
    swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapChainDesc.BufferCount = 2;
    swapChainDesc.Scaling = DXGI_SCALING_STRETCH;
    swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
    swapChainDesc.AlphaMode = DXGI_ALPHA_MODE_PREMULTIPLIED; // 반투명 윈도우 핵심

    ComPtr<IDXGIAdapter> dxgiAdapter;
    dxgiDevice->GetAdapter(&dxgiAdapter);
    ComPtr<IDXGIFactory2> dxgiFactory;
    dxgiAdapter->GetParent(IID_PPV_ARGS(&dxgiFactory));
    dxgiFactory->CreateSwapChainForComposition(g_pD3D11Device.Get(), &swapChainDesc, nullptr, &g_pSwapChain);

    // 5. DirectComposition 연결
    DCompositionCreateDevice(dxgiDevice.Get(), IID_PPV_ARGS(&g_pDCompDevice));
    g_pDCompDevice->CreateTargetForHwnd(g_hwnd, TRUE, &g_pDCompTarget);
    g_pDCompDevice->CreateVisual(&g_pDCompVisual);
    g_pDCompVisual->SetContent(g_pSwapChain.Get());
    g_pDCompTarget->SetRoot(g_pDCompVisual.Get());
    g_pDCompDevice->Commit();

    // 6. D2D 렌더 타겟 비트맵 설정
    ComPtr<IDXGISurface> surface;
    g_pSwapChain->GetBuffer(0, IID_PPV_ARGS(&surface));
    D2D1_BITMAP_PROPERTIES1 bitmapProperties = D2D1::BitmapProperties1(
        D2D1_BITMAP_OPTIONS_TARGET | D2D1_BITMAP_OPTIONS_CANNOT_DRAW,
        D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED)
    );
    ComPtr<ID2D1Bitmap1> d2dTargetBitmap;
    g_pD2DDC->CreateBitmapFromDxgiSurface(surface.Get(), &bitmapProperties, &d2dTargetBitmap);
    g_pD2DDC->SetTarget(d2dTargetBitmap.Get());
}

void InitWindow(WNDPROC WndProc, HINSTANCE hInstance, const wchar_t* title) {
    WNDCLASS wc = { 0 };
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = L"DCompWindow";
    RegisterClass(&wc);

    g_hwnd = CreateWindowEx(WS_EX_TOPMOST | WS_EX_NOREDIRECTIONBITMAP | WS_EX_LAYERED, wc.lpszClassName, title,
        WS_POPUP, 200, 200, gDrawW, gDrawH, nullptr, nullptr, hInstance, nullptr);
    SetLayeredWindowAttributes(g_hwnd, 0, 255, LWA_ALPHA);
}

void UpdateMousePassthrough() {
    static POINT lastPt = { -1, -1 };
    POINT currPt;
    GetCursorPos(&currPt);
    if (currPt.x == lastPt.x && currPt.y == lastPt.y) return;
    lastPt = currPt;

    ScreenToClient(g_hwnd, &currPt);
    bool is_hit = Call<bool>("CheckHit", (double)currPt.x, (double)currPt.y).value_or(true);

    static bool last_hit_state = true;
    if (is_hit != last_hit_state) {
        LONG_PTR exStyle = GetWindowLongPtr(g_hwnd, GWL_EXSTYLE);
        if (is_hit) {
            SetWindowLongPtr(g_hwnd, GWL_EXSTYLE, exStyle & ~WS_EX_TRANSPARENT);
        }
        else {
            SetWindowLongPtr(g_hwnd, GWL_EXSTYLE, exStyle | WS_EX_TRANSPARENT);
        }
        last_hit_state = is_hit;
    }
}

void PreDraw() {
    if (!g_pD2DDC || g_isDrawing) return; // 이미 그리고 있으면 중복 호출 방지

    g_pD2DDC->BeginDraw();
    g_isDrawing = true;
    g_pD2DDC->Clear(D2D1::ColorF(0, 0, 0, 0));
}

void PostDraw() {
    if (!g_pD2DDC || !g_isDrawing) return;

    HRESULT hr = g_pD2DDC->EndDraw();
    g_isDrawing = false; // 상태 해제

    if (hr == D2DERR_RECREATE_TARGET) {
        RecreateDevice();
        return;
    }
    else if (FAILED(hr))
    {
        printf("EndDraw Failed: 0x%08X\n", hr);
        return;
    }
    g_pSwapChain->Present(1, 0);
}

void ReleaseGraphic() {
    if (g_pD2DDC) {
        g_pD2DDC->SetTarget(nullptr);
    }

    g_pDCompVisual.Reset();
    g_pDCompTarget.Reset();
    g_pDCompDevice.Reset();
    g_pSwapChain.Reset();
    g_pD2DDC.Reset();
    g_pD3D11Device.Reset();

    g_pWICFactory.Reset();
    g_pDWriteFactory.Reset();
    g_pD2DFactory.Reset();

    CoUninitialize();
}

void RecreateDevice() {
    printf("[Win] Recreating Device due to error...\n");
    g_pD2DDC.Reset();
    g_pD2DFactory.Reset();
    g_pSwapChain.Reset();
    g_pD3D11Device.Reset();

    InitD2D();
}

void ResizeWindow(UINT width, UINT height)
{
    if (!g_pSwapChain) return;
    if (width == 0 || height == 0) return;

    gDrawW = width;
    gDrawH = height;

    g_pD2DDC->SetTarget(nullptr);

    HRESULT hr = g_pSwapChain->ResizeBuffers(
        0,
        width,
        height,
        DXGI_FORMAT_UNKNOWN,
        0
    );

    if (FAILED(hr)) {
        printf("ResizeBuffers failed: 0x%08X\n", hr);
        return;
    }

    ComPtr<IDXGISurface> surface;
    hr = g_pSwapChain->GetBuffer(
        0,
        IID_PPV_ARGS(&surface)
    );

    if (FAILED(hr)) {
        printf("GetBuffer failed: 0x%08X\n", hr);
        return;
    }

    D2D1_BITMAP_PROPERTIES1 props =
        D2D1::BitmapProperties1(
            D2D1_BITMAP_OPTIONS_TARGET | D2D1_BITMAP_OPTIONS_CANNOT_DRAW,
            D2D1::PixelFormat(
                DXGI_FORMAT_B8G8R8A8_UNORM,
                D2D1_ALPHA_MODE_PREMULTIPLIED
            )
        );

    ComPtr<ID2D1Bitmap1> bitmap;
    hr = g_pD2DDC->CreateBitmapFromDxgiSurface(
        surface.Get(),
        &props,
        &bitmap
    );

    if (FAILED(hr)) {
        printf("CreateBitmapFromDxgiSurface failed: 0x%08X\n", hr);
        return;
    }

    g_pD2DDC->SetTarget(bitmap.Get());

    if (g_pDCompDevice)
        g_pDCompDevice->Commit();
}
