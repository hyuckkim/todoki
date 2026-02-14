#pragma once

#include <windows.h>
#include <d2d1_1.h>
#include <dwrite.h>
#include <wincodec.h>
#include <string>
#include <vector>
#include <wrl/client.h>

// Microsoft::WRL::ComPtr을 사용하기 쉽게 별칭 설정
using Microsoft::WRL::ComPtr;

// --- 전역 설정 및 상태 ---
extern int gDrawW;
extern int gDrawH;
extern HWND g_hwnd;

extern std::vector<ComPtr<ID2D1Bitmap>> g_bitmapTable;
extern std::vector<ComPtr<IDWriteTextFormat>> g_fontTable;

// --- 핵심 엔진 컴포넌트 (Extern 선언) ---
extern ComPtr<ID2D1Factory1> g_pD2DFactory;
extern ComPtr<ID2D1DeviceContext> g_pD2DDC;
extern ComPtr<IDWriteFactory> g_pDWriteFactory;
extern ComPtr<IWICImagingFactory> g_pWICFactory;

extern bool g_vSync;
extern int g_targetFPS;
// --- 초기화 및 제어 함수 ---
void InitCom();
void InitD2D();
void InitWindow(WNDPROC WndProc, HINSTANCE hInstance, const wchar_t* title);

// 루프에서 호출되는 핵심 함수
void UpdateMousePassthrough();
void PreDraw();
void PostDraw();
void ReleaseGraphic();
void RecreateDevice();
void ResizeWindow(UINT width, UINT height);
