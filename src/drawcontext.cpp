#include "DrawContext.h"
#include <d2d1_1.h>
#include <wrl/client.h>
#include "resourcehub.h"
#include "util.h"

using Microsoft::WRL::ComPtr;

DrawContext::DrawContext(ID2D1RenderTarget* renderTarget, ID2D1Factory1* factory)
    : m_rt(renderTarget), m_factory(factory)
{
    if (m_rt) {
        m_rt->CreateSolidColorBrush(m_color, &m_brush);
    }
}

DrawContext::~DrawContext() {
    m_brush.Reset();
    m_rt.Reset();
}

// --- 그리기 함수 ---
void DrawContext::rect(float x, float y, float w, float h, sol::optional<bool> fill) {
    if (!m_rt || !m_brush) return;
    auto r = D2D1::RectF(x, y, x + w, y + h);
    if (fill.value_or(true)) m_rt->FillRectangle(r, m_brush.Get());
    else m_rt->DrawRectangle(r, m_brush.Get(), m_strokeWidth);
}

void DrawContext::circle(float x, float y, float r, sol::optional<bool> fill) {
    if (!m_rt || !m_brush) return;
    auto e = D2D1::Ellipse(D2D1::Point2F(x, y), r, r);
    if (fill.value_or(true)) m_rt->FillEllipse(e, m_brush.Get());
    else m_rt->DrawEllipse(e, m_brush.Get(), m_strokeWidth);
}

void DrawContext::polyline(sol::table vertices, sol::optional<bool> closed) {
    if (!m_rt || !m_factory || vertices.size() < 4) return;
    ComPtr<ID2D1PathGeometry> path;
    m_factory->CreatePathGeometry(&path);
    ComPtr<ID2D1GeometrySink> sink;
    path->Open(&sink);

    sink->BeginFigure(D2D1::Point2F(vertices[1], vertices[2]), D2D1_FIGURE_BEGIN_FILLED);
    for (size_t i = 3; i <= vertices.size(); i += 2) {
        sink->AddLine(D2D1::Point2F(vertices[i], vertices[i + 1]));
    }
    sink->EndFigure(closed.value_or(false) ? D2D1_FIGURE_END_CLOSED : D2D1_FIGURE_END_OPEN);
    sink->Close();
    m_rt->DrawGeometry(path.Get(), m_brush.Get(), m_strokeWidth);
}

void DrawContext::polygon(sol::table vertices) {
    if (!m_rt || !m_factory || vertices.size() < 6) return;
    ComPtr<ID2D1PathGeometry> path;
    m_factory->CreatePathGeometry(&path);
    ComPtr<ID2D1GeometrySink> sink;
    path->Open(&sink);
    sink->BeginFigure(D2D1::Point2F(vertices[1], vertices[2]), D2D1_FIGURE_BEGIN_FILLED);
    for (size_t i = 3; i <= vertices.size(); i += 2) {
        sink->AddLine(D2D1::Point2F(vertices[i], vertices[i + 1]));
    }
    sink->EndFigure(D2D1_FIGURE_END_CLOSED);
    sink->Close();
    m_rt->FillGeometry(path.Get(), m_brush.Get());
}
// --- 폰트/텍스트 그리기 ---
void DrawContext::text(int fontId, std::string str, float x, float y) {
    if (!m_rt || !m_brush) return;

    IDWriteTextFormat* pFormat = ResourceHub::Instance().GetFont(fontId);
    if (!pFormat) return;
    std::wstring wstr = ToWString(str);
    D2D1_RECT_F layoutRect = D2D1::RectF(
        x, y,
        x + 10000.0f, y + 10000.0f
    );

    m_rt->DrawText(
        wstr.c_str(),
        static_cast<UINT32>(wstr.length()),
        pFormat,
        layoutRect,
        m_brush.Get()
    );
}

// --- 이미지/비트맵 그리기 ---
void DrawContext::image(int id, float dx, float dy,
    sol::optional<float> dw, sol::optional<float> dh,
    sol::optional<float> sx, sol::optional<float> sy,
    sol::optional<float> sw, sol::optional<float> sh,
    sol::optional<float> alpha)
{
	ID2D1Bitmap* bmp = ResourceHub::Instance().GetBitmap(id);
    if (!bmp || !m_rt) return;
    auto size = bmp->GetSize();
    float sX = sx.value_or(0);
    float sY = sy.value_or(0);
    float sW = sw.value_or(size.width - sX);
    float sH = sh.value_or(size.height - sY);
    m_rt->DrawBitmap(bmp,
        D2D1::RectF(dx, dy, dx + dw.value_or(sW), dy + dh.value_or(sH)),
        alpha.value_or(m_globalAlpha),
        D2D1_BITMAP_INTERPOLATION_MODE_NEAREST_NEIGHBOR,
		D2D1::RectF(sX, sY, sX + sW, sY + sH));
}
// --- 상태 관리 ---
void DrawContext::push() {
    if (!m_rt) return;
    D2D1_MATRIX_3X2_F current;
    m_rt->GetTransform(&current);
    m_stateStack.push_back({ current, m_clipCount, m_strokeWidth });
}

void DrawContext::pop() {
    if (!m_rt || m_stateStack.empty()) return;
    StateLayer last = m_stateStack.back();
    m_stateStack.pop_back();

    while (m_clipCount > last.clipDepth) {
        m_rt->PopAxisAlignedClip();
        m_clipCount--;
    }

    m_strokeWidth = last.strokeWidth;
    m_rt->SetTransform(last.matrix);
}

void DrawContext::translate(float x, float y) {
    if (!m_rt) return;
    D2D1_MATRIX_3X2_F m;
    m_rt->GetTransform(&m);
    m_rt->SetTransform(m * D2D1::Matrix3x2F::Translation(x, y));
}

void DrawContext::scale(float sx, float sy, sol::optional<float> ox, sol::optional<float> oy) {
    if (!m_rt) return;
    D2D1_MATRIX_3X2_F m;
    m_rt->GetTransform(&m);
    m_rt->SetTransform(m * D2D1::Matrix3x2F::Scale(sx, sy, D2D1::Point2F(ox.value_or(sx), oy.value_or(sy))));
}

void DrawContext::clip(float x, float y, float w, float h) {
    if (!m_rt) return;
    m_rt->PushAxisAlignedClip(D2D1::RectF(x, y, x + w, y + h), D2D1_ANTIALIAS_MODE_ALIASED);
    m_clipCount++;
}

// --- 속성 ---
void DrawContext::updateBrush() {
    if (!m_rt) return;
    if (!m_brush) m_rt->CreateSolidColorBrush(m_color, &m_brush);
    else m_brush->SetColor(m_color);
    m_brush->SetOpacity(m_globalAlpha);
}
void DrawContext::color(sol::object arg1, sol::optional<float> arg2, sol::optional<float> arg3, sol::optional<float> arg4) {
    auto smartColor = [](sol::object arg1, sol::optional<float> arg2, sol::optional<float> arg3, sol::optional<float> arg4) {
        // 1. 문자열 처리 (예: "#FF0000FF")
        if (arg1.is<std::string>()) {
            std::string s = arg1.as<std::string>();

            // '#' 제거
            if (!s.empty() && s[0] == '#') s = s.substr(1);

            // 6자리 또는 8자리인지 확인
            if (s.length() == 6 || s.length() == 8) {
                uint32_t hex = std::stoul(s, nullptr, 16);
                float r, g, b, a;

                if (s.length() == 8) {
                    // RRGGBBAA
                    r = ((hex >> 24) & 0xFF) / 255.f;
                    g = ((hex >> 16) & 0xFF) / 255.f;
                    b = ((hex >> 8) & 0xFF) / 255.f;
                    a = (hex & 0xFF) / 255.f;
                }
                else {
                    // RRGGBB + optional alpha
                    r = ((hex >> 16) & 0xFF) / 255.f;
                    g = ((hex >> 8) & 0xFF) / 255.f;
                    b = (hex & 0xFF) / 255.f;
                    a = arg2.value_or(1.f);
                    if (a > 1.f) a /= 255.f;
                }
                return D2D1::ColorF(r, g, b, a);
            }

            // 문자열 형식이 이상하면 Black 반환
            return D2D1::ColorF(D2D1::ColorF::Black);
        }

        // 2. 숫자 처리
        if (arg1.is<float>()) {
            float r = arg1.as<float>();

            // 인자가 3개 이상 들어온 경우 (r, g, b, [a])
            if (arg2.has_value() && arg3.has_value()) {
                float g = arg2.value();
                float b = arg3.value();

                bool isIntRange = (r > 1.0f || g > 1.0f || b > 1.0f);
                float rf = isIntRange ? r / 255.0f : r;
                float gf = isIntRange ? g / 255.0f : g;
                float bf = isIntRange ? b / 255.0f : b;

                float av = arg4.value_or(isIntRange ? 255.0f : 1.0f);
                float af = (av > 1.0f) ? av / 255.0f : av;

                return D2D1::ColorF(rf, gf, bf, af);
            }

            // 인자가 1개 또는 2개인 경우 (Hex 처리)
            // 0xRRGGBBAA 또는 0xRRGGBB 형태 판별
            uint32_t hex = static_cast<uint32_t>(r);
            float finalA = 1.0f;

            if (hex > 0xFFFFFF) {
                // 8자리 Hex (RRGGBBAA)로 간주
                float rv = ((hex >> 24) & 0xFF) / 255.0f;
                float gv = ((hex >> 16) & 0xFF) / 255.0f;
                float bv = ((hex >> 8) & 0xFF) / 255.0f;
                float av = (hex & 0xFF) / 255.0f;
                return D2D1::ColorF(rv, gv, bv, av);
            }
            else {
                // 6자리 Hex (RRGGBB)로 간주 + 별도 Alpha 인자 확인
                float av = arg2.value_or(1.0f);
                finalA = (av > 1.0f) ? av / 255.0f : av;
                return D2D1::ColorF(hex, finalA);
            }
        }
        return D2D1::ColorF(D2D1::ColorF::Black);
        };
    m_color = smartColor(arg1, arg2, arg3, arg4);
    updateBrush();
}
void DrawContext::setStrokeWidth(float width) { m_strokeWidth = width; }
void DrawContext::setGlobalAlpha(float alpha) { m_globalAlpha = alpha; updateBrush(); }

// --- Offscreen 생성 ---
std::shared_ptr<DrawContext> DrawContext::createOffscreen(float w, float h) {
    // 1. 현재 DC와 호환되는 새로운 비트맵 렌더 타겟 생성
    ComPtr<ID2D1BitmapRenderTarget> pRT;
    HRESULT hr = m_rt->CreateCompatibleRenderTarget(D2D1::SizeF(w, h), &pRT);

    if (FAILED(hr)) return nullptr;
    auto offscreenCtx = std::make_shared<DrawContext>(pRT.Get(), m_factory.Get());

    return offscreenCtx;
}

void DrawContext::batchBegin() { if (!isBatching) { m_rt->BeginDraw(); isBatching = true; } }
void DrawContext::batchEnd() { if (isBatching) { m_rt->EndDraw(); isBatching = false; } }

void DrawContext::draw(DrawContext* source, float x, float y,
    sol::optional<float> w, sol::optional<float> h,
    sol::optional<float> sx, sol::optional<float> sy,
    sol::optional<float> sw, sol::optional<float> sh, sol::optional<float> alpha)
{
    if (!source || !source->m_rt) return;

    if (source->isBatching) source->batchEnd();

    ComPtr<ID2D1BitmapRenderTarget> bitmapRT;
    if (SUCCEEDED(source->m_rt.As(&bitmapRT))) {
        ComPtr<ID2D1Bitmap> bmp;
        if (SUCCEEDED(bitmapRT->GetBitmap(&bmp))) {
            auto size = bmp->GetSize();
            float sX = sx.value_or(0);
            float sY = sy.value_or(0);
            float sW = sw.value_or(size.width - sX);
            float sH = sh.value_or(size.height - sY);

            m_rt->DrawBitmap(bmp.Get(),
                D2D1::RectF(x, y, x + w.value_or(sW), y + h.value_or(sH)),
                alpha.value_or(source->m_globalAlpha),
                D2D1_BITMAP_INTERPOLATION_MODE_NEAREST_NEIGHBOR,
                D2D1::RectF(sX, sY, sX + sW, sY + sH));
        }
    }
}

void DrawContext::BindGlobal(sol::state& lua, const char* name) {
    // 1. 오프스크린 객체(Canvas)를 위한 타입 정의
    // (객체는 생성 후 canvas:rect() 처럼 콜론(:)으로 호출합니다)
    lua.new_usertype<DrawContext>("Canvas",
        sol::no_constructor,
        "batchBegin", &DrawContext::batchBegin,
        "batchEnd", &DrawContext::batchEnd,
        "rect", &DrawContext::rect,
        "circle", &DrawContext::circle,
        "polyline", &DrawContext::polyline,
        "polygon", &DrawContext::polygon,
        "text", &DrawContext::text,
        "image", &DrawContext::image,
        "color", &DrawContext::color,
        "lineWidth", &DrawContext::setStrokeWidth,
        "push", &DrawContext::push,
        "pop", &DrawContext::pop,
        "translate", &DrawContext::translate,
        "scale", &DrawContext::scale
    );

    // 2. 전역 테이블 'g' 생성 (this가 고정된 헬퍼 테이블)
    auto g = lua.create_named_table(name);

    // --- 기본 도형 ---
    g["rect"] = [this](float x, float y, float w, float h, sol::optional<bool> f) { rect(x, y, w, h, f); };
    g["circle"] = [this](float x, float y, float r, sol::optional<bool> f) { circle(x, y, r, f); };

    // --- 폴리곤 / 폴리라인 (테이블 인자) ---
    g["polyline"] = [this](sol::table t, sol::optional<bool> c) { polyline(t, c); };
    g["polygon"] = [this](sol::table t) { polygon(t); };

    // --- 텍스트 / 이미지 ---
    g["text"] = [this](int fontId, std::string s, float x, float y) { text(fontId, s, x, y); };
    g["image"] = [this](int id, float dx, float dy,
        sol::optional<float> dw, sol::optional<float> dh,
        sol::optional<float> sx, sol::optional<float> sy,
        sol::optional<float> sw, sol::optional<float> sh,
        sol::optional<float> a) {
            image(id, dx, dy, dw, dh, sx, sy, sw, sh, a);
        };

    // --- 오프스크린 & 드로우 ---
    g["offscreen"] = [this](float w, float h) { return createOffscreen(w, h); };
    g["draw"] = [this](DrawContext* src, float x, float y,
        sol::optional<float> w, sol::optional<float> h,
        sol::optional<float> sx, sol::optional<float> sy,
        sol::optional<float> sw, sol::optional<float> sh,
        sol::optional<float> a) {
            draw(src, x, y, w, h, sx, sy, sw, sh, a);
        };

    // --- 상태 관리 및 속성 ---

    g["color"] = [this](sol::object arg1, sol::optional<float> arg2, sol::optional<float> arg3, sol::optional<float> arg4) {
		color(arg1, arg2, arg3, arg4);
        };
    g["lineWidth"] = [this](float w) { setStrokeWidth(w); };
    g["push"] = [this]() { push(); };
    g["pop"] = [this]() { pop(); };
    g["translate"] = [this](float x, float y) { translate(x, y); };
    g["scale"] = [this](float sx, float sy, sol::optional<float> ox, sol::optional<float> oy) { scale(sx, sy, ox, oy); };
    g["clip"] = [this](float x, float y, float w, float h) { clip(x, y, w, h); };
}