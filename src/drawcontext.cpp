#include "DrawContext.h"
#include <d2d1_1.h>
#include <wrl/client.h>

using Microsoft::WRL::ComPtr;

DrawContext::DrawContext(ID2D1DeviceContext* context, ID2D1Factory1* factory)
    : m_dc(context), m_factory(factory)
{
    if (m_dc) {
        m_dc->CreateSolidColorBrush(m_color, &m_brush);
    }
}

DrawContext::~DrawContext() {
    m_brush.Reset();
    m_dc.Reset();
}

// --- 그리기 함수 ---
void DrawContext::rect(float x, float y, float w, float h, bool fill) {
    if (!m_dc || !m_brush) return;
    if (fill) m_dc->FillRectangle(D2D1::RectF(x, y, x + w, y + h), m_brush.Get());
    else m_dc->DrawRectangle(D2D1::RectF(x, y, x + w, y + h), m_brush.Get(), m_strokeWidth);
}

void DrawContext::circle(float x, float y, float radius, bool fill) {
    if (!m_dc || !m_brush) return;
    D2D1_ELLIPSE ellipse = D2D1::Ellipse(D2D1::Point2F(x, y), radius, radius);
    if (fill) m_dc->FillEllipse(ellipse, m_brush.Get());
    else m_dc->DrawEllipse(ellipse, m_brush.Get(), m_strokeWidth);
}

void DrawContext::polyline(const std::vector<D2D1_POINT_2F>& points, bool closed) {
    if (points.size() < 2 || !m_dc || !m_brush) return;
    ComPtr<ID2D1PathGeometry> pPath;
    m_factory->CreatePathGeometry(&pPath);
    ComPtr<ID2D1GeometrySink> sink;
    pPath->Open(&sink);
    sink->BeginFigure(points[0], D2D1_FIGURE_BEGIN_FILLED);
    for (size_t i = 1; i < points.size(); ++i)
        sink->AddLine(points[i]);
    sink->EndFigure(closed ? D2D1_FIGURE_END_CLOSED : D2D1_FIGURE_END_OPEN);
    sink->Close();
    m_dc->DrawGeometry(pPath.Get(), m_brush.Get(), m_strokeWidth);
}

void DrawContext::polygon(const std::vector<D2D1_POINT_2F>& points) {
    if (points.size() < 3 || !m_dc || !m_brush) return;
    ComPtr<ID2D1PathGeometry> pPath;
    m_factory->CreatePathGeometry(&pPath);
    ComPtr<ID2D1GeometrySink> sink;
    pPath->Open(&sink);
    sink->BeginFigure(points[0], D2D1_FIGURE_BEGIN_FILLED);
    for (size_t i = 1; i < points.size(); ++i)
        sink->AddLine(points[i]);
    sink->EndFigure(D2D1_FIGURE_END_CLOSED);
    sink->Close();
    m_dc->FillGeometry(pPath.Get(), m_brush.Get());
}

void DrawContext::text(IDWriteTextFormat* fmt, const std::wstring& text, float x, float y) {
    if (!m_dc || !m_brush || !fmt) return;
    D2D1_RECT_F rect = D2D1::RectF(x, y, 10000.f, 10000.f);
    m_dc->DrawText(text.c_str(), static_cast<UINT32>(text.length()), fmt, rect, m_brush.Get());
}

void DrawContext::image(ID2D1Bitmap* bmp, float dx, float dy, float dw, float dh,
    float sx, float sy, float sw, float sh, float alpha)
{
    if (!m_dc || !bmp) return;
    if (sw < 0 || sh < 0) {
        auto size = bmp->GetSize();
        sw = size.width; sh = size.height;
    }
    D2D1_RECT_F dest = D2D1::RectF(dx, dy, dx + dw, dy + dh);
    D2D1_RECT_F src = D2D1::RectF(sx, sy, sx + sw, sy + sh);
    m_dc->DrawBitmap(bmp, dest, alpha, D2D1_BITMAP_INTERPOLATION_MODE_LINEAR, src);
}

// --- 상태 관리 ---
void DrawContext::push() {
    if (!m_dc) return;
    D2D1_MATRIX_3X2_F current;
    m_dc->GetTransform(&current);
    m_stateStack.push_back({ current, m_clipCount, m_strokeWidth });
}

void DrawContext::pop() {
    if (!m_dc || m_stateStack.empty()) return;
    StateLayer last = m_stateStack.back();
    m_stateStack.pop_back();

    while (m_clipCount > last.clipDepth) {
        m_dc->PopAxisAlignedClip();
        m_clipCount--;
    }

    m_strokeWidth = last.strokeWidth;
    m_dc->SetTransform(last.matrix);
}

void DrawContext::translate(float x, float y) {
    if (!m_dc) return;
    D2D1_MATRIX_3X2_F m;
    m_dc->GetTransform(&m);
    m_dc->SetTransform(m * D2D1::Matrix3x2F::Translation(x, y));
}

void DrawContext::scale(float sx, float sy, float ox, float oy) {
    if (!m_dc) return;
    D2D1_MATRIX_3X2_F m;
    m_dc->GetTransform(&m);
    m_dc->SetTransform(m * D2D1::Matrix3x2F::Scale(sx, sy, D2D1::Point2F(ox, oy)));
}

void DrawContext::clip(float x, float y, float w, float h) {
    if (!m_dc) return;
    m_dc->PushAxisAlignedClip(D2D1::RectF(x, y, x + w, y + h), D2D1_ANTIALIAS_MODE_ALIASED);
    m_clipCount++;
}

// --- 속성 ---
void DrawContext::setColor(float r, float g, float b, float a) {
    m_color = D2D1::ColorF(r, g, b, a);
    updateBrush();
}

void DrawContext::setStrokeWidth(float width) { m_strokeWidth = width; }
void DrawContext::setGlobalAlpha(float alpha) { m_globalAlpha = alpha; updateBrush(); }

void DrawContext::updateBrush() {
    if (!m_dc) return;
    if (!m_brush) m_dc->CreateSolidColorBrush(m_color, &m_brush);
    else m_brush->SetColor(m_color);
    if (m_brush) m_brush->SetOpacity(m_globalAlpha);
}

// --- Offscreen 생성 ---
std::shared_ptr<DrawContext> DrawContext::createOffscreen(ID2D1DeviceContext* parentDC, float w, float h) {
    // not implemented yet
    throw "not implemented yet!";
}

void DrawContext::BindGlobal(sol::state& lua, const char* name) {
    auto g = lua[name].get_or_create<sol::table>();

    g["rect"] = [this](float x, float y, float w, float h, sol::optional<bool> fill) {
        rect(x, y, w, h, fill.value_or(true));
        };

    g["circle"] = [this](float x, float y, float r, sol::optional<bool> fill) {
        circle(x, y, r, fill.value_or(true));
        };

    g["polyline"] = [this](sol::table t, sol::optional<bool> closed) {
        std::vector<D2D1_POINT_2F> points;
        for (size_t i = 1; i <= t.size(); i += 2)
            points.push_back(D2D1::Point2F(t[i], t[i + 1]));
        polyline(points, closed.value_or(false));
        };

    g["polygon"] = [this](sol::table t) {
        std::vector<D2D1_POINT_2F> points;
        for (size_t i = 1; i <= t.size(); i += 2)
            points.push_back(D2D1::Point2F(t[i], t[i + 1]));
        polygon(points);
        };

    g["push"] = [this]() { push(); };
    g["pop"] = [this]() { pop(); };
    g["translate"] = [this](float x, float y) { translate(x, y); };
    g["scale"] = [this](float sx, float sy, sol::optional<float> ox, sol::optional<float> oy) {
        scale(sx, sy, ox.value_or(0), oy.value_or(0));
        };
    g["clip"] = [this](float x, float y, float w, float h) { clip(x, y, w, h); };
    g["lineWidth"] = [this](float w) { setStrokeWidth(w); };
    g["color"] = [this](float r, float g, float b, sol::optional<float> a) {
        setColor(r, g, b, a.value_or(1.0f));
        };
    g["globalAlpha"] = [this](float a) { setGlobalAlpha(a); };
}