#pragma once

#include <windows.h>
#include <gdiplus.h>
#include <wtypes.h>
#include <winerror.h>
#include <d2d1.h>
#include <wincodec.h>
#include <dwrite.h>
using namespace Gdiplus;

#include <cstdio>
#include "util.h"
#include "engine_lua.h"

#pragma comment(lib, "d2d1.lib")
#pragma comment(lib, "dwrite.lib")
#pragma comment(lib, "Gdiplus.lib")
#pragma comment(lib, "windowscodecs.lib")

int gDrawW = 0, gDrawH = 0;

ID2D1Factory* g_pD2DFactory = nullptr;
ID2D1DCRenderTarget* g_pDCRT = nullptr;
IDWriteFactory* g_pDWriteFactory = nullptr;
IWICImagingFactory* g_pWICFactory = nullptr;
HWND    g_hwnd = nullptr;
HDC     g_hdcScreen = nullptr;
HDC     g_hdcMem = nullptr;
HBITMAP g_hBmp = nullptr;
HBITMAP g_hBmpOld = nullptr;
int     g_bufW = 0;
int     g_bufH = 0;
ULONG_PTR gdiplusToken;

void InitCom() {
    GdiplusStartupInput gdiplusStartupInput;
    GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, nullptr);

    HRESULT hr = CoInitialize(NULL); // WIC와 COM 사용을 위해 필수
    hr = DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory), reinterpret_cast<IUnknown**>(&g_pDWriteFactory));
    hr = CoCreateInstance(CLSID_WICImagingFactory, NULL, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&g_pWICFactory));
}
void InitD2D() {

    D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &g_pD2DFactory);

    // DC 렌더 타겟의 속성 설정 (투명도 지원 필수)
    D2D1_RENDER_TARGET_PROPERTIES props = D2D1::RenderTargetProperties(
        D2D1_RENDER_TARGET_TYPE_DEFAULT,
        D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED),
        0, 0, D2D1_RENDER_TARGET_USAGE_NONE, D2D1_FEATURE_LEVEL_DEFAULT
    );

    // DCRT 생성 (실제 사용은 BindDC에서 함)
    g_pD2DFactory->CreateDCRenderTarget(&props, &g_pDCRT);
    RebuildAllBitmaps();
    g_bufW = 0;
    g_bufH = 0;
}

void InitWindow(WNDPROC WndProc, HINSTANCE hInstance, const wchar_t* title) {
    WNDCLASS wc = { 0 };
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = L"LayeredImageWindow";
    RegisterClass(&wc);

    g_hwnd = CreateWindowEx(WS_EX_LAYERED | WS_EX_TOPMOST, wc.lpszClassName, title,
        WS_POPUP, 200, 200, gDrawW, gDrawH, nullptr, nullptr, hInstance, nullptr);
    g_hdcScreen = GetDC(g_hwnd);
}

void RefreshBackBuffer(int w, int h) {
    if (g_hBmp) {
        SelectObject(g_hdcMem, g_hBmpOld);
        DeleteObject(g_hBmp);
        g_hBmp = nullptr;
    }

    if (!g_hdcScreen)
        g_hdcScreen = GetDC(g_hwnd);

    if (!g_hdcMem)
        g_hdcMem = CreateCompatibleDC(g_hdcScreen);

    BITMAPINFO bmi = {};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = w;
    bmi.bmiHeader.biHeight = -h; // top-down
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    void* bits = nullptr;
    g_hBmp = CreateDIBSection(
        g_hdcScreen,
        &bmi,
        DIB_RGB_COLORS,
        &bits,
        nullptr,
        0
    );

    if (!g_hBmp) return;

    g_hBmpOld = (HBITMAP)SelectObject(g_hdcMem, g_hBmp);

    g_bufW = w;
    g_bufH = h;
    if (g_pDCRT) {
        RECT rc = { 0, 0, w, h };
        g_pDCRT->BindDC(g_hdcMem, &rc);
    }
}
void PreDraw() {

    int w = gDrawW;
    int h = gDrawH;
    if (w <= 0 || h <= 0) return;

    // 1. 백버퍼 생성/재생성 (기존 로직 유지)
    if (!g_hBmp || gDrawW != g_bufW || gDrawH != g_bufH) {
        RefreshBackBuffer(gDrawW, gDrawH);
    }
    g_pDCRT->BeginDraw();
    g_pDCRT->SetTransform(D2D1::Matrix3x2F::Identity());
    g_pDCRT->Clear(D2D1::ColorF(0, 0, 0, 0)); // GPU 가속 클리어
}

void PostDraw() {
    HRESULT hr = g_pDCRT->EndDraw();
    if (hr != S_OK) {
        if (hr == D2DERR_RECREATE_TARGET) {
            SafeRelease(&g_pDCRT);
            InitD2D();
            g_bufW = 0;
            g_bufH = 0;
            return;
        }
    }

    // 5. 레이어드 윈도우 갱신 (기존 GDI 로직 그대로 사용)
    RECT winRc; GetWindowRect(g_hwnd, &winRc);
    POINT ptWinPos = { winRc.left, winRc.top };
    SIZE sizeWin = { gDrawW, gDrawH };
    POINT ptSrc = { 0, 0 };

    BLENDFUNCTION blend = { AC_SRC_OVER, 0, 255, AC_SRC_ALPHA };

    BOOL ok = UpdateLayeredWindow(
        g_hwnd, g_hdcScreen, &ptWinPos, &sizeWin,
        g_hdcMem, &ptSrc, 0, &blend, ULW_ALPHA
    );
    if (!ok) {
        DWORD err = GetLastError(); printf("UpdateLayeredWindow failed, error=%lu\n", err);
    }
}

void ReleaseGraphic() {
    if (g_pDCRT) g_pDCRT->Release();

    if (g_pWICFactory) g_pWICFactory->Release();
    if (g_pDWriteFactory) g_pDWriteFactory->Release();
    if (g_pD2DFactory) g_pD2DFactory->Release();
    CoUninitialize();
    ReleaseDC(g_hwnd, g_hdcScreen);
    GdiplusShutdown(gdiplusToken);
}