#include "graphicengine.h"
#include <windows.h>
#include <wincodec.h>
#include <d3d11_1.h>
#include <dxgi1_2.h>
#include <dcomp.h>
#include <d3dcompiler.h>
#include <d2d1_1.h>
#include <algorithm>
#include <cstring>
#include <wrl/client.h>
#include <stdio.h>
#include "drawcontext.h"
#include "window.h"
#include "resourcehub.h"
#include "luabind.h"

using Microsoft::WRL::ComPtr;

namespace {
    HRESULT GetOrCreateFullscreenVS(ID3D11Device* device, ID3D11VertexShader** outVs) {
        if (!device || !outVs) return E_INVALIDARG;

        static ComPtr<ID3D11VertexShader> s_vs;
        if (s_vs) {
            *outVs = s_vs.Get();
            (*outVs)->AddRef();
            return S_OK;
        }

        static const char* kVsSrc = R"(
struct VSOut {
    float4 pos : SV_POSITION;
    float2 uv  : TEXCOORD0;
};
VSOut main(uint vid : SV_VertexID)
{
    float2 pos[3] = {
        float2(-1.0, -1.0),
        float2(-1.0,  3.0),
        float2( 3.0, -1.0)
    };
    float2 uv[3] = {
        float2(0.0, 1.0),
        float2(0.0, -1.0),
        float2(2.0, 1.0)
    };
    VSOut o;
    o.pos = float4(pos[vid], 0.0, 1.0);
    o.uv = uv[vid];
    return o;
}
)";
        ComPtr<ID3DBlob> vsBlob;
        ComPtr<ID3DBlob> errBlob;
        UINT flags = D3DCOMPILE_ENABLE_STRICTNESS;
#ifdef _DEBUG
        flags |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif
        HRESULT hr = D3DCompile(
            kVsSrc,
            strlen(kVsSrc),
            nullptr,
            nullptr,
            nullptr,
            "main",
            "vs_5_0",
            flags,
            0,
            &vsBlob,
            &errBlob);

        if (FAILED(hr) || !vsBlob) {
            return FAILED(hr) ? hr : E_FAIL;
        }

        hr = device->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), nullptr, &s_vs);
        if (FAILED(hr)) return hr;

        *outVs = s_vs.Get();
        (*outVs)->AddRef();
        return S_OK;
    }

    HRESULT GetOrCreateLinearSampler(ID3D11Device* device, ID3D11SamplerState** outSampler) {
        if (!device || !outSampler) return E_INVALIDARG;

        static ComPtr<ID3D11SamplerState> s_sampler;
        if (s_sampler) {
            *outSampler = s_sampler.Get();
            (*outSampler)->AddRef();
            return S_OK;
        }

        D3D11_SAMPLER_DESC desc = {};
        desc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
        desc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
        desc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
        desc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
        desc.MinLOD = 0;
        desc.MaxLOD = D3D11_FLOAT32_MAX;

        HRESULT hr = device->CreateSamplerState(&desc, &s_sampler);
        if (FAILED(hr)) return hr;

        *outSampler = s_sampler.Get();
        (*outSampler)->AddRef();
        return S_OK;
    }

    HRESULT GetOrCreateNoCullRasterizer(ID3D11Device* device, ID3D11RasterizerState** outRs) {
        if (!device || !outRs) return E_INVALIDARG;

        static ComPtr<ID3D11RasterizerState> s_rs;
        if (s_rs) {
            *outRs = s_rs.Get();
            (*outRs)->AddRef();
            return S_OK;
        }

        D3D11_RASTERIZER_DESC rd = {};
        rd.FillMode = D3D11_FILL_SOLID;
        rd.CullMode = D3D11_CULL_NONE;
        rd.FrontCounterClockwise = FALSE;
        rd.DepthClipEnable = TRUE;

        HRESULT hr = device->CreateRasterizerState(&rd, &s_rs);
        if (FAILED(hr)) return hr;

        *outRs = s_rs.Get();
        (*outRs)->AddRef();
        return S_OK;
    }
}

bool GraphicEngine::Init(HWND hwnd, WindowConfig cfg) {
    m_hwnd = hwnd;
    m_width = cfg.getWidth();
    m_height = cfg.getHeight();
	m_vSync = cfg.getVSync();
    m_useComposition = cfg.getTransparent();

    if (!InitCom()) return false;
    if (!InitD2D()) return false;

    m_drawContext.reset(new DrawContext(m_pD2DDC.Get(), m_pD2DFactory.Get(), this));
    ResourceHub::Instance().Init(
		m_pD2DDC.Get(), m_pDWriteFactory.Get(), m_pWICFactory.Get(), this);
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
    m_pD3D11Context.Reset();
    m_pD3D11Device.Reset();
    InitD2D();
}

bool GraphicEngine::InitCom() {
    HRESULT hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    if (FAILED(hr)) { printf("InitCom: CoInitializeEx failed: 0x%08X\n", hr); return false; }
    m_comInitialized = true;
    hr = DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory),
        reinterpret_cast<IUnknown**>(m_pDWriteFactory.ReleaseAndGetAddressOf()));
    if (FAILED(hr)) {
        printf("InitCom: DWriteCreateFactory failed: 0x%08X\n", hr);
        CoUninitialize();
        m_comInitialized = false;
        return false;
    }

    hr = CoCreateInstance(CLSID_WICImagingFactory, NULL, CLSCTX_INPROC_SERVER,
        IID_PPV_ARGS(m_pWICFactory.ReleaseAndGetAddressOf()));
    if (FAILED(hr)) {
        printf("InitCom: CoCreateInstance(WIC) failed: 0x%08X\n", hr);
        CoUninitialize();
        m_comInitialized = false;
        return false;
    }

    return true;
}

bool GraphicEngine::InitD2D() {
    HRESULT hr = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, __uuidof(ID2D1Factory1), (void**)m_pD2DFactory.GetAddressOf());
    if (FAILED(hr)) { printf("InitD2D: D2D1CreateFactory failed: 0x%08X\n", hr); return false; }

    UINT flags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
#ifdef _DEBUG
    flags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

    hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, flags, nullptr, 0, D3D11_SDK_VERSION, &m_pD3D11Device, nullptr, &m_pD3D11Context);
    if (FAILED(hr)) { printf("InitD2D: D3D11CreateDevice failed: 0x%08X\n", hr); return false; }

    ComPtr<IDXGIDevice> dxgiDevice;
    hr = m_pD3D11Device.As(&dxgiDevice);
    if (FAILED(hr)) { printf("InitD2D: ID3D11Device->QueryInterface(IDXGIDevice) failed: 0x%08X\n", hr); return false; }

    ComPtr<IDXGIAdapter> dxgiAdapter;
    hr = dxgiDevice->GetAdapter(&dxgiAdapter);
    if (FAILED(hr)) { printf("InitD2D: GetAdapter failed: 0x%08X\n", hr); return false; }

    ComPtr<IDXGIFactory2> dxgiFactory;
    hr = dxgiAdapter->GetParent(IID_PPV_ARGS(&dxgiFactory));
    if (FAILED(hr)) { printf("InitD2D: GetParent(IDXGIFactory2) failed: 0x%08X\n", hr); return false; }

    ComPtr<ID2D1Device> d2dDevice;
    hr = m_pD2DFactory->CreateDevice(dxgiDevice.Get(), &d2dDevice);
    if (FAILED(hr)) { printf("InitD2D: CreateDevice failed: 0x%08X\n", hr); return false; }

    hr = d2dDevice->CreateDeviceContext(D2D1_DEVICE_CONTEXT_OPTIONS_NONE, &m_pD2DDC);
    if (FAILED(hr)) { printf("InitD2D: CreateDeviceContext failed: 0x%08X\n", hr); return false; }

    DXGI_SWAP_CHAIN_DESC1 scDesc = {};
    scDesc.Width = m_width;
    scDesc.Height = m_height;
    scDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    scDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    scDesc.BufferCount = 2;
    scDesc.Scaling = DXGI_SCALING_STRETCH;
    scDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
    // Use premultiplied alpha only for composition (transparent) swapchains.
    scDesc.AlphaMode = m_useComposition ? DXGI_ALPHA_MODE_PREMULTIPLIED : DXGI_ALPHA_MODE_IGNORE;
    scDesc.SampleDesc.Count = 1;

    if (m_useComposition) {
        // For transparent windows we use composition swapchain + DComposition
        hr = dxgiFactory->CreateSwapChainForComposition(m_pD3D11Device.Get(), &scDesc, nullptr, &m_pSwapChain);
        if (FAILED(hr)) { printf("InitD2D: CreateSwapChainForComposition failed: 0x%08X\n", hr); return false; }

        hr = DCompositionCreateDevice(dxgiDevice.Get(), IID_PPV_ARGS(&m_pDCompDevice));
        if (FAILED(hr)) { printf("InitD2D: DCompositionCreateDevice failed: 0x%08X\n", hr); return false; }

        m_pDCompDevice->CreateTargetForHwnd(m_hwnd, TRUE, &m_pDCompTarget);
        m_pDCompDevice->CreateVisual(&m_pDCompVisual);
        m_pDCompVisual->SetContent(m_pSwapChain.Get());
        m_pDCompTarget->SetRoot(m_pDCompVisual.Get());
        m_pDCompDevice->Commit();
    }
    else {
        // For opaque windows create a normal HWND-bound swapchain
        hr = dxgiFactory->CreateSwapChainForHwnd(m_pD3D11Device.Get(), m_hwnd, &scDesc, nullptr, nullptr, &m_pSwapChain);
        if (FAILED(hr)) { printf("InitD2D: CreateSwapChainForHwnd failed: 0x%08X\n", hr); return false; }
    }

    ComPtr<IDXGISurface> surface;
    hr = m_pSwapChain->GetBuffer(0, IID_PPV_ARGS(&surface));
    if (FAILED(hr)) { printf("InitD2D: GetBuffer failed: 0x%08X\n", hr); return false; }

    D2D1_BITMAP_PROPERTIES1 props = D2D1::BitmapProperties1(
        D2D1_BITMAP_OPTIONS_TARGET | D2D1_BITMAP_OPTIONS_CANNOT_DRAW,
        D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED));

    ComPtr<ID2D1Bitmap1> targetBitmap;
    hr = m_pD2DDC->CreateBitmapFromDxgiSurface(surface.Get(), &props, &targetBitmap);
    if (FAILED(hr)) { printf("InitD2D: CreateBitmapFromDxgiSurface failed: 0x%08X\n", hr); return false; }

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
    // If a draw is in progress, end it first to avoid device state issues
    if (m_isDrawing && m_pD2DDC) {
        m_pD2DDC->EndDraw();
        m_isDrawing = false;
    }

    // Release draw-related context first so it doesn't hold references to D2D/D3D objects
    m_drawContext.reset();

    ResourceHub::Instance().Shutdown();

    if (m_pD2DDC) m_pD2DDC->SetTarget(nullptr);
    m_pDCompVisual.Reset();
    m_pDCompTarget.Reset();
    m_pDCompDevice.Reset();
    m_pSwapChain.Reset();
    m_pD2DDC.Reset();
    m_pD3D11Context.Reset();
    m_pD3D11Device.Reset();
    m_pWICFactory.Reset();
    m_pDWriteFactory.Reset();
    m_pD2DFactory.Reset();

    if (m_comInitialized) {
        CoUninitialize();
        m_comInitialized = false;
    }
}

void GraphicEngine::BindToLua(LuaBindContext& ctx) {
    if (m_drawContext) {
        m_drawContext->BindGlobal(ctx);
        DrawContext::BindClass(ctx);
    }
}

bool GraphicEngine::RenderOffscreenWithShader(
    ID2D1Bitmap1* sourceBitmap,
    int shaderId,
    ID2D1RenderTarget* targetRT,
    float dx, float dy, float dw, float dh,
    float sx, float sy, float sw, float sh,
    float alpha)
{
    if (!sourceBitmap || !targetRT || shaderId < 0) return false;
    if (!m_pD3D11Device || !m_pD3D11Context) return false;

    ID3D11PixelShader* ps = ResourceHub::Instance().GetPixelShader(shaderId);
    if (!ps) return false;

    ComPtr<IDXGISurface> srcSurface;
    if (FAILED(sourceBitmap->GetSurface(&srcSurface)) || !srcSurface) return false;

    ComPtr<ID3D11Texture2D> srcTex;
    if (FAILED(srcSurface.As(&srcTex)) || !srcTex) return false;

    D3D11_TEXTURE2D_DESC srcDesc = {};
    srcTex->GetDesc(&srcDesc);

    UINT srcX = (UINT)(std::max)(0.0f, sx);
    UINT srcY = (UINT)(std::max)(0.0f, sy);
    UINT srcW = (UINT)(std::max)(1.0f, sw);
    UINT srcH = (UINT)(std::max)(1.0f, sh);
    if (srcX + srcW > srcDesc.Width) srcW = (srcDesc.Width > srcX) ? (srcDesc.Width - srcX) : 1;
    if (srcY + srcH > srcDesc.Height) srcH = (srcDesc.Height > srcY) ? (srcDesc.Height - srcY) : 1;

    D3D11_TEXTURE2D_DESC croppedDesc = {};
    croppedDesc.Width = srcW;
    croppedDesc.Height = srcH;
    croppedDesc.MipLevels = 1;
    croppedDesc.ArraySize = 1;
    croppedDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    croppedDesc.SampleDesc.Count = 1;
    croppedDesc.Usage = D3D11_USAGE_DEFAULT;
    croppedDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

    ComPtr<ID3D11Texture2D> croppedTex;
    if (FAILED(m_pD3D11Device->CreateTexture2D(&croppedDesc, nullptr, &croppedTex)) || !croppedTex) return false;

    D3D11_BOX srcBox = {};
    srcBox.left = srcX;
    srcBox.top = srcY;
    srcBox.front = 0;
    srcBox.right = srcX + srcW;
    srcBox.bottom = srcY + srcH;
    srcBox.back = 1;
    m_pD3D11Context->CopySubresourceRegion(croppedTex.Get(), 0, 0, 0, 0, srcTex.Get(), 0, &srcBox);

    UINT outW = (UINT)(std::max)(1.0f, dw);
    UINT outH = (UINT)(std::max)(1.0f, dh);

    D3D11_TEXTURE2D_DESC outDesc = {};
    outDesc.Width = outW;
    outDesc.Height = outH;
    outDesc.MipLevels = 1;
    outDesc.ArraySize = 1;
    outDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    outDesc.SampleDesc.Count = 1;
    outDesc.Usage = D3D11_USAGE_DEFAULT;
    outDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;

    ComPtr<ID3D11Texture2D> outTex;
    if (FAILED(m_pD3D11Device->CreateTexture2D(&outDesc, nullptr, &outTex)) || !outTex) return false;

    ComPtr<ID3D11RenderTargetView> outRtv;
    if (FAILED(m_pD3D11Device->CreateRenderTargetView(outTex.Get(), nullptr, &outRtv)) || !outRtv) return false;

    ComPtr<ID3D11ShaderResourceView> srcSrv;
    if (FAILED(m_pD3D11Device->CreateShaderResourceView(croppedTex.Get(), nullptr, &srcSrv)) || !srcSrv) return false;

    ComPtr<ID3D11VertexShader> vs;
    if (FAILED(GetOrCreateFullscreenVS(m_pD3D11Device.Get(), &vs))) return false;

    ComPtr<ID3D11SamplerState> sampler;
    if (FAILED(GetOrCreateLinearSampler(m_pD3D11Device.Get(), &sampler))) return false;

    ComPtr<ID3D11RasterizerState> rs;
    if (FAILED(GetOrCreateNoCullRasterizer(m_pD3D11Device.Get(), &rs))) return false;

    D3D11_VIEWPORT vp = {};
    vp.Width = (FLOAT)outW;
    vp.Height = (FLOAT)outH;
    vp.MinDepth = 0.0f;
    vp.MaxDepth = 1.0f;

    m_pD3D11Context->OMSetRenderTargets(1, outRtv.GetAddressOf(), nullptr);
    m_pD3D11Context->RSSetState(rs.Get());
    m_pD3D11Context->RSSetViewports(1, &vp);
    m_pD3D11Context->IASetInputLayout(nullptr);
    m_pD3D11Context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    m_pD3D11Context->VSSetShader(vs.Get(), nullptr, 0);
    m_pD3D11Context->PSSetShader(ps, nullptr, 0);
    m_pD3D11Context->PSSetShaderResources(0, 1, srcSrv.GetAddressOf());
    m_pD3D11Context->PSSetSamplers(0, 1, sampler.GetAddressOf());
    m_pD3D11Context->Draw(3, 0);

    ID3D11ShaderResourceView* nullSrv = nullptr;
    m_pD3D11Context->PSSetShaderResources(0, 1, &nullSrv);

    ComPtr<IDXGISurface> outSurface;
    if (FAILED(outTex.As(&outSurface)) || !outSurface) return false;

    ComPtr<ID2D1DeviceContext> dc;
    if (FAILED(targetRT->QueryInterface(__uuidof(ID2D1DeviceContext), (void**)&dc)) || !dc) return false;

    D2D1_BITMAP_PROPERTIES1 outProps = D2D1::BitmapProperties1(
        D2D1_BITMAP_OPTIONS_NONE,
        D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED));

    ComPtr<ID2D1Bitmap1> outBitmap;
    if (FAILED(dc->CreateBitmapFromDxgiSurface(outSurface.Get(), &outProps, &outBitmap)) || !outBitmap) return false;

    targetRT->DrawBitmap(outBitmap.Get(),
        D2D1::RectF(dx, dy, dx + dw, dy + dh),
        alpha,
        D2D1_BITMAP_INTERPOLATION_MODE_LINEAR,
        D2D1::RectF(0.0f, 0.0f, (float)outW, (float)outH));

    return true;
}

bool GraphicEngine::DrawOffscreenFallback(
    ID2D1Bitmap1* sourceBitmap,
    ID2D1RenderTarget* sourceRT,
    ID2D1RenderTarget* targetRT,
    float dx, float dy, float dw, float dh,
    float sx, float sy, float sw, float sh,
    float alpha)
{
    if (!targetRT) return false;

    if (sourceBitmap) {
        ComPtr<ID2D1DeviceContext> dstDc;
        if (SUCCEEDED(targetRT->QueryInterface(__uuidof(ID2D1DeviceContext), (void**)&dstDc)) && dstDc) {
            dstDc->DrawImage(
                sourceBitmap,
                D2D1::Point2F(dx, dy),
                D2D1::RectF(sx, sy, sx + sw, sy + sh),
                D2D1_INTERPOLATION_MODE_NEAREST_NEIGHBOR,
                D2D1_COMPOSITE_MODE_SOURCE_OVER);
        }
        else {
            targetRT->DrawBitmap(sourceBitmap,
                D2D1::RectF(dx, dy, dx + dw, dy + dh),
                alpha,
                D2D1_BITMAP_INTERPOLATION_MODE_NEAREST_NEIGHBOR,
                D2D1::RectF(sx, sy, sx + sw, sy + sh));
        }
        return true;
    }

    if (sourceRT) {
        ComPtr<ID2D1BitmapRenderTarget> bitmapRT;
        if (SUCCEEDED(sourceRT->QueryInterface(__uuidof(ID2D1BitmapRenderTarget), (void**)&bitmapRT)) && bitmapRT) {
            ComPtr<ID2D1Bitmap> bmp;
            if (SUCCEEDED(bitmapRT->GetBitmap(&bmp)) && bmp) {
                targetRT->DrawBitmap(bmp.Get(),
                    D2D1::RectF(dx, dy, dx + dw, dy + dh),
                    alpha,
                    D2D1_BITMAP_INTERPOLATION_MODE_NEAREST_NEIGHBOR,
                    D2D1::RectF(sx, sy, sx + sw, sy + sh));
                return true;
            }
        }
    }

    return false;
}