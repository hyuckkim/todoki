#pragma once
#include <d2d1_1.h>
#include <dwrite.h>
#include <d3d11.h>
#include <wrl/client.h>
#include <vector>
#include "includesol.h"

struct LuaBindContext;
class GraphicEngine;

using Microsoft::WRL::ComPtr;

// 상태 스택 레이어
struct StateLayer {
    D2D1_MATRIX_3X2_F matrix;
    int clipDepth;
    float strokeWidth;
};

class DrawContext {
public:
    DrawContext(ID2D1RenderTarget* renderTarget, ID2D1Factory1* factory, GraphicEngine* engine = nullptr);
    ~DrawContext();
    void BindGlobal(LuaBindContext& ctx);
    static void BindClass(LuaBindContext& ctx);

    void SetGraphicEngine(GraphicEngine* engine) { m_engine = engine; }

    // 기본 그리기 함수
    void rect(float x, float y, float w, float h, sol::optional<bool> fill);
    void circle(float x, float y, float r, sol::optional<bool> fill);
    void polyline(sol::table vertices, sol::optional<bool> closed);
    void polygon(sol::table vertices);
    void text(int fontId, std::string str, float x, float y);
    void image(int id, float dx, float dy, sol::optional<float> dw, sol::optional<float> dh,
        sol::optional<float> sx, sol::optional<float> sy,
        sol::optional<float> sw, sol::optional<float> sh, sol::optional<float> alpha);

    // Canvas 자체를 그리기
    void draw(DrawContext* source, float x, float y, sol::optional<float> w, sol::optional<float> h,
        sol::optional<float> sx, sol::optional<float> sy,
        sol::optional<float> sw, sol::optional<float> sh, sol::optional<float> alpha);

    // --- 상태 및 속성 ---
    void color(sol::object arg1, sol::optional<float> arg2, sol::optional<float> arg3, sol::optional<float> arg4);
    void setStrokeWidth(float width);
    void setGlobalAlpha(float alpha);
    void setShader(sol::optional<int> shaderId);

    void push();
    void pop();
    void translate(float x, float y);
    void scale(float sx, float sy, sol::optional<float> ox, sol::optional<float> oy);
    void clip(float x, float y, float w, float h);

    // --- 시스템 ---
    void batchBegin();
    void batchEnd();
    std::shared_ptr<DrawContext> createOffscreen(float w, float h);
     

private:
    ComPtr<ID2D1RenderTarget> m_rt;
	ComPtr<ID2D1Factory1> m_factory;
    ComPtr<ID2D1SolidColorBrush> m_brush;
    ComPtr<ID2D1Bitmap1> m_targetBitmap;
    GraphicEngine* m_engine;

    D2D1_COLOR_F m_color = D2D1::ColorF(1, 1, 1, 1);
    float m_strokeWidth = 1.0f;
    float m_globalAlpha = 1.0f;
    int m_clipCount = 0;
    int m_shaderId = -1;
    std::vector<StateLayer> m_stateStack;

    void EnsureDrawSession();
    void updateBrush();
    bool isBatching = false;

    void ApplyShaderToOffscreen(DrawContext* source, float dx, float dy, float dw, float dh,
        float sx, float sy, float sw, float sh, float alpha);
};