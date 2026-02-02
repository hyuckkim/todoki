#include "lua_engine.h"

Color g_currentColor(255, 255, 255, 255);
std::vector<Image*> g_imageTable;
std::vector<Font*> g_fontTable;
std::map<std::string, int> g_pathCache;

void unregisterLuaFunctions(lua_State* L) {
    // 이미지 메모리 해제
    for (Image* img : g_imageTable) {
        delete img;
    }
    for (Font* font : g_fontTable) {
        delete font;
	}
    g_fontTable.clear();
    g_imageTable.clear();
    g_pathCache.clear();
}

int l_loadFont(lua_State* L) {
    const char* name = luaL_checkstring(L, 1);
    float size = (float)luaL_checknumber(L, 2);
    int style = (int)luaL_optinteger(L, 3, FontStyleRegular); // 기본값 0

    STR_TO_WCHAR(name, wName);

    // 폰트 객체 생성 후 테이블에 보관
    Font* font = new Font(wName, size, style);

    int id = (int)g_fontTable.size();
    g_fontTable.push_back(font);

    lua_pushinteger(L, id);
    return 1;
}
int l_loadFontFile(lua_State* L) {
    const char* path = luaL_checkstring(L, 1);
    float size = (float)luaL_checknumber(L, 2);

    // 1. 상대 경로를 절대 경로로 변환
    char fullPath[MAX_PATH];
    GetFullPathNameA(path, MAX_PATH, fullPath, NULL);

    STR_TO_WCHAR(fullPath, wPath);
    Status status = g_pfc->AddFontFile(wPath);
    if (status != Ok) return 0;

    // 장부의 가장 마지막에 추가된 패밀리 가져오기
    int count = g_pfc->GetFamilyCount();
    FontFamily* families = new FontFamily[count];
    int found = 0;
    g_pfc->GetFamilies(count, families, &found);

    // 방금 추가한 폰트 패밀리로 폰트 생성 (보통 마지막에 추가됨)
    Font* font = new Font(&families[count - 1], size, FontStyleRegular, UnitPixel);

    delete[] families; // 임시 배열은 삭제

    int id = (int)g_fontTable.size();
    g_fontTable.push_back(font);
    lua_pushinteger(L, id);
    return 1;
}
int l_loadImage(lua_State* L) {
    std::string path = luaL_checkstring(L, 1);

    // 1. 이미 로드된 적이 있는지 확인
    if (g_pathCache.count(path)) {
        lua_pushinteger(L, g_pathCache[path]);
        return 1;
    }

    // 2. 처음 로드하는 거라면 로직 실행
    STR_TO_WCHAR(path, wPath);
    Image* img = Image::FromFile(wPath);

    int newID = (int)g_imageTable.size();
    g_imageTable.push_back(img);
    g_pathCache[path] = newID; // 캐시에 기록

    lua_pushinteger(L, newID);
    return 1;
}

void register_res(lua_State* L, const char* name) {
    lua_newtable(L);
    REG_METHOD(L, "image", l_loadImage);
    REG_METHOD(L, "font", l_loadFont);
    REG_METHOD(L, "fontFile", l_loadFontFile);
    lua_setglobal(L, name);
}