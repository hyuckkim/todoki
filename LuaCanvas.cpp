#include "LuaCanvas.h"

// 외부 전역 변수 참조 (엔진 구조에 맞게 조정하세요)
extern ID2D1DCRenderTarget* g_pDCRT;
extern std::vector<IDWriteTextFormat*> g_fontTable;
extern std::vector<ID2D1Bitmap*> g_bitmapTable;

LuaCanvas::LuaCanvas(float w, float h) {
    // 1. 호환되는 렌더 타겟 생성
    g_pDCRT->CreateCompatibleRenderTarget(D2D1::SizeF(w, h), &pRT);
    // 2. 이 캔버스 전용 브러시 생성
    pRT->CreateSolidColorBrush(D2D1::ColorF(1.0f, 1.0f, 1.0f), &pCanvasBrush);
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

void LuaCanvas::draw(float x, float y) {
    if (isBatching) batchEnd();
    ComPtr<ID2D1Bitmap> pBitmap;
    pRT->GetBitmap(&pBitmap); // 그려진 내용 비트맵으로 추출
    if (pBitmap) {
        // 메인 화면(g_pDCRT)에 캔버스 비트맵을 그림
        g_pDCRT->DrawBitmap(pBitmap.Get(), D2D1::RectF(x, y, x + pBitmap->GetSize().width, y + pBitmap->GetSize().height));
    }
}