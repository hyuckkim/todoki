#pragma once
#include <windows.h>
#include <lua.hpp>
#include <gdiplus.h>
#include <map>
#include <string>
#include <vector>
#include <shlwapi.h>

#pragma comment(lib, "Shlwapi.lib")
#pragma comment (lib, "Gdiplus.lib")
using namespace Gdiplus;

extern Graphics* g_currentGraphics;
extern HWND g_hwnd;
extern int gDrawW, gDrawH;
extern PrivateFontCollection* g_pfc;

extern Color g_currentColor;
extern std::vector<Image*> g_imageTable;
extern std::vector<Font*> g_fontTable;
extern std::map<std::string, int> g_pathCache;


inline std::vector<wchar_t> to_wstring(const char* s) {
    int len = MultiByteToWideChar(CP_UTF8, 0, s, -1, NULL, 0);
    if (len <= 0) return std::vector<wchar_t>();

    std::vector<wchar_t> buf(len);
    MultiByteToWideChar(CP_UTF8, 0, s, -1, buf.data(), len);
    return buf;
}

inline std::vector<wchar_t> to_wstring(const std::string& s) {
    return to_wstring(s.c_str());
}

// wchar_t*를 !생성함!
#define STR_TO_WCHAR(_s, _w) \
    auto _w##_vector = to_wstring(_s); \
    const wchar_t* _w = _w##_vector.data();

#define REG_METHOD(L, name, func) \
    lua_pushcfunction(L, func); \
    lua_setfield(L, -2, name);

void register_res(lua_State* L, const char* name);
void register_draw(lua_State* L, const char* name);
void register_input(lua_State* L, const char* name);
void register_sys(lua_State* L, const char* name);

void unregisterLuaFunctions(lua_State* L);