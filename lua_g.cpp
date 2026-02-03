#include "lua_engine.h"

ID2D1SolidColorBrush* g_pSolidBrush = nullptr; // 전역 브러시 하나를 색상 변경 시마다 업데이트
D2D1_COLOR_F g_d2dColor = { 1.0f, 1.0f, 1.0f, 1.0f }; // 현재 색상 저장용

void register_draw(sol::state& lua, const char* name) {
    g_pDCRT->CreateSolidColorBrush(g_d2dColor, &g_pSolidBrush);

    // 1. 테이블 생성 (기존 lua_newtable + lua_setglobal 대용)
    auto g = lua.create_named_table(name);

    // 2. Rect 그리기
    g["rect"] = [](float x, float y, float w, float h) {
        if (g_pDCRT && g_pSolidBrush) {
            g_pSolidBrush->SetColor(g_d2dColor); // 그리기 직전 색상 동기화
            g_pDCRT->FillRectangle(D2D1::RectF(x, y, x + w, y + h), g_pSolidBrush);
        }
    };

    // 3. 색상 설정 (알파값 선택적 처리)
    // sol::optional을 쓰면 루아에서 인자를 안 보냈을 때 기본값을 줄 수 있습니다.
    g["color"] = [](int r, int g, int b, sol::optional<int> a) {
        g_d2dColor = D2D1::ColorF(r / 255.0f, g / 255.0f, b / 255.0f, a.value_or(255) / 255.0f);

        if (g_pDCRT) {
            if (g_pSolidBrush == nullptr) {
                // 브러시가 처음일 때만 생성
                g_pDCRT->CreateSolidColorBrush(g_d2dColor, &g_pSolidBrush);
            }
            else {
                // 이미 있으면 색상만 변경 (이게 훨씬 빠릅니다)
                g_pSolidBrush->SetColor(g_d2dColor);
            }
        }
        };

    // 4. 텍스트 그리기
    g["text"] = [](int fontId, std::string text, float x, float y) {
        if (fontId >= 0 && fontId < (int)g_fontTable.size() && g_pDCRT) {
            std::wstring wText = to_wstring(text);
            IDWriteTextFormat* pFormat = g_fontTable[fontId];

            // D2D는 텍스트를 그릴 영역(Rect)을 지정해야 합니다.
            // x, y부터 시작해서 아주 넓은 영역을 잡아주면 GDI+처럼 동작합니다.
            D2D1_RECT_F layoutRect = D2D1::RectF(x, y, 10000.0f, 10000.0f);

            // 현재 설정된 전역 브러시(g_pSolidBrush)로 그리기
            g_pDCRT->DrawText(
                wText.c_str(),
                (UINT32)wText.length(),
                pFormat,
                layoutRect,
                g_pSolidBrush
            );
        }
    };
    g["fontSize"] = [](int fontId, std::string text) -> std::pair<float, float> {
        if (fontId >= 0 && fontId < (int)g_fontTable.size()) {
            std::wstring wText = to_wstring(text);
            IDWriteTextFormat* pFormat = g_fontTable[fontId]; // IDWriteTextFormat* 저장된 테이블

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

    // 5. 이미지 그리기 (기본값 파라미터가 많으므로 매우 편해집니다)
    g["image"] = [](int id, float dx, float dy,
        sol::optional<float> dw, sol::optional<float> dh,
        sol::optional<float> sx, sol::optional<float> sy,
        sol::optional<float> sw, sol::optional<float> sh) {
            if (id >= 0 && id < (int)g_bitmapTable.size() && g_pDCRT) {
                ID2D1Bitmap* bmp = g_bitmapTable[id];
                auto size = bmp->GetSize();

                float _dw = dw.value_or(size.width);
                float _dh = dh.value_or(size.height);
                float _sx = sx.value_or(0.0f);
                float _sy = sy.value_or(0.0f);
                float _sw = sw.value_or(size.width);
                float _sh = sh.value_or(size.height);

                g_pDCRT->DrawBitmap(
                    bmp,
                    D2D1::RectF(dx, dy, dx + _dw, dy + _dh),
                    1.0f,
                    D2D1_BITMAP_INTERPOLATION_MODE_LINEAR,
                    D2D1::RectF(_sx, _sy, _sx + _sw, _sy + _sh)
                );
            }
        };
}