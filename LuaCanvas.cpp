#include "LuaCanvas.h"
#include "engine_graphic.h"

#ifndef SafeRelease
#define SafeRelease(p) { if(p) { (p)->Release(); (p) = nullptr; } }
#endif
LuaCanvas::LuaCanvas(float w, float h) {
    // 1. 호환되는 렌더 타겟 생성
    HRESULT hr = g_pD2DDC->CreateCompatibleRenderTarget(D2D1::SizeF(w, h), &pRT);
    if (SUCCEEDED(hr)) {
        // 2. 이 캔버스 전용 브러시 생성
        pRT->CreateSolidColorBrush(D2D1::ColorF(1.0f, 1.0f, 1.0f), &pCanvasBrush);
    }
}

void LuaCanvas::release() {
    if (isBatching && pRT) {
        pRT->EndDraw();
        isBatching = false;
    }

    pCanvasBrush.Reset();
    pRT.Reset();
}

// 4. 소멸자 (Lua GC에 의해 호출됨)
LuaCanvas::~LuaCanvas() {
    release();
}

void LuaCanvas::batchBegin() {
    if (!isBatching) { pRT->BeginDraw(); isBatching = true; }
}

void LuaCanvas::batchEnd() {
    if (isBatching) { pRT->EndDraw(); isBatching = false; }
}

void LuaCanvas::execute(std::function<void()> drawFunc) {
    bool autoEnd = false;
    if (!isBatching) { pRT->BeginDraw(); autoEnd = true; }
    drawFunc();
    if (autoEnd) pRT->EndDraw();
}

void LuaCanvas::color(float r, float g, float b, sol::optional<float> a) {
    pCanvasBrush->SetColor(D2D1::ColorF(r / 255.0f, g / 255.0f, b / 255.0f, a.value_or(255.0f) / 255.0f));
}

void LuaCanvas::rect(float x, float y, float w, float h, bool fill) {
    execute([&]() { DrawCore::Rect(pRT.Get(), pCanvasBrush.Get(), x, y, w, h, fill); });
}

void LuaCanvas::circle(float x, float y, float r, bool fill) {
    execute([&]() { DrawCore::Circle(pRT.Get(), pCanvasBrush.Get(), x, y, r, fill); });
}

void LuaCanvas::polyline(sol::table vertices, bool closed, float strokeWidth) {
    execute([&]() { DrawCore::Polyline(pRT.Get(), pCanvasBrush.Get(), vertices, closed, strokeWidth); });
}

void LuaCanvas::polygon(sol::table vertices) {
    execute([&]() { DrawCore::Polygon(pRT.Get(), pCanvasBrush.Get(), vertices); });
}

void LuaCanvas::text(int fontId, std::string name, float x, float y) {
    if (fontId < 0 || fontId >= g_fontTable.size()) return;
    execute([&]() { DrawCore::Text(pRT.Get(), pCanvasBrush.Get(), g_fontTable[fontId], name, x, y); });
}

void LuaCanvas::image(int id, float dx, float dy, sol::optional<float> dw, sol::optional<float> dh,
    sol::optional<float> sx, sol::optional<float> sy, sol::optional<float> sw, sol::optional<float> sh) {
    if (id < 0 || id >= g_bitmapTable.size()) return;
    ID2D1Bitmap* bmp = g_bitmapTable[id];
    auto s = bmp->GetSize();
    execute([&]() {
        DrawCore::Image(pRT.Get(), bmp, dx, dy, dw.value_or(s.width), dh.value_or(s.height),
            sx.value_or(0), sy.value_or(0), sw.value_or(s.width), sh.value_or(s.height));
        });
}

void LuaCanvas::draw(float x, float y,
    sol::optional<float> w, sol::optional<float> h,
    sol::optional<float> sx, sol::optional<float> sy,
    sol::optional<float> sw, sol::optional<float> sh) {
    if (isBatching) batchEnd();

    ComPtr<ID2D1Bitmap> pBitmap;
    HRESULT hr = pRT->GetBitmap(&pBitmap);

    if (SUCCEEDED(hr) && pBitmap) {
        D2D1_SIZE_F originalSize = pBitmap->GetSize();

        // 1. 소스 영역 결정 (이미지의 어디를 얼마만큼 잘라낼 것인가)
        float sourceX = sx.value_or(0.0f);
        float sourceY = sy.value_or(0.0f);
        float sourceW = sw.value_or(originalSize.width - sourceX);
        float sourceH = sh.value_or(originalSize.height - sourceY);

        D2D1_RECT_F srcRect = D2D1::RectF(sourceX, sourceY, sourceX + sourceW, sourceY + sourceH);

        // 2. 목적지 영역 결정 (화면 어디에 어떤 크기로 그릴 것인가)
        // w, h가 없으면 잘라낸 소스 크기(sourceW, sourceH) 그대로 그립니다.
        float drawW = w.value_or(sourceW);
        float drawH = h.value_or(sourceH);

        D2D1_RECT_F destRect = D2D1::RectF(x, y, x + drawW, y + drawH);

        // 3. 출력 (픽셀 아트를 위해 NEAREST_NEIGHBOR)
        g_pD2DDC->DrawBitmap(
            pBitmap.Get(),
            destRect,
            1.0f,
            D2D1_BITMAP_INTERPOLATION_MODE_NEAREST_NEIGHBOR,
            &srcRect
        );
    }
}