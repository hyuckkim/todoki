#include <dwrite.h>
#include "LuaCanvas.h"
#include "engine_graphic.h"
#include "util.h"

ID2D1SolidColorBrush* g_pSolidBrush = nullptr; // 전역 브러시 하나를 색상 변경 시마다 업데이트
D2D1_COLOR_F g_d2dColor = { 1.0f, 1.0f, 1.0f, 1.0f }; // 현재 색상 저장용
int g_clipCount = 0;
float g_strokeWidth = 1.0;
float g_globalAlpha = 1.0;

std::vector<StateLayer> g_stateStack;

void UpdateBrushAlpha() {
    if (g_pSolidBrush) {
        // 현재 브러시 색상의 알파값은 유지하되, globalAlpha를 곱해서 적용
        // 혹은 g_d2dColor 자체를 이미 계산된 값으로 관리해도 됩니다.
        g_pSolidBrush->SetOpacity(g_globalAlpha);
    }
}

void register_draw(sol::state& lua, const char* name) {
    g_pD2DDC->CreateSolidColorBrush(g_d2dColor, &g_pSolidBrush);
    g_stateStack.clear();
    g_clipCount = 0;
    g_strokeWidth = 1.0;
    g_globalAlpha = 1.0;

    lua.new_usertype<LuaCanvas>("Canvas",
        sol::constructors<LuaCanvas(float, float)>(),
        "batchBegin", &LuaCanvas::batchBegin,
        "batchEnd", &LuaCanvas::batchEnd,
        "release", &LuaCanvas::release,
        "color", &LuaCanvas::color,
        "rect", &LuaCanvas::rect,
        "circle", &LuaCanvas::circle,
        "polyline", &LuaCanvas::polyline,
        "polygon", &LuaCanvas::polygon,
        "text", &LuaCanvas::text,
        "image", &LuaCanvas::image,
        "draw", &LuaCanvas::draw
    );

    // 1. 테이블 생성 (기존 lua_newtable + lua_setglobal 대용)
    auto g = lua.create_named_table(name);

    g["offscreenCanvas"] = [](float w, float h) {
        return std::make_shared<LuaCanvas>(w, h);
        };

    g["rect"] = [](float x, float y, float w, float h) {
        DrawCore::Rect(g_pD2DDC.Get(), g_pSolidBrush, x, y, w, h);
        };

    g["circle"] = [](float x, float y, float r) {
        DrawCore::Circle(g_pD2DDC.Get(), g_pSolidBrush, x, y, r);
        };

    g["polyline"] = [](sol::table v, sol::optional<bool> c) {
        DrawCore::Polyline(g_pD2DDC.Get(), g_pSolidBrush, v, c.value_or(false), g_strokeWidth);
        };

    g["polygon"] = [](sol::table v) {
        DrawCore::Polygon(g_pD2DDC.Get(), g_pSolidBrush, v);
        };

    g["text"] = [](int fontId, std::string name, float x, float y) {
        if (fontId >= 0 && fontId < (int)g_fontTable.size())
            DrawCore::Text(g_pD2DDC.Get(), g_pSolidBrush, g_fontTable[fontId].Get(), name, x, y);
        };

    g["image"] = [](int id, float dx, float dy, sol::optional<float> dw, sol::optional<float> dh,
        sol::optional<float> sx, sol::optional<float> sy, sol::optional<float> sw, sol::optional<float> sh, sol::optional<bool> flipX) {
            if (id < 0 || id >= (int)g_bitmapTable.size()) return;

            ID2D1Bitmap* bmp = g_bitmapTable[id].Get();
            auto size = bmp->GetSize();
            float _dw = dw.value_or(size.width);
            float _dh = dh.value_or(size.height);

            // Flip 처리는 Matrix 변환이 필요하므로 Core 호출 전후로 처리
            D2D1_MATRIX_3X2_F old;
            g_pD2DDC->GetTransform(&old);
            if (flipX.value_or(false)) {
                g_pD2DDC->SetTransform(D2D1::Matrix3x2F::Scale(-1.0f, 1.0f, D2D1::Point2F(dx + _dw / 2.0f, dy + _dh / 2.0f)) * old);
            }

            DrawCore::Image(g_pD2DDC.Get(), bmp, dx, dy, _dw, _dh,
                sx.value_or(0), sy.value_or(0),
                sw.value_or(size.width), sh.value_or(size.height),
                g_globalAlpha);

            if (flipX.value_or(false)) g_pD2DDC->SetTransform(old);
        };
    g["lineWidth"] = [](float width) {
        g_strokeWidth = width;
        };
    g["globalAlpha"] = [](float alpha) {
        g_globalAlpha = alpha;
        UpdateBrushAlpha(); // 투명도 즉시 반영
        };

    g["color"] = [](float r, float g, float b, sol::optional<float> a) {
        // 컬러 설정 시에도 globalAlpha를 고려한 최종 색상 결정
        // (a는 0~255 범위라고 가정)
        float baseAlpha = a.value_or(255.0f) / 255.0f;
        g_d2dColor = D2D1::ColorF(r / 255.0f, g / 255.0f, b / 255.0f, baseAlpha);

        if (g_pD2DDC) {
            if (g_pSolidBrush == nullptr) {
                g_pD2DDC->CreateSolidColorBrush(g_d2dColor, &g_pSolidBrush);
            }
            else {
                g_pSolidBrush->SetColor(g_d2dColor);
            }
            // 색상을 바꾼 후에도 전역 알파가 적용되도록 세팅
            UpdateBrushAlpha();
        }
        };

    g["fontSize"] = [](int fontId, std::string text) -> std::pair<float, float> {
        if (fontId >= 0 && fontId < (int)g_fontTable.size()) {
            std::wstring wText = to_wstring(text);
            IDWriteTextFormat* pFormat = g_fontTable[fontId].Get(); // IDWriteTextFormat* 저장된 테이블

            IDWriteTextLayout* pLayout = nullptr;
            g_pDWriteFactory->CreateTextLayout(wText.c_str(), wText.length(), pFormat, 10000.0f, 10000.0f, &pLayout);

            DWRITE_TEXT_METRICS metrics;
            pLayout->GetMetrics(&metrics);

            float w = metrics.width;
            float h = metrics.height;
            pLayout->Release();

            return { w, h };
        }
        return { 0.0f, 0.0f };
    };

    g["clip"] = [](float x, float y, float w, float h) {
        g_pD2DDC->PushAxisAlignedClip(D2D1::RectF(x, y, x + w, y + h), D2D1_ANTIALIAS_MODE_ALIASED);
        g_clipCount++;
        };

    g["push"] = []() {
        D2D1_MATRIX_3X2_F current;
        g_pD2DDC->GetTransform(&current);
        g_stateStack.push_back({ current, g_clipCount, g_strokeWidth });
        };

    g["pop"] = []() {
        if (g_stateStack.empty()) return;

        StateLayer last = g_stateStack.back();
        g_stateStack.pop_back();

        // 1. push했던 시점보다 더 많이 쌓인 클립들을 모두 해제
        while (g_clipCount > last.clipDepth) {
            g_pD2DDC->PopAxisAlignedClip();
            g_clipCount--;
        }
        g_strokeWidth = last.strokeWidth;

        // 2. 변환 행렬 복구
        g_pD2DDC->SetTransform(last.matrix);
        };

    // 3. 이동 (Translate)
    g["translate"] = [](float x, float y) {
        D2D1_MATRIX_3X2_F current, next;
        g_pD2DDC->GetTransform(&current);
        next = current * D2D1::Matrix3x2F::Translation(x, y);
        g_pD2DDC->SetTransform(next);
        };

    // 4. 확대/축소 (Scale)
    g["scale"] = [](float sx, float sy, sol::optional<float> ox, sol::optional<float> oy) {
        D2D1_MATRIX_3X2_F current, next;
        g_pD2DDC->GetTransform(&current);

        // 중심점(ox, oy)이 주어지면 그 지점을 기준으로 확대, 아니면 (0,0) 기준
        D2D1_POINT_2F center = D2D1::Point2F(ox.value_or(0.0f), oy.value_or(0.0f));
        next = current * D2D1::Matrix3x2F::Scale(sx, sy, center);
        g_pD2DDC->SetTransform(next);
        };
}