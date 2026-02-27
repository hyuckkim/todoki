#include "graphicengine.h"
#include <windows.h>
#include <wincodec.h>
#include <d3d11_1.h>
#include <dxgi1_2.h>
#include <dcomp.h>
#include <wrl/client.h>
#include <stdio.h>

GraphicEngine::GraphicEngine(HWND hwnd, int width, int height)
    : m_hwnd(hwnd), m_width(width), m_height(height) {
}

void GraphicEngine::Init() {
    InitCom();
    InitD2D();
}

void GraphicEngine::PreDraw() {
    if (!m_pD2DDC || m_isDrawing) return;
    m_pD2DDC->BeginDraw();
    m_pD2DDC->Clear(D2D1::ColorF(0, 0, 0, 0));
    m_isDrawing = true;
}

void GraphicEngine::PostDraw() {
    if (!m_pD2DDC || !m_isDrawing) return;
    HRESULT hr = m_pD2DDC->EndDraw();
    m_isDrawing = false;

    if (hr == D2DERR_RECREATE_TARGET) {
        RecreateDevice();
        return;
    }

    if (FAILED(hr)) printf("EndDraw Failed: 0x%08X\n", hr);
    m_pSwapChain->Present(m_vSync ? 1 : 0, 0);
}

void GraphicEngine::RecreateDevice() {
    printf("[Win] Recreating Device due to error...\n");
    m_pD2DDC.Reset();
    m_pD2DFactory.Reset();
    m_pSwapChain.Reset();
    m_pD3D11Device.Reset();

    InitD2D();
}

void GraphicEngine::InitCom() {
    HRESULT hr = 0;

    // COM 라이브러리 초기화
    hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    if (FAILED(hr)) {
        printf("COM Initialization Failed: 0x%08X\n", hr);
        return;
    }

    // DirectWrite Factory 생성
    // .ReleaseAndGetAddressOf()를 사용하면 혹시 모를 기존 객체를 해제하고 주소를 가져옵니다.
    hr = DWriteCreateFactory(
        DWRITE_FACTORY_TYPE_SHARED,
        __uuidof(IDWriteFactory),
        reinterpret_cast<IUnknown**>(m_pDWriteFactory.ReleaseAndGetAddressOf())
    );
    if (FAILED(hr)) printf("DWrite Factory Creation Failed\n");

    // WIC Imaging Factory 생성
    hr = CoCreateInstance(
        CLSID_WICImagingFactory,
        NULL,
        CLSCTX_INPROC_SERVER,
        IID_PPV_ARGS(m_pWICFactory.ReleaseAndGetAddressOf())
    );
    if (FAILED(hr)) printf("WIC Factory Creation Failed\n");
}

void GraphicEngine::InitD2D() {
    HRESULT hr;

    // 1. D2D Factory 생성
    hr = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED,
        __uuidof(ID2D1Factory1),
        (void**)m_pD2DFactory.GetAddressOf());
    if (FAILED(hr)) { printf("D2D Factory creation failed: 0x%08X\n", hr); return; }

    // 2. D3D11 Device 생성
    UINT creationFlags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
#ifdef _DEBUG
    creationFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

    hr = D3D11CreateDevice(
        nullptr,
        D3D_DRIVER_TYPE_HARDWARE,
        nullptr,
        creationFlags,
        nullptr, 0,
        D3D11_SDK_VERSION,
        &m_pD3D11Device,
        nullptr,
        nullptr);
    if (FAILED(hr)) { printf("D3D11 Device creation failed: 0x%08X\n", hr); return; }

    ComPtr<IDXGIDevice> dxgiDevice;
    hr = m_pD3D11Device.As(&dxgiDevice);
    if (FAILED(hr)) { printf("Failed to get IDXGIDevice: 0x%08X\n", hr); return; }

    ComPtr<IDXGIAdapter> dxgiAdapter;
    hr = dxgiDevice->GetAdapter(&dxgiAdapter);
    if (FAILED(hr)) { printf("Failed to get IDXGIAdapter: 0x%08X\n", hr); return; }

    ComPtr<IDXGIFactory2> dxgiFactory;
    hr = dxgiAdapter->GetParent(IID_PPV_ARGS(&dxgiFactory));
    if (FAILED(hr)) { printf("Failed to get DXGIFactory2: 0x%08X\n", hr); return; }

    ComPtr<ID2D1Device> d2dDevice;
    hr = m_pD2DFactory->CreateDevice(dxgiDevice.Get(), &d2dDevice);
    if (FAILED(hr)) { printf("D2D Device creation failed: 0x%08X\n", hr); return; }

    hr = d2dDevice->CreateDeviceContext(D2D1_DEVICE_CONTEXT_OPTIONS_NONE, &m_pD2DDC);
    if (FAILED(hr)) { printf("D2D DeviceContext creation failed: 0x%08X\n", hr); return; }

    // 7. SwapChain 생성
    DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
    swapChainDesc.Width = m_width;
    swapChainDesc.Height = m_height;
    swapChainDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapChainDesc.BufferCount = 2;
    swapChainDesc.Scaling = DXGI_SCALING_STRETCH;
    swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
    swapChainDesc.AlphaMode = DXGI_ALPHA_MODE_PREMULTIPLIED;
    swapChainDesc.SampleDesc.Count = 1;

    hr = dxgiFactory->CreateSwapChainForComposition(m_pD3D11Device.Get(), &swapChainDesc, nullptr, &m_pSwapChain);
    if (FAILED(hr)) { printf("SwapChain creation failed: 0x%08X\n", hr); return; }

    // 8. DirectComposition 연결
    hr = DCompositionCreateDevice(dxgiDevice.Get(), IID_PPV_ARGS(&m_pDCompDevice));
    if (FAILED(hr)) { printf("DComp device creation failed: 0x%08X\n", hr); return; }

    m_pDCompDevice->CreateTargetForHwnd(m_hwnd, TRUE, &m_pDCompTarget);
    m_pDCompDevice->CreateVisual(&m_pDCompVisual);
    m_pDCompVisual->SetContent(m_pSwapChain.Get());
    m_pDCompTarget->SetRoot(m_pDCompVisual.Get());
    m_pDCompDevice->Commit();

    // 9. D2D Render Target 설정
    ComPtr<IDXGISurface> surface;
    hr = m_pSwapChain->GetBuffer(0, IID_PPV_ARGS(&surface));
    if (FAILED(hr)) { printf("GetBuffer failed: 0x%08X\n", hr); return; }

    D2D1_BITMAP_PROPERTIES1 bitmapProps =
        D2D1::BitmapProperties1(
            D2D1_BITMAP_OPTIONS_TARGET | D2D1_BITMAP_OPTIONS_CANNOT_DRAW,
            D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM,
                D2D1_ALPHA_MODE_PREMULTIPLIED)
        );

    ComPtr<ID2D1Bitmap1> targetBitmap;
    hr = m_pD2DDC->CreateBitmapFromDxgiSurface(surface.Get(), &bitmapProps, &targetBitmap);
    if (FAILED(hr)) { printf("CreateBitmapFromDxgiSurface failed: 0x%08X\n", hr); return; }

    m_pD2DDC->SetTarget(targetBitmap.Get());
}

void GraphicEngine::Release() {
    if (m_pD2DDC) m_pD2DDC->SetTarget(nullptr);
    m_pDCompVisual.Reset();
    m_pDCompTarget.Reset();
    m_pDCompDevice.Reset();
    m_pSwapChain.Reset();
    m_pD2DDC.Reset();
    m_pD3D11Device.Reset();
    m_pWICFactory.Reset();
    m_pDWriteFactory.Reset();
    m_pD2DFactory.Reset();
    CoUninitialize();
}