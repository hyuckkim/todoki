#pragma once
#include "lua_engine.h"

using Microsoft::WRL::ComPtr;

class LuaCanvas {
private:
    bool isBatching = false;
    // 이 캔버스 전용 타겟과 브러시
    ComPtr<ID2D1BitmapRenderTarget> pRT;
    ComPtr<ID2D1SolidColorBrush> pCanvasBrush;

public:
    LuaCanvas(float w, float h);

    // 배치 제어
    void batchBegin();
    void batchEnd();

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
    void draw(float x, float y);
};