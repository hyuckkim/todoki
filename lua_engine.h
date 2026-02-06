#pragma once
#include <windows.h>

#include <codeanalysis\warnings.h>
#pragma warning( push )
#pragma warning ( disable : ALL_CODE_ANALYSIS_WARNINGS )
#include <sol/sol.hpp>
#include <nlohmann/json.hpp>
#pragma warning( pop )

#include <future>
#include <fstream>
#include <gdiplus.h>
#include <string>
#include <vector>
#include <map>
#include <d2d1.h>
#include <dwrite.h>
#include <wincodec.h> // 이미지 로딩을 위한 WIC

#pragma comment(lib, "d2d1.lib")
#pragma comment(lib, "dwrite.lib")
#pragma comment(lib, "Gdiplus.lib")
#pragma comment(lib, "windowscodecs.lib")
using namespace Gdiplus;
using json = nlohmann::json;

extern ID2D1Factory* g_pD2DFactory;
extern ID2D1DCRenderTarget* g_pDCRT;
extern IDWriteFactory* g_pDWriteFactory;
extern IWICImagingFactory* g_pWICFactory;

extern HWND g_hwnd;
extern sol::state lua;

extern int gDrawW, gDrawH;
extern std::vector<ID2D1Bitmap*> g_bitmapTable;
extern std::vector<IDWriteTextFormat*> g_fontTable;
extern std::map<std::string, int> g_pathCache;

struct StateLayer {
    D2D1_MATRIX_3X2_F matrix;
    int clipDepth; // 해당 push 시점의 클립 깊이
};

struct ITask {
    virtual ~ITask() = default;
    virtual bool check(sol::this_state s) = 0;
    virtual sol::object getResult() = 0;
    bool isDone = false;
};

extern int g_clipCount;
extern std::vector<StateLayer> g_stateStack;

// Lua가 들고 다닐 가벼운 객체
struct JsonNode {
    nlohmann::json* node = nullptr;
};
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


template <class T>
inline void SafeRelease(T** ppT) {
    if (ppT && *ppT) {
        (*ppT)->Release();
        *ppT = nullptr;
    }
}

void register_draw(sol::state& lua, const char* name);
void register_input(sol::state& lua, const char* name);
void register_sys(sol::state& lua, const char* name);
void register_res(sol::state& lua, const char* name);
void RebuildAllBitmaps();