#include "ResourceHub.h"
#include <Windows.h>
#include <dwrite_3.h>
#include "util.h"

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
        m_factory->CreateFontFileReference(m_filePath.c_str(), nullptr, &m_currentFile);
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
    int size = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, nullptr, 0);
    std::wstring w(size, 0);
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, &w[0], size);
    w.pop_back();
    return w;
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
        WICDecodeMetadataCacheOnLoad, &decoder)))
        return nullptr;

    decoder->GetFrame(0, &frame);
    m_wic->CreateFormatConverter(&conv);

    conv->Initialize(frame.Get(),
        GUID_WICPixelFormat32bppPBGRA,
        WICBitmapDitherTypeNone,
        nullptr, 0.f,
        WICBitmapPaletteTypeMedianCut);

    m_dc->CreateBitmapFromWicBitmap(conv.Get(), nullptr, &bmp);
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
        &fmt)))
        return -1;

    int id = (int)m_fonts.size();
    m_fonts.push_back(fmt);
    m_fontCache[key] = id;

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
    // 키 값으로 '경로 문자열' 자체를 넘깁니다. (널 종료 문자 포함 필수)
    IDWriteFontCollection* pCollection = nullptr;
    HRESULT hr = m_dwrite->CreateCustomFontCollection(
        CustomFontCollectionLoader::Instance,
        wPath.c_str(),
        (uint32_t)((wPath.length() + 1) * sizeof(wchar_t)),
        &pCollection
    );

    if (FAILED(hr)) return -1;

    // 4. 텍스트 포맷 생성
    IDWriteTextFormat* fmt = nullptr;
    hr = m_dwrite->CreateTextFormat(
        wFamily.c_str(),
        pCollection,  // 생성한 커스텀 컬렉션을 주입!
        (DWRITE_FONT_WEIGHT)weight,
        DWRITE_FONT_STYLE_NORMAL,
        DWRITE_FONT_STRETCH_NORMAL,
        size,
        L"ko-kr",
        &fmt
    );

    // 컬렉션은 포맷 내부에서 AddRef 되므로 여기서 Release 해도 안전함
    if (pCollection) pCollection->Release();

    if (FAILED(hr)) return -1;

    // 5. 결과 저장 및 반환
    int id = (int)m_fonts.size();
    m_fonts.push_back(fmt);
    m_fontCache[key] = id;

    return id;
}
int ResourceHub::LoadSound(const std::string& path)
{
    auto it = m_soundCache.find(path);
    if (it != m_soundCache.end())
        return it->second;

    if (GetFileAttributesA(path.c_str()) == INVALID_FILE_ATTRIBUTES)
        return -1;

    auto wav = std::make_shared<SoLoud::Wav>();
    if (wav->load(path.c_str()) != SoLoud::SO_NO_ERROR)
        return -1;

    int id = (int)m_sounds.size();
    m_sounds.push_back(wav);
    m_soundCache[path] = id;
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