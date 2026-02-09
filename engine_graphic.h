#pragma once

#include <windows.h>
#include <d2d1.h>
#include <wincodec.h>
#include <dwrite.h>
#include <vector>
#include <map>
#include <string>

void InitCom();
void InitD2D();
void InitWindow(WNDPROC WndProc, HINSTANCE hInstance, const wchar_t* title);
void PreDraw();
void PostDraw();
void ReleaseGraphic();

extern HWND g_hwnd;
extern ID2D1Factory* g_pD2DFactory;
extern ID2D1DCRenderTarget* g_pDCRT;
extern IDWriteFactory* g_pDWriteFactory;
extern IWICImagingFactory* g_pWICFactory;

extern int gDrawW, gDrawH;
extern std::vector<ID2D1Bitmap*> g_bitmapTable;
extern std::vector<IDWriteTextFormat*> g_fontTable;
extern std::map<std::string, int> g_pathCache;