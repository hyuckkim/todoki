#include "lua_engine.h"

// 전역 변수 초기화 (기존 유지)
Color g_currentColor(255, 255, 255, 255);
std::vector<ID2D1Bitmap*> g_bitmapTable;
std::vector<IDWriteTextFormat*> g_fontTable;
std::vector<std::wstring> g_fontFamilyTable;
std::map<std::string, int> g_pathCache;

// sol2에서는 sol::state가 알아서 죽을 때 정리하지만, 
// GDI+ 객체(new로 생성한 것)들은 명시적으로 지워주는 게 좋습니다.
void unregisterLuaFunctions() {
    for (auto img : g_bitmapTable) img->Release();
    for (auto font : g_fontTable) font->Release();
    for (auto& fontPath : g_fontFamilyTable) {
        RemoveFontResourceExW(fontPath.c_str(), FR_PRIVATE, 0);
	}
    g_fontTable.clear();
    g_bitmapTable.clear();
    g_pathCache.clear();
}

void register_res(sol::state& lua, const char* name) {
    auto res = lua.create_named_table(name);

    // 1. 이미지 로드 (캐싱 로직 포함)
    res["image"] = [](std::string path) -> int {
        auto it = g_pathCache.find(path);
        if (it != g_pathCache.end()) return it->second;

        std::wstring wPath = to_wstring(path);

        // 1. 디코더 생성
        IWICBitmapDecoder* pDecoder = nullptr;
        HRESULT hr = g_pWICFactory->CreateDecoderFromFilename(wPath.c_str(), NULL, GENERIC_READ, WICDecodeMetadataCacheOnLoad, &pDecoder);
        
        if (FAILED(hr)) {
            // [오류 처리] 파일이 없거나 열 수 없는 경우
            if (hr == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND)) {
                printf("[Resource Error] File not found: %s\n", path.c_str());
            }
            else {
                printf("[Resource Error] Failed to load '%s' (HRESULT: 0x%08X)\n", path.c_str(), hr);
            }

            // 프로그램이 죽지 않도록 -1을 반환하거나, 0번(기본 에러 이미지)을 반환
            return -1;
        }
        // 2. 첫 번째 프레임 가져오기
        IWICBitmapFrameDecode* pSource = nullptr;
        pDecoder->GetFrame(0, &pSource);

        // 3. 포맷 변환 (D2D가 좋아하는 32bppPBGRA로)
        IWICFormatConverter* pConverter = nullptr;
        g_pWICFactory->CreateFormatConverter(&pConverter);
        pConverter->Initialize(pSource, GUID_WICPixelFormat32bppPBGRA, WICBitmapDitherTypeNone, NULL, 0.f, WICBitmapPaletteTypeMedianCut);

        // 4. D2D 비트맵 생성
        ID2D1Bitmap* pBitmap = nullptr;
        g_pDCRT->CreateBitmapFromWicBitmap(pConverter, NULL, &pBitmap);

        // 정리
        pConverter->Release();
        pSource->Release();
        pDecoder->Release();

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

    // 4. 드디어 대망의 JSON 로더 (여기에 꽂으시면 됩니다)
    res["json"] = [&lua](std::string path) -> sol::object {
        // nlohmann/json 등을 써서 파일을 읽은 뒤 
        // sol::table로 변환해서 반환할 예정입니다.
        // 지금은 일단 nil을 반환하게 둡니다.
        return sol::nil;
        };
}