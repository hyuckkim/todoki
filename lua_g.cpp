#include "lua_engine.h"

int l_drawRect(lua_State* L) {
    float x = (float)luaL_checknumber(L, 1);
    float y = (float)luaL_checknumber(L, 2);
    float w = (float)luaL_checknumber(L, 3);
    float h = (float)luaL_checknumber(L, 4);

    if (g_currentGraphics) {
        SolidBrush brush(g_currentColor);
        g_currentGraphics->FillRectangle(&brush, x, y, w, h);
    }
    return 0;
}

int l_setFillStyle(lua_State* L) {
    int r = (int)luaL_checkinteger(L, 1);
    int g = (int)luaL_checkinteger(L, 2);
    int b = (int)luaL_checkinteger(L, 3);
    int a = (int)luaL_optinteger(L, 4, 255); // 알파는 선택

    g_currentColor = Color(a, r, g, b); // GDI+ 색상 갱신
    return 0;
}

int l_drawText(lua_State* L) {
    int fontId = (int)luaL_checkinteger(L, 1);
    const char* text = luaL_checkstring(L, 2);
    float x = (float)luaL_checknumber(L, 3);
    float y = (float)luaL_checknumber(L, 4);

    if (fontId >= 0 && fontId < (int)g_fontTable.size() && g_currentGraphics) {
        STR_TO_WCHAR(text, wText);
        Font* font = g_fontTable[fontId];
        SolidBrush brush(g_currentColor);

        g_currentGraphics->DrawString(wText, -1, font, PointF(x, y), &brush);
    }
    return 0;
}

int l_drawImage(lua_State* L) {
    int id = (int)luaL_checkinteger(L, 1);

    // 1. 목적지 좌표 및 크기 (Destination)
    float dx = (float)luaL_checknumber(L, 2);
    float dy = (float)luaL_checknumber(L, 3);
    float dw = (float)luaL_optnumber(L, 4, -1); // -1이면 원본 크기 사용 로직용
    float dh = (float)luaL_optnumber(L, 5, -1);

    // 2. 소스 좌표 및 크기 (Source)
    float sx = (float)luaL_optnumber(L, 6, 0);
    float sy = (float)luaL_optnumber(L, 7, 0);
    float sw = (float)luaL_optnumber(L, 8, -1);
    float sh = (float)luaL_optnumber(L, 9, -1);

    if (id >= 0 && id < (int)g_imageTable.size() && g_currentGraphics) {
        Image* img = g_imageTable[id];

        // 기본값 처리: 인자가 생략되었을 경우 이미지 전체 크기로 설정
        if (dw < 0) dw = (float)img->GetWidth();
        if (dh < 0) dh = (float)img->GetHeight();
        if (sw < 0) sw = (float)img->GetWidth();
        if (sh < 0) sh = (float)img->GetHeight();

        // GDI+ DrawImage(이미지, 목적지 사각형, 소스 x, 소스 y, 소스 w, 소스 h, 단위)
        RectF destRect(dx, dy, dw, dh);
        g_currentGraphics->DrawImage(img, destRect, sx, sy, sw, sh, UnitPixel);
    }
    return 0;
}

void register_draw(lua_State* L, const char* name) {
    lua_newtable(L);
    REG_METHOD(L, "rect", l_drawRect);
    REG_METHOD(L, "color", l_setFillStyle);
    REG_METHOD(L, "text", l_drawText);
    REG_METHOD(L, "image", l_drawImage);
    lua_setglobal(L, name);
}