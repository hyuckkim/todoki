#pragma once
#include <windows.h>
#include <sol/sol.hpp>
#include <gdiplus.h>
#include <string>
#include <vector>
#include <map>

#pragma comment(lib, "Gdiplus.lib")
using namespace Gdiplus;

extern Graphics* g_currentGraphics;
extern HWND g_hwnd;
extern sol::state lua;

extern Graphics* g_currentGraphics;
extern HWND g_hwnd;
extern int gDrawW, gDrawH;
extern PrivateFontCollection* g_pfc;

extern Color g_currentColor;
extern std::vector<Image*> g_imageTable;
extern std::vector<Font*> g_fontTable;
extern std::map<std::string, int> g_pathCache;

inline std::wstring to_wstring(const std::string& s) {
    if (s.empty()) return L"";

    // 필요한 크기 계산
    int len = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, NULL, 0);
    if (len <= 0) return L"";

    // wstring 공간 확보
    std::wstring buf(len, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, &buf[0], len);

    // MultiByteToWideChar는 널 문자를 포함하므로, 끝의 \0를 제거해주는 게 좋습니다.
    if (!buf.empty() && buf.back() == L'\0') {
        buf.pop_back();
    }

    return buf;
}

void register_draw(sol::state& lua, const char* name);
void register_input(sol::state& lua, const char* name);
void register_sys(sol::state& lua, const char* name);
void register_res(sol::state& lua, const char* name);