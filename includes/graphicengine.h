#pragma once
#include <windows.h>
#include <wincodec.h>
#include <d3d11_1.h>
#include <dxgi1_2.h>
#include <dcomp.h>
#include <d2d1_1.h>
#include <dwrite.h>
#include <wrl/client.h>
#include "drawcontext.h"
#include "window.h"

struct LuaBindContext;

using Microsoft::WRL::ComPtr;

class GraphicEngine {
public:
    bool Init(HWND hwnd, WindowConfig cfg);
    void PreDraw();
    void PostDraw();
    void Release();

    void Resize(int width, int height);
    void BindToLua(LuaBindContext& ctx);

private:
    bool InitCom();
    bool InitD2D();
	void RecreateDevice();

    HWND m_hwnd;
    int m_width;
    int m_height;
    bool m_isDrawing = false; 

    // Direct2D
    ComPtr<ID2D1Factory1> m_pD2DFactory;
    ComPtr<ID2D1DeviceContext> m_pD2DDC;
    ComPtr<IDWriteFactory> m_pDWriteFactory;
    ComPtr<IWICImagingFactory> m_pWICFactory;

    // Direct3D / DXGI / DComp
    ComPtr<ID3D11Device>    m_pD3D11Device;
    ComPtr<ID3D11DeviceContext> m_pD3D11Context;
    ComPtr<IDXGISwapChain1> m_pSwapChain;
    ComPtr<IDCompositionDevice> m_pDCompDevice;
    ComPtr<IDCompositionTarget> m_pDCompTarget;
    ComPtr<IDCompositionVisual> m_pDCompVisual;

    bool m_vSync = false;
	std::unique_ptr<DrawContext> m_drawContext;

    // whether we use DComposition (needed for transparent windows)
    bool m_useComposition = false;

    // Tracks whether CoInitializeEx succeeded so we only call CoUninitialize when needed
    bool m_comInitialized = false;
};