#include "graphicengine.h"
#include <windows.h>
#include <wincodec.h>
#include <d3d11_1.h>
#include <dxgi1_2.h>
#include <dcomp.h>
#include <wrl/client.h>
#include <stdio.h>
#include "drawcontext.h"
#include "window.h"
#include "resourcehub.h"

bool GraphicEngine::Init(HWND hwnd, WindowConfig cfg) {
    m_hwnd = hwnd;
    m_width = cfg.width;
    m_height = cfg.height;
	m_vSync = cfg.vSync;

    if (!InitCom()) return false;
    if (!InitD2D()) return false;

    m_drawContext.reset(new DrawContext(m_pD2DDC.Get(), m_pD2DFactory.Get()));
    ResourceHub::Instance().Init(
		m_pD2DDC.Get(), m_pDWriteFactory.Get(), m_pWICFactory.Get());
    return true;
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
    printf("[Win] Recreating Device...\n");
    m_pD2DDC.Reset();
    m_pD2DFactory.Reset();
    m_pSwapChain.Reset();
    m_pD3D11Device.Reset();
    InitD2D();
}

bool GraphicEngine::InitCom() {
    if (FAILED(CoInitializeEx(NULL, COINIT_APARTMENTTHREADED))) return false;

    if (FAILED(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory),
        reinterpret_cast<IUnknown**>(m_pDWriteFactory.ReleaseAndGetAddressOf())))) return false;

    if (FAILED(CoCreateInstance(CLSID_WICImagingFactory, NULL, CLSCTX_INPROC_SERVER,
        IID_PPV_ARGS(m_pWICFactory.ReleaseAndGetAddressOf())))) return false;

    return true;
}

bool GraphicEngine::InitD2D() {

    if (FAILED(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, __uuidof(ID2D1Factory1), (void**)m_pD2DFactory.GetAddressOf()))) return false;

    UINT flags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
#ifdef _DEBUG
    flags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

    if (FAILED(D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, flags, nullptr, 0, D3D11_SDK_VERSION, &m_pD3D11Device, nullptr, nullptr))) return false;

    ComPtr<IDXGIDevice> dxgiDevice;
    if (FAILED(m_pD3D11Device.As(&dxgiDevice))) return false;

    ComPtr<IDXGIAdapter> dxgiAdapter;
    if (FAILED(dxgiDevice->GetAdapter(&dxgiAdapter))) return false;

    ComPtr<IDXGIFactory2> dxgiFactory;
    if (FAILED(dxgiAdapter->GetParent(IID_PPV_ARGS(&dxgiFactory)))) return false;

    ComPtr<ID2D1Device> d2dDevice;
    if (FAILED(m_pD2DFactory->CreateDevice(dxgiDevice.Get(), &d2dDevice))) return false;
    if (FAILED(d2dDevice->CreateDeviceContext(D2D1_DEVICE_CONTEXT_OPTIONS_NONE, &m_pD2DDC))) return false;

    DXGI_SWAP_CHAIN_DESC1 scDesc = {};
    scDesc.Width = m_width;
    scDesc.Height = m_height;
    scDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    scDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    scDesc.BufferCount = 2;
    scDesc.Scaling = DXGI_SCALING_STRETCH;
    scDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
    scDesc.AlphaMode = DXGI_ALPHA_MODE_PREMULTIPLIED;
    scDesc.SampleDesc.Count = 1;

    if (FAILED(dxgiFactory->CreateSwapChainForComposition(m_pD3D11Device.Get(), &scDesc, nullptr, &m_pSwapChain))) return false;

    if (FAILED(DCompositionCreateDevice(dxgiDevice.Get(), IID_PPV_ARGS(&m_pDCompDevice)))) return false;

    m_pDCompDevice->CreateTargetForHwnd(m_hwnd, TRUE, &m_pDCompTarget);
    m_pDCompDevice->CreateVisual(&m_pDCompVisual);
    m_pDCompVisual->SetContent(m_pSwapChain.Get());
    m_pDCompTarget->SetRoot(m_pDCompVisual.Get());
    m_pDCompDevice->Commit();

    ComPtr<IDXGISurface> surface;
    if (FAILED(m_pSwapChain->GetBuffer(0, IID_PPV_ARGS(&surface)))) return false;

    D2D1_BITMAP_PROPERTIES1 props = D2D1::BitmapProperties1(
        D2D1_BITMAP_OPTIONS_TARGET | D2D1_BITMAP_OPTIONS_CANNOT_DRAW,
        D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED));

    ComPtr<ID2D1Bitmap1> targetBitmap;
    if (FAILED(m_pD2DDC->CreateBitmapFromDxgiSurface(surface.Get(), &props, &targetBitmap))) return false;

    m_pD2DDC->SetTarget(targetBitmap.Get());
    return true;
}

void GraphicEngine::Resize(int width, int height) {
    if (!m_pSwapChain || width == 0 || height == 0) return;

    m_width = width;
    m_height = height;
    m_pD2DDC->SetTarget(nullptr);

    if (FAILED(m_pSwapChain->ResizeBuffers(0, width, height, DXGI_FORMAT_UNKNOWN, 0))) return;

    ComPtr<IDXGISurface> surface;
    if (FAILED(m_pSwapChain->GetBuffer(0, IID_PPV_ARGS(&surface)))) return;

    D2D1_BITMAP_PROPERTIES1 props = D2D1::BitmapProperties1(
        D2D1_BITMAP_OPTIONS_TARGET | D2D1_BITMAP_OPTIONS_CANNOT_DRAW,
        D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED));

    ComPtr<ID2D1Bitmap1> bitmap;
    if (FAILED(m_pD2DDC->CreateBitmapFromDxgiSurface(surface.Get(), &props, &bitmap))) return;

    m_pD2DDC->SetTarget(bitmap.Get());
    if (m_pDCompDevice) m_pDCompDevice->Commit();
}

void GraphicEngine::Release() {
    ResourceHub::Instance().Shutdown();
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

void GraphicEngine::BindToLua(sol::state& lua, const char* name) {
    if (m_drawContext) m_drawContext->BindGlobal(lua, name);
}