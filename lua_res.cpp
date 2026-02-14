#include "util.h"
#include "LuaJson.h"
#include "engine_graphic.h"
#include "sol.h"
#include "LuaRandom.h"
#include <soloud_wav.h>
#include "packManager.cpp"

// 전역 변수 초기화 (기존 유지)
std::vector<ComPtr<ID2D1Bitmap>> g_bitmapTable;
std::vector<ComPtr<IDWriteTextFormat>> g_fontTable;
std::vector<std::shared_ptr<SoLoud::Wav>> g_soundTable;
std::map<std::string, int> g_soundPathCache; // 사운드용 캐시

std::map<std::string, int> g_pathCache;
std::vector<std::wstring> g_fontFamilyTable;

ComPtr<ID2D1Bitmap> LoadBitmapFromMemory(ID2D1DeviceContext* rt, const uint8_t* pData, size_t size) {
    if (!pData || size == 0) return nullptr;

    ComPtr<IWICStream> pStream;
    ComPtr<IWICBitmapDecoder> pDecoder;
    ComPtr<IWICBitmapFrameDecode> pSource;
    ComPtr<IWICFormatConverter> pConverter;
    ComPtr<ID2D1Bitmap> pBitmap;

    if (FAILED(g_pWICFactory->CreateStream(&pStream))) return nullptr;
    if (FAILED(pStream->InitializeFromMemory(const_cast<BYTE*>(pData), (DWORD)size))) return nullptr;

    if (FAILED(g_pWICFactory->CreateDecoderFromStream(pStream.Get(), NULL, WICDecodeMetadataCacheOnLoad, &pDecoder))) return nullptr;
    if (FAILED(pDecoder->GetFrame(0, &pSource))) return nullptr;

    g_pWICFactory->CreateFormatConverter(&pConverter);
    pConverter->Initialize(pSource.Get(), GUID_WICPixelFormat32bppPBGRA, WICBitmapDitherTypeNone, NULL, 0.f, WICBitmapPaletteTypeMedianCut);

    rt->CreateBitmapFromWicBitmap(pConverter.Get(), NULL, &pBitmap);
    return pBitmap;
}

ComPtr<ID2D1Bitmap> LoadBitmapFromFile(ID2D1DeviceContext* rt, const std::string& path) {
    std::wstring wPath = to_wstring(path);

    // ComPtr을 사용하면 함수 종료 시 자동으로 Release됩니다.
    ComPtr<IWICBitmapDecoder> pDecoder;
    ComPtr<IWICBitmapFrameDecode> pSource;
    ComPtr<IWICFormatConverter> pConverter;
    ComPtr<ID2D1Bitmap> pBitmap;

    HRESULT hr = g_pWICFactory->CreateDecoderFromFilename(
        wPath.c_str(), NULL, GENERIC_READ, WICDecodeMetadataCacheOnLoad, &pDecoder
    );
    if (FAILED(hr)) return nullptr;

    hr = pDecoder->GetFrame(0, &pSource);
    if (FAILED(hr)) return nullptr;

    hr = g_pWICFactory->CreateFormatConverter(&pConverter);
    if (FAILED(hr)) return nullptr;

    hr = pConverter->Initialize(
        pSource.Get(), GUID_WICPixelFormat32bppPBGRA,
        WICBitmapDitherTypeNone, NULL, 0.f, WICBitmapPaletteTypeMedianCut
    );
    if (FAILED(hr)) return nullptr;

    // 최종 비트맵 생성
    hr = rt->CreateBitmapFromWicBitmap(pConverter.Get(), NULL, &pBitmap);
    if (FAILED(hr)) return nullptr;

    return pBitmap;
}

void unregisterLuaFunctions() {
    // 1. 비트맵 테이블 강제 리셋
    for (auto& bmp : g_bitmapTable) {
        bmp.Reset(); // 각 ComPtr을 명시적으로 Reset
    }
    g_bitmapTable.clear();

    // 2. 폰트 테이블 강제 리셋
    for (auto& font : g_fontTable) {
        font.Reset();
    }
    g_fontTable.clear();

    // 3. 캐시 및 기타 테이블 정리
    g_pathCache.clear();
    g_fontFamilyTable.clear();

    for (auto& snd : g_soundTable) {
        snd.reset();
    }
    g_soundTable.clear();
    g_soundPathCache.clear();
}

void RebuildAllBitmaps() {
    // 1. 기존 비트맵 모두 해제 (ComPtr이므로 clear만으로 충분)
    for (auto& bmp : g_bitmapTable) bmp.Reset();

    // 2. 캐시된 경로를 바탕으로 재생성
    for (const auto& [path, index] : g_pathCache) {
        if (index < 0 || index >= (int)g_bitmapTable.size()) continue;

        // .Get()을 통해 현재 장치 컨텍스트 전달
        g_bitmapTable[index] = LoadBitmapFromFile(g_pD2DDC.Get(), path);
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
        // 1. 중복 로드 방지 캐시
        auto it = g_pathCache.find(path);
        if (it != g_pathCache.end()) return it->second;

        ComPtr<ID2D1Bitmap> pBitmap;

        // 2. 파일 시스템 우선 순위 (Disk Check)
        // GetFileAttributesA는 파일이 실제로 디스크에 있을 때만 성공합니다.
        if (GetFileAttributesA(path.c_str()) != INVALID_FILE_ATTRIBUTES) {
            pBitmap = LoadBitmapFromFile(g_pD2DDC.Get(), path);
        }
        // 3. 디스크에 없으면 PAK 파일에서 검색
        else {
            auto data = PackManager::Instance().GetFileData(path);
            if (!data.empty()) {
                pBitmap = LoadBitmapFromMemory(g_pD2DDC.Get(), data.data(), data.size());
            }
        }

        if (!pBitmap) return -1;

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
        std::wstring wName = to_wstring(familyName);
        HANDLE hFontRes = nullptr;
        bool isMemFont = false;

        // 1. 로컬 파일 확인 (우선순위)
        if (GetFileAttributesA(path.c_str()) != INVALID_FILE_ATTRIBUTES) {
            std::wstring wPath = to_wstring(path);
            if (AddFontResourceExW(wPath.c_str(), FR_PRIVATE, 0) > 0) {
                g_fontFamilyTable.push_back(wPath); // 나중에 해제하기 위함
            }
            else {
                return -1;
            }
        }
        // 2. PAK 파일 확인
        else {
            auto data = PackManager::Instance().GetFileData(path);
            if (data.empty()) return -1;

            // 메모리에 로드된 폰트 데이터를 OS에 등록
            DWORD numFonts = 0;
            hFontRes = AddFontMemResourceEx(data.data(), (DWORD)data.size(), nullptr, &numFonts);

            if (!hFontRes) {
                printf("[Resource Error] Failed to load font from memory: %s\n", path.c_str());
                return -1;
            }
            isMemFont = true;
        }

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
            // 실패 시 등록했던 폰트 해제
            if (isMemFont) RemoveFontMemResourceEx(hFontRes);
            else RemoveFontResourceExW(to_wstring(path).c_str(), FR_PRIVATE, 0);
            return -1;
        }

        int id = (int)g_fontTable.size();
        g_fontTable.push_back(pTextFormat);
        return id;
        };

    res["sound"] = [](std::string path) -> int {
        // 1. 중복 로드 방지 캐시 확인
        auto it = g_soundPathCache.find(path);
        if (it != g_soundPathCache.end()) return it->second;

        auto pWav = std::make_shared<SoLoud::Wav>();
        SoLoud::result hr;

        // 2. 파일 시스템 우선 순위
        if (GetFileAttributesA(path.c_str()) != INVALID_FILE_ATTRIBUTES) {
            hr = pWav->load(path.c_str());
        }
        // 3. PAK 파일 확인
        else {
            auto data = PackManager::Instance().GetFileData(path);
            if (!data.empty()) {
                // 데이터를 복사해서 들고 있도록(true, true) 설정
                hr = pWav->loadMem(data.data(), (unsigned int)data.size(), true, true);
            }
            else {
                return -1;
            }
        }

        if (hr != SoLoud::SO_NO_ERROR) return -1;

        int newID = (int)g_soundTable.size();
        g_soundTable.push_back(pWav);
        g_soundPathCache[path] = newID;
        return newID;
        };


    // 분리한 JSON 모듈 등록 호출
    sol::state_view lua_view(lua);
    register_json_module(lua_view, name);
    register_random_module(lua_view, name);
}
