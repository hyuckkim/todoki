#include "ResourceHub.h"
#include <Windows.h>
#include <dwrite_3.h>
#include "util.h"
#include <fstream>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

// JSON ↔ Lua 변환 헬퍼 함수
static sol::object convert_json_to_table(const json& j, sol::state_view lua) {
    if (j.is_null()) return sol::nil;
    if (j.is_boolean()) return sol::make_object(lua, j.get<bool>());
    if (j.is_number()) return sol::make_object(lua, j.get<double>());
    if (j.is_string()) return sol::make_object(lua, j.get<std::string>());

    if (j.is_array()) {
        sol::table t = lua.create_table();
        for (size_t i = 0; i < j.size(); ++i) {
            t[i + 1] = convert_json_to_table(j[i], lua);
        }
        return t;
    }
    if (j.is_object()) {
        sol::table t = lua.create_table();
        for (auto& el : j.items()) {
            t[el.key()] = convert_json_to_table(el.value(), lua);
        }
        return t;
    }
    return sol::nil;
}

static json convert_table_to_json(sol::object obj) {
    if (obj.is<sol::nil_t>()) return nullptr;
    if (obj.is<bool>()) return obj.as<bool>();
    if (obj.is<double>()) return obj.as<double>();
    if (obj.is<std::string>()) return obj.as<std::string>();

    if (obj.is<sol::table>()) {
        sol::table t = obj.as<sol::table>();

        // 배열 여부 판별
        bool is_array = false;
        t.for_each([&is_array](sol::object key, sol::object value) {
            if (key.is<int>() && key.as<int>() == 1) is_array = true;
            return false;
        });

        if (is_array) {
            json j = json::array();
            for (size_t i = 1; i <= t.size(); ++i) {
                j.push_back(convert_table_to_json(t[i]));
            }
            return j;
        }
        else {
            json j = json::object();
            t.for_each([&j](sol::object key, sol::object value) {
                if (key.is<std::string>()) {
                    j[key.as<std::string>()] = convert_table_to_json(value);
                }
            });
            return j;
        }
    }
    return nullptr;
}

CustomFontCollectionLoader* CustomFontCollectionLoader::Instance = new CustomFontCollectionLoader();

// --- CustomFontFileEnumerator 구현 ---
CustomFontFileEnumerator::CustomFontFileEnumerator(IDWriteFactory* factory, const std::wstring& path)
    : m_factory(factory), m_filePath(path), m_refCount(1), m_currentFile(nullptr), m_hasNext(true) {
}

CustomFontFileEnumerator::~CustomFontFileEnumerator() {
    if (m_currentFile) m_currentFile->Release();
}

HRESULT STDMETHODCALLTYPE CustomFontFileEnumerator::QueryInterface(REFIID riid, void** ppv) {
    if (riid == __uuidof(IDWriteFontFileEnumerator) || riid == __uuidof(IUnknown)) {
        *ppv = this; AddRef(); return S_OK;
    }
    return E_NOINTERFACE;
}

ULONG STDMETHODCALLTYPE CustomFontFileEnumerator::AddRef() { return InterlockedIncrement(&m_refCount); }
ULONG STDMETHODCALLTYPE CustomFontFileEnumerator::Release() {
    ULONG res = InterlockedDecrement(&m_refCount);
    if (res == 0) delete this; return res;
}

HRESULT STDMETHODCALLTYPE CustomFontFileEnumerator::MoveNext(BOOL* hasNext) {
    *hasNext = m_hasNext;
    if (m_hasNext) {
        HRESULT hr = m_factory->CreateFontFileReference(m_filePath.c_str(), nullptr, &m_currentFile);
        if (FAILED(hr)) {
            *hasNext = FALSE;
        }
        m_hasNext = false;
    }
    return S_OK;
}

HRESULT STDMETHODCALLTYPE CustomFontFileEnumerator::GetCurrentFontFile(IDWriteFontFile** fontFile) {
    if (!m_currentFile) return E_FAIL;
    *fontFile = m_currentFile; (*fontFile)->AddRef(); return S_OK;
}

// --- CustomFontCollectionLoader 구현 ---
HRESULT STDMETHODCALLTYPE CustomFontCollectionLoader::QueryInterface(REFIID riid, void** ppv) {
    if (riid == __uuidof(IDWriteFontCollectionLoader) || riid == __uuidof(IUnknown)) {
        *ppv = this; return S_OK;
    }
    return E_NOINTERFACE;
}

ULONG STDMETHODCALLTYPE CustomFontCollectionLoader::AddRef() { return 1; }
ULONG STDMETHODCALLTYPE CustomFontCollectionLoader::Release() { return 1; }

HRESULT STDMETHODCALLTYPE CustomFontCollectionLoader::CreateEnumeratorFromKey(
    IDWriteFactory* factory, const void* key, UINT32 keySize, IDWriteFontFileEnumerator** enumerator) {
    *enumerator = new CustomFontFileEnumerator(factory, (const wchar_t*)key);
    return S_OK;
}

static std::string MakeFontKey(const std::string& path,
    const std::string& family,
    float size,
    int weight)
{
    return path + "|" + family + "|" +
        std::to_string(size) + "|" +
        std::to_string(weight);
}

ResourceHub& ResourceHub::Instance()
{
    static ResourceHub inst;
    return inst;
}

ResourceHub::~ResourceHub()
{
    // Ensure resources are released deterministically
    Shutdown();
}

void ResourceHub::Init(ID2D1DeviceContext* dc,
    IDWriteFactory* dwrite,
    IWICImagingFactory* wic)
{
    m_dc = dc;
    m_dwrite = dwrite;
    m_wic = wic;
}

void ResourceHub::Shutdown()
{
    // GDI font cleanup
    for (auto& f : m_fileFonts)
        RemoveFontResourceExW(f.c_str(), FR_PRIVATE, 0);

    for (auto& h : m_memFonts)
        RemoveFontMemResourceEx(h);

    // Release D2D/DWrite/WIC references first
    m_bitmaps.clear();
    m_fonts.clear();
    m_sounds.clear();
    m_pathCache.clear();
    m_soundCache.clear();

    m_dc.Reset();
    m_dwrite.Reset();
    m_wic.Reset();

    m_fileFonts.clear();
    m_memFonts.clear();
}

static std::wstring to_wstring(const std::string& s)
{
    if (s.empty()) return L"";

    // Prefer UTF-8
    int size = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, nullptr, 0);
    UINT codePage = CP_UTF8;

    // Fallback for non-UTF8 source strings (e.g. local ACP-encoded script)
    if (size <= 0) {
        size = MultiByteToWideChar(CP_ACP, 0, s.c_str(), -1, nullptr, 0);
        codePage = CP_ACP;
    }

    if (size <= 0) return L"";

    std::wstring w(size, 0);
    MultiByteToWideChar(codePage, 0, s.c_str(), -1, &w[0], size);
    if (!w.empty() && w.back() == L'\0') w.pop_back();
    return w;
}

static std::string to_utf8(const std::wstring& w)
{
    if (w.empty()) return "";
    int size = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), -1, nullptr, 0, nullptr, nullptr);
    if (size <= 0) return "";
    std::string s(size, '\0');
    WideCharToMultiByte(CP_UTF8, 0, w.c_str(), -1, &s[0], size, nullptr, nullptr);
    if (!s.empty() && s.back() == '\0') s.pop_back();
    return s;
}

ComPtr<ID2D1Bitmap>
ResourceHub::LoadBitmapFromFile(const std::string& path)
{
    std::wstring wPath = to_wstring(path);

    ComPtr<IWICBitmapDecoder> decoder;
    ComPtr<IWICBitmapFrameDecode> frame;
    ComPtr<IWICFormatConverter> conv;
    ComPtr<ID2D1Bitmap> bmp;

    if (FAILED(m_wic->CreateDecoderFromFilename(
        wPath.c_str(), nullptr, GENERIC_READ,
        WICDecodeMetadataCacheOnLoad, &decoder))) {
        printf("[RES] image fail: %s\n", path.c_str());
        return nullptr;
    }

    decoder->GetFrame(0, &frame);
    m_wic->CreateFormatConverter(&conv);

    conv->Initialize(frame.Get(),
        GUID_WICPixelFormat32bppPBGRA,
        WICBitmapDitherTypeNone,
        nullptr, 0.f,
        WICBitmapPaletteTypeMedianCut);

    m_dc->CreateBitmapFromWicBitmap(conv.Get(), nullptr, &bmp);
    printf("[RES] image: %s\n", path.c_str());
    return bmp;
}
int ResourceHub::LoadSystemFont(const std::string& name,
    float size,
    int weight)
{
    std::string key = MakeFontKey("", name, size, weight);

    auto it = m_fontCache.find(key);
    if (it != m_fontCache.end())
        return it->second;

    std::wstring wName = to_wstring(name);

    IDWriteTextFormat* fmt = nullptr;

    if (FAILED(m_dwrite->CreateTextFormat(
        wName.c_str(),
        nullptr,
        (DWRITE_FONT_WEIGHT)weight,
        DWRITE_FONT_STYLE_NORMAL,
        DWRITE_FONT_STRETCH_NORMAL,
        size,
        L"ko-kr",
        &fmt))) {
        printf("[RES] system font fail: %s\n", name.c_str());
        return -1;
    }

    int id = (int)m_fonts.size();
    m_fonts.push_back(fmt);
    m_fontCache[key] = id;
    printf("[RES] system font: %s\n", name.c_str());

    return id;
}
int ResourceHub::LoadFontFile(const std::string& path, const std::string& family, float size)
{
    // 1. 캐시 체크 (무거운 로직 방지)
    int weight = 400;
    std::string key = MakeFontKey(path, family, size, weight);
    if (m_fontCache.count(key)) return m_fontCache[key];

    std::wstring wPath = to_wstring(path);
    std::wstring wFamily = to_wstring(family);

    // 2. 로더 등록 (이미 등록되어 있어도 안전함)
    m_dwrite->RegisterFontCollectionLoader(CustomFontCollectionLoader::Instance);

    // 3. 커스텀 컬렉션 생성
    IDWriteFontCollection* pCollection = nullptr;

    // 절대경로로 변환
    char fullPath[MAX_PATH];
    if (!GetFullPathNameA(path.c_str(), MAX_PATH, fullPath, nullptr)) {
        printf("[RES] fontFile fail: %s\n", path.c_str());
        return -1;
    }

    std::wstring wFullPath = to_wstring(fullPath);

    HRESULT hr = m_dwrite->CreateCustomFontCollection(
        CustomFontCollectionLoader::Instance,
        wFullPath.c_str(),
        (uint32_t)((wFullPath.length() + 1) * sizeof(wchar_t)),
        &pCollection
    );

    if (FAILED(hr)) {
        printf("[RES] fontFile fail: %s\n", path.c_str());
        return -1;
    }

    // 4. 요청 family가 컬렉션에 없으면 첫 번째 family를 강제로 사용
    std::wstring resolvedFamily = wFamily;
    bool usedFirstFamilyFallback = false;
    UINT32 familyIndex = 0;
    BOOL familyExists = FALSE;
    hr = pCollection->FindFamilyName(wFamily.c_str(), &familyIndex, &familyExists);

    if (FAILED(hr) || !familyExists) {
        UINT32 familyCount = pCollection->GetFontFamilyCount();
        if (familyCount > 0) {
            IDWriteFontFamily* pFamily = nullptr;
            if (SUCCEEDED(pCollection->GetFontFamily(0, &pFamily)) && pFamily) {
                IDWriteLocalizedStrings* pNames = nullptr;
                if (SUCCEEDED(pFamily->GetFamilyNames(&pNames)) && pNames) {
                    UINT32 nameIndex = 0;
                    BOOL exists = FALSE;
                    pNames->FindLocaleName(L"ko-kr", &nameIndex, &exists);
                    if (!exists) pNames->FindLocaleName(L"en-us", &nameIndex, &exists);
                    if (!exists) nameIndex = 0;

                    UINT32 nameLen = 0;
                    if (SUCCEEDED(pNames->GetStringLength(nameIndex, &nameLen))) {
                        std::wstring name(nameLen + 1, L'\0');
                        if (SUCCEEDED(pNames->GetString(nameIndex, &name[0], nameLen + 1))) {
                            name.resize(nameLen);
                            resolvedFamily = name;
                            usedFirstFamilyFallback = true;
                        }
                    }
                    pNames->Release();
                }
                pFamily->Release();
            }
        }
    }

    if (usedFirstFamilyFallback) {
        if (resolvedFamily.empty()) {
            resolvedFamily = wFamily;
        }
        std::string familyUtf8 = to_utf8(resolvedFamily);
        if (familyUtf8.empty()) familyUtf8 = "<empty>";
        printf("[RES] fontFile fallback family: %s\n", familyUtf8.c_str());
    }

    // 5. 텍스트 포맷 생성
    IDWriteTextFormat* fmt = nullptr;
    hr = m_dwrite->CreateTextFormat(
        resolvedFamily.c_str(),
        pCollection,
        (DWRITE_FONT_WEIGHT)weight,
        DWRITE_FONT_STYLE_NORMAL,
        DWRITE_FONT_STRETCH_NORMAL,
        size,
        L"ko-kr",
        &fmt
    );

    if (pCollection) pCollection->Release();

    if (FAILED(hr)) {
        printf("[RES] fontFile fail: %s\n", path.c_str());
        return -1;
    }

    // 6. 결과 저장 및 반환
    int id = (int)m_fonts.size();
    m_fonts.push_back(fmt);
    m_fontCache[key] = id;

    printf("[RES] fontFile: %s\n", path.c_str());

    return id;
}
int ResourceHub::LoadSound(const std::string& path)
{
    auto it = m_soundCache.find(path);
    if (it != m_soundCache.end())
        return it->second;

    if (GetFileAttributesA(path.c_str()) == INVALID_FILE_ATTRIBUTES) {
        printf("[RES] sound fail: %s\n", path.c_str());
        return -1;
    }

    auto wav = std::make_shared<SoLoud::Wav>();
    if (wav->load(path.c_str()) != SoLoud::SO_NO_ERROR) {
        printf("[RES] sound fail: %s\n", path.c_str());
        return -1;
    }

    int id = (int)m_sounds.size();
    m_sounds.push_back(wav);
    m_soundCache[path] = id;
    printf("[RES] sound: %s\n", path.c_str());
    return id;
}
void ResourceHub::BindLua(sol::state& lua, const char* name)
{
    auto res = lua.create_named_table(name);

    res["image"] = [this](std::string path)
        {
            auto it = m_pathCache.find(path);
            if (it != m_pathCache.end())
                return it->second;

            auto bmp = LoadBitmapFromFile(path);
            if (!bmp) return -1;

            int id = (int)m_bitmaps.size();
            m_bitmaps.push_back(bmp);
            m_pathCache[path] = id;
            return id;
        };

    res["font"] = [this](std::string name,
        float size,
        sol::optional<int> weight)
        {
            return LoadSystemFont(name, size,
                weight.value_or(400));
        };

    res["fontFile"] = [this](std::string path,
        std::string family,
        float size)
        {
            return LoadFontFile(path, family, size);
        };

    res["sound"] = [this](std::string path)
        {
            return LoadSound(path);
        };

    // JSON 입출력
    res["loadjson"] = [](std::string path, sol::this_state s) -> sol::object {
        std::ifstream file(path);
        if (!file.is_open()) {
            printf("[RES] json fail: %s\n", path.c_str());
            return sol::nil;
        }
        try {
            json j;
            file >> j;
            sol::state_view lua(s);
            printf("[RES] json: %s\n", path.c_str());
            return convert_json_to_table(j, lua);
        }
        catch (...) {
            printf("[RES] json fail: %s\n", path.c_str());
            return sol::nil;
        }
    };

    res["savejson"] = [](std::string path, sol::object table) -> bool {
        std::ofstream file(path);
        if (!file.is_open()) {
            printf("[RES] json save fail: %s\n", path.c_str());
            return false;
        }
        try {
            json j = convert_table_to_json(table);
            file << j.dump(4);
            printf("[RES] json save: %s\n", path.c_str());
            return true;
        }
        catch (...) {
            printf("[RES] json save fail: %s\n", path.c_str());
            return false;
        }
    };
}
ID2D1Bitmap* ResourceHub::GetBitmap(int id)
{
    if (id < 0 || id >= (int)m_bitmaps.size())
        return nullptr;

    return m_bitmaps[id].Get();
}
IDWriteTextFormat* ResourceHub::GetFont(int id)
{
    if (id < 0 || id >= (int)m_fonts.size())
        return nullptr;

    return m_fonts[id].Get();
}
SoLoud::Wav* ResourceHub::GetSound(int id)
{
    if (id < 0 || id >= (int)m_sounds.size())
        return nullptr;

    return m_sounds[id].get();
}