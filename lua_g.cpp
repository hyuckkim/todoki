#include "lua_engine.h"
// 이제 luaL_check... 시리즈는 필요 없습니다. sol2가 인자 개수와 타입을 자동으로 검증합니다.

void register_draw(sol::state& lua, const char* name) {
    // 1. 테이블 생성 (기존 lua_newtable + lua_setglobal 대용)
    auto g = lua.create_named_table(name);

    // 2. Rect 그리기
    g["rect"] = [](float x, float y, float w, float h) {
        if (g_currentGraphics) {
            SolidBrush brush(g_currentColor);
            g_currentGraphics->FillRectangle(&brush, x, y, w, h);
        }
    };

    // 3. 색상 설정 (알파값 선택적 처리)
    // sol::optional을 쓰면 루아에서 인자를 안 보냈을 때 기본값을 줄 수 있습니다.
    g["color"] = [](int r, int g, int b, sol::optional<int> a) {
        g_currentColor = Color(a.value_or(255), r, g, b);
    };

    // 4. 텍스트 그리기
    g["text"] = [](int fontId, std::string text, float x, float y) {
        if (fontId >= 0 && fontId < (int)g_fontTable.size() && g_currentGraphics) {
            // 기존 STR_TO_WCHAR 매크로 대신 더 안전한 변환 사용 가능
            std::wstring wText = to_wstring(text);
            Font* font = g_fontTable[fontId];
            SolidBrush brush(g_currentColor);
            g_currentGraphics->DrawString(wText.c_str(), -1, font, PointF(x, y), &brush);
        }
    };
    g["fontSize"] = [](int fontId, std::string text) -> std::pair<float, float> {
        if (fontId >= 0 && fontId < (int)g_fontTable.size()) {
            std::wstring wText = to_wstring(text);
            Font* font = g_fontTable[fontId];

            // 실제 화면 Graphics 대신, 측정용 임시 비트맵 Graphics를 사용 (일관성 유지)
            static Bitmap dummy(1, 1);
            static Graphics* measurer = Graphics::FromImage(&dummy);

            // 여백(Overhang) 없는 정확한 측정을 위한 포맷
            static StringFormat format(StringFormat::GenericTypographic());

            RectF boundRect;
            measurer->MeasureString(wText.c_str(), -1, font, PointF(0, 0), &format, &boundRect);

            return { boundRect.Width, boundRect.Height };
        }
        return { 0.0f, 0.0f };
        };

    // 5. 이미지 그리기 (기본값 파라미터가 많으므로 매우 편해집니다)
    g["image"] = [](int id, float dx, float dy,
        sol::optional<float> dw, sol::optional<float> dh,
        sol::optional<float> sx, sol::optional<float> sy,
        sol::optional<float> sw, sol::optional<float> sh) {

        if (id >= 0 && id < (int)g_imageTable.size() && g_currentGraphics) {
            Image* img = g_imageTable[id];

            // 루아에서 넘어오지 않은 값들은 이미지 원본 크기/0으로 채움
            float _dw = dw.value_or((float)img->GetWidth());
            float _dh = dh.value_or((float)img->GetHeight());
            float _sx = sx.value_or(0.0f);
            float _sy = sy.value_or(0.0f);
            float _sw = sw.value_or((float)img->GetWidth());
            float _sh = sh.value_or((float)img->GetHeight());

            RectF destRect(dx, dy, _dw, _dh);
            g_currentGraphics->DrawImage(img, destRect, _sx, _sy, _sw, _sh, UnitPixel);
        }
    };

    // 6. 클리핑 관련
    g["setClip"] = [](int x, int y, int w, int h) {
        if (g_currentGraphics) g_currentGraphics->SetClip(Rect(x, y, w, h));
    };

    g["resetClip"] = []() {
        if (g_currentGraphics) g_currentGraphics->ResetClip();
    };
}