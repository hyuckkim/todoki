#include "util.h"
#include "LuaJson.h"
#include "engine_graphic.h"
#include "sol.h"
#include <gdiplus.h>

// 전역 변수 초기화 (기존 유지)
Gdiplus::Color g_currentColor(255, 255, 255, 255);
std::vector<ID2D1Bitmap*> g_bitmapTable;
std::map<std::string, int> g_pathCache;
std::vector<IDWriteTextFormat*> g_fontTable;
std::vector<std::wstring> g_fontFamilyTable;

ID2D1Bitmap* LoadBitmapFromFile(
    ID2D1DeviceContext* rt,
    const std::string& path
) {
    std::wstring wPath = to_wstring(path);

    IWICBitmapDecoder* pDecoder = nullptr;
    HRESULT hr = g_pWICFactory->CreateDecoderFromFilename(
        wPath.c_str(),
        NULL,
        GENERIC_READ,
        WICDecodeMetadataCacheOnLoad,
        &pDecoder
    );

    if (FAILED(hr)) {
        if (hr == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND)) {
            printf("[Resource Error] File not found: %s\n", path.c_str());
        }
        else {
            printf("[Resource Error] Failed to load '%s' (HRESULT: 0x%08X)\n", path.c_str(), hr);
        }
        return nullptr;
    }

    IWICBitmapFrameDecode* pSource = nullptr;
    pDecoder->GetFrame(0, &pSource);

    IWICFormatConverter* pConverter = nullptr;
    g_pWICFactory->CreateFormatConverter(&pConverter);
    pConverter->Initialize(
        pSource,
        GUID_WICPixelFormat32bppPBGRA,
        WICBitmapDitherTypeNone,
        NULL,
        0.f,
        WICBitmapPaletteTypeMedianCut
    );

    ID2D1Bitmap* pBitmap = nullptr;
    rt->CreateBitmapFromWicBitmap(pConverter, NULL, &pBitmap);

    pConverter->Release();
    pSource->Release();
    pDecoder->Release();

    return pBitmap; // 실패 시 nullptr 가능
}

void unregisterLuaFunctions() {
    for (auto img : g_bitmapTable) if (img) img->Release();
    for (auto font : g_fontTable) if (font) font->Release();
    for (auto& fontPath : g_fontFamilyTable) {
        RemoveFontResourceExW(fontPath.c_str(), FR_PRIVATE, 0);
	}
    g_fontTable.clear();
    g_bitmapTable.clear();
    g_pathCache.clear();
}

void RebuildAllBitmaps() {
    for (auto& bmp : g_bitmapTable) {
        SafeRelease(&bmp);
    }
    for (const auto& [path, index]: g_pathCache) {
        if (index < 0 || index >= (int)g_bitmapTable.size())
            continue; // 방어

        ID2D1Bitmap* bmp = LoadBitmapFromFile(g_pD2DDC, path);
        g_bitmapTable[index] = bmp; // 실패 시 nullptr
    }
}

void register_res(sol::state& lua, const char* name) {
    // 공통 Task 타입 등록
    lua.new_usertype<ITask>("Task",
        "check", &ITask::check,
        "getResult", &ITask::getResult,
        "isDone", sol::readonly(&ITask::isDone)
    );

    // 이미지/폰트 등 기존 로직 등록...
    auto res = lua.create_named_table(name);
    res["image"] = [](std::string path) -> int {
        auto it = g_pathCache.find(path);
        if (it != g_pathCache.end())
            return it->second;

        ID2D1Bitmap* pBitmap = LoadBitmapFromFile(g_pD2DDC, path);
        if (!pBitmap)
            return -1;

        int newID = (int)g_bitmapTable.size();
        g_bitmapTable.push_back(pBitmap);
        g_pathCache[path] = newID;
        return newID;
        };


    // 2. 시스템 폰트 로드
    res["font"] = [](std::string name, float size, sol::optional<int> weight) -> int {
        std::wstring wName = to_wstring(name);

        IDWriteTextFormat* pTextFormat = nullptr;
        // weight: DWRITE_FONT_WEIGHT_NORMAL (400) 등 사용
        g_pDWriteFactory->CreateTextFormat(
            wName.c_str(), NULL,
            (DWRITE_FONT_WEIGHT)weight.value_or(400),
            DWRITE_FONT_STYLE_NORMAL,
            DWRITE_FONT_STRETCH_NORMAL,
            size, L"ko-kr", &pTextFormat
        );

        int id = (int)g_fontTable.size();
        g_fontTable.push_back(pTextFormat);
        return id;
        };

    // 3. 폰트 파일(.ttf) 로드
    res["fontFile"] = [](std::string path, std::string familyName, float size) -> int {
        // 1. 파일 존재 여부 확인 (기본적인 가드)
        std::wstring wPath = to_wstring(path);
        std::wstring wName = to_wstring(familyName);

        // 2. OS에 폰트 등록 시도
        // 반환값이 0이면 등록 실패 (파일이 없거나 형식이 잘못됨)
        int fontsAdded = AddFontResourceExW(wPath.c_str(), FR_PRIVATE, 0);

        if (fontsAdded == 0) {
            printf("[Resource Error] Font file not found or invalid: %s\n", path.c_str());
            // 실패 시 -1 반환 혹은 기본 폰트 처리
            return -1;
        }

        g_fontFamilyTable.push_back(wPath);

        // 3. TextFormat 생성
        IDWriteTextFormat* pTextFormat = nullptr;
        HRESULT hr = g_pDWriteFactory->CreateTextFormat(
            wName.c_str(),
            nullptr,
            DWRITE_FONT_WEIGHT_NORMAL,
            DWRITE_FONT_STYLE_NORMAL,
            DWRITE_FONT_STRETCH_NORMAL,
            size, L"ko-kr", &pTextFormat
        );

        if (FAILED(hr)) {
            printf("[Resource Error] Failed to create TextFormat for: %s (HRESULT: 0x%08X)\n", familyName.c_str(), hr);
            // 등록했던 리소스 해제
            RemoveFontResourceExW(wPath.c_str(), FR_PRIVATE, 0);
            return -1;
        }

        int id = (int)g_fontTable.size();
        g_fontTable.push_back(pTextFormat);

        return id;
        };

    // 분리한 JSON 모듈 등록 호출
    sol::state_view lua_view(lua);
    register_json_module(lua_view, name);
}
