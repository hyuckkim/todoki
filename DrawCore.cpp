#include "lua_engine.h"

namespace DrawCore {
    void Rect(ID2D1RenderTarget* rt, ID2D1SolidColorBrush* brush, float x, float y, float w, float h, bool fill) {
        if (fill) rt->FillRectangle(D2D1::RectF(x, y, x + w, y + h), brush);
        else rt->DrawRectangle(D2D1::RectF(x, y, x + w, y + h), brush);
    }

    void Circle(ID2D1RenderTarget* rt, ID2D1SolidColorBrush* brush, float x, float y, float radius, bool fill) {
        D2D1_ELLIPSE ellipse = D2D1::Ellipse(D2D1::Point2F(x, y), radius, radius);
        if (fill) rt->FillEllipse(ellipse, brush);
        else rt->DrawEllipse(ellipse, brush, 1.0f);
    }

    void Polyline(ID2D1RenderTarget* rt, ID2D1SolidColorBrush* brush, sol::table vertices, bool closed, float strokeWidth) {
        if (vertices.size() < 4) return;
        ComPtr<ID2D1PathGeometry> pPath;
        g_pD2DFactory->CreatePathGeometry(&pPath);
        ComPtr<ID2D1GeometrySink> pSink;
        pPath->Open(&pSink);
        pSink->BeginFigure(D2D1::Point2F(vertices[1], vertices[2]), D2D1_FIGURE_BEGIN_FILLED);
        for (size_t i = 3; i < vertices.size(); i += 2) pSink->AddLine(D2D1::Point2F(vertices[i], vertices[i + 1]));
        pSink->EndFigure(closed ? D2D1_FIGURE_END_CLOSED : D2D1_FIGURE_END_OPEN);
        pSink->Close();
        rt->DrawGeometry(pPath.Get(), brush, strokeWidth);
    }

    void Polygon(ID2D1RenderTarget* rt, ID2D1SolidColorBrush* brush, sol::table vertices) {
        if (vertices.size() < 6) return;
        ComPtr<ID2D1PathGeometry> pPath;
        g_pD2DFactory->CreatePathGeometry(&pPath);
        ComPtr<ID2D1GeometrySink> pSink;
        pPath->Open(&pSink);
        pSink->BeginFigure(D2D1::Point2F(vertices[1], vertices[2]), D2D1_FIGURE_BEGIN_FILLED);
        for (size_t i = 3; i < vertices.size(); i += 2) pSink->AddLine(D2D1::Point2F(vertices[i], vertices[i + 1]));
        pSink->EndFigure(D2D1_FIGURE_END_CLOSED);
        pSink->Close();
        rt->FillGeometry(pPath.Get(), brush);
    }

    void Text(ID2D1RenderTarget* rt, ID2D1SolidColorBrush* brush, IDWriteTextFormat* pFormat, const std::string& text, float x, float y) {
        std::wstring wText = to_wstring(text);
        D2D1_RECT_F rect = D2D1::RectF(x, y, 10000.0f, 10000.0f);
        rt->DrawText(wText.c_str(), (UINT32)wText.length(), pFormat, rect, brush);
    }

    void Image(ID2D1RenderTarget* rt, ID2D1Bitmap* bmp, float dx, float dy, float dw, float dh, float sx, float sy, float sw, float sh) {
        D2D1_RECT_F destRect = D2D1::RectF(dx, dy, dx + dw, dy + dh);
        D2D1_RECT_F srcRect = D2D1::RectF(sx, sy, sx + sw, sy + sh);
        rt->DrawBitmap(bmp, destRect, 1.0f, D2D1_BITMAP_INTERPOLATION_MODE_LINEAR, srcRect);
    }
}