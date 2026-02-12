#pragma once
#include <d2d1.h>
#include <wrl.h>
#include <wrl/client.h>
#include "sol.h"

using Microsoft::WRL::ComPtr;

class LuaCanvas {
private:
    bool isBatching = false;
    // 이 캔버스 전용 타겟과 브러시
    ComPtr<ID2D1BitmapRenderTarget> pRT;
    ComPtr<ID2D1SolidColorBrush> pCanvasBrush;
    float pGlobalAlpha;

public:
    LuaCanvas(float w, float h);
    ~LuaCanvas();

    // 배치 제어
    void batchBegin();
    void batchEnd();
    void release(); // 명시적 해제

    // 내부 실행 헬퍼
    void execute(std::function<void()> drawFunc);

    // 그리기 메서드 (DrawCore 활용)
    void color(float r, float g, float b, sol::optional<float> a);
    void rect(float x, float y, float w, float h, bool fill);
    void circle(float x, float y, float r, bool fill);
    void polyline(sol::table vertices, bool closed, float strokeWidth);
    void polygon(sol::table vertices);
    void text(int fontId, std::string name, float x, float y);
    void image(int id, float dx, float dy, sol::optional<float> dw, sol::optional<float> dh,
        sol::optional<float> sx, sol::optional<float> sy, sol::optional<float> sw, sol::optional<float> sh);

    // 메인 화면에 출력
    void draw(float x, float y,
        sol::optional<float> w, sol::optional<float> h,
        sol::optional<float> sx, sol::optional<float> sy,
        sol::optional<float> sw, sol::optional<float> sh);
};

struct StateLayer {
    D2D1_MATRIX_3X2_F matrix;
    int clipDepth; // 해당 push 시점의 클립 깊이
    float strokeWidth;
};

namespace DrawCore {
    // 도형
    void Rect(ID2D1RenderTarget* rt, ID2D1SolidColorBrush* brush, float x, float y, float w, float h, bool fill = true);
    void Circle(ID2D1RenderTarget* rt, ID2D1SolidColorBrush* brush, float x, float y, float radius, bool fill = true);
    void Polyline(ID2D1RenderTarget* rt, ID2D1SolidColorBrush* brush, sol::table vertices, bool closed, float strokeWidth);
    void Polygon(ID2D1RenderTarget* rt, ID2D1SolidColorBrush* brush, sol::table vertices);

    // 텍스트 (FontTable 관리 필요)
    void Text(ID2D1RenderTarget* rt, ID2D1SolidColorBrush* brush, IDWriteTextFormat* pFormat, const std::string& text, float x, float y);

    // 이미지
    void Image(ID2D1RenderTarget* rt, ID2D1Bitmap* bmp, float dx, float dy, float dw, float dh, float sx, float sy, float sw, float sh, float alpha);
}