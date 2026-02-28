#include "ResourceHub.h"
#include <Windows.h>
#include "util.h"

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

    m_bitmaps.clear();
    m_fonts.clear();
    m_sounds.clear();
    m_pathCache.clear();
    m_soundCache.clear();
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
int ResourceHub::LoadFontFile(const std::string& path,
    const std::string& family,
    float size)
{
    int weight = 400;

    std::string key = MakeFontKey(path, family, size, weight);

    auto it = m_fontCache.find(key);
    if (it != m_fontCache.end())
        return it->second;

    if (GetFileAttributesA(path.c_str()) == INVALID_FILE_ATTRIBUTES)
        return -1;

    std::wstring wPath = to_wstring(path);

    if (AddFontResourceExW(wPath.c_str(), FR_PRIVATE, 0) <= 0)
        return -1;

    m_fileFonts.push_back(wPath);

    int id = LoadSystemFont(family, size, weight);
    if (id >= 0)
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