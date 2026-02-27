#pragma once
#include <d2d1_1.h>
#include <dwrite.h>
#include <wrl/client.h>
#include <vector>
#include <sol/sol.hpp>

using Microsoft::WRL::ComPtr;

// 상태 스택 레이어
struct StateLayer {
    D2D1_MATRIX_3X2_F matrix;
    int clipDepth;
    float strokeWidth;
};

class DrawContext {
public:
    DrawContext(ID2D1DeviceContext* context, ID2D1Factory1* factory);
    ~DrawContext();
    void BindGlobal(sol::state& lua, const char* name);

    // 기본 그리기 함수
    void rect(float x, float y, float w, float h, bool fill = true);
    void circle(float x, float y, float radius, bool fill = true);
    void polyline(const std::vector<D2D1_POINT_2F>& points, bool closed = false);
    void polygon(const std::vector<D2D1_POINT_2F>& points);
    void text(IDWriteTextFormat* fmt, const std::wstring& text, float x, float y);
    void image(ID2D1Bitmap* bmp, float dx, float dy, float dw, float dh,
        float sx = 0, float sy = 0, float sw = -1, float sh = -1, float alpha = 1.0f);

    // 상태 관리
    void push();
    void pop();
    void translate(float x, float y);
    void scale(float sx, float sy, float ox = 0, float oy = 0);
    void clip(float x, float y, float w, float h);

    // 속성
    void setColor(float r, float g, float b, float a = 1.0f);
    void setStrokeWidth(float width);
    void setGlobalAlpha(float alpha);

    // Offscreen canvas 생성
    static std::shared_ptr<DrawContext> createOffscreen(ID2D1DeviceContext* parentDC, float w, float h);

private:
    ComPtr<ID2D1DeviceContext> m_dc;
	ComPtr<ID2D1Factory1> m_factory;
    ComPtr<ID2D1SolidColorBrush> m_brush;

    D2D1_COLOR_F m_color = D2D1::ColorF(1, 1, 1, 1);
    float m_strokeWidth = 1.0f;
    float m_globalAlpha = 1.0f;
    int m_clipCount = 0;
    std::vector<StateLayer> m_stateStack;

    void updateBrush();
};