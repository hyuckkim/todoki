#pragma once
#include <d2d1_1.h>
#include <dwrite.h>
#include <wincodec.h>
#include <wrl/client.h>
#include <sol/sol.hpp>
#include <soloud_wav.h>

#include <vector>
#include <map>
#include <string>
#include <memory>

using Microsoft::WRL::ComPtr;

class ResourceHub {
public:
    static ResourceHub& Instance();

    void Init(ID2D1DeviceContext* dc,
        IDWriteFactory* dwrite,
        IWICImagingFactory* wic);

    void Shutdown();

    void BindLua(sol::state& lua, const char* name);

    ID2D1Bitmap* GetBitmap(int id);
    IDWriteTextFormat* GetFont(int id);
    SoLoud::Wav* GetSound(int id);

private:
    ResourceHub() = default;
    ~ResourceHub();

    // ---- internal helpers ----
    ComPtr<ID2D1Bitmap> LoadBitmapFromFile(const std::string& path);

    int LoadSystemFont(const std::string& name, float size, int weight);
    int LoadFontFile(const std::string& path,
        const std::string& family,
        float size);

    int LoadSound(const std::string& path);

private:
    // factories
    ComPtr<ID2D1DeviceContext> m_dc;
    ComPtr<IDWriteFactory>     m_dwrite;
    ComPtr<IWICImagingFactory> m_wic;

    // ---- resource tables ----
    std::vector<ComPtr<ID2D1Bitmap>> m_bitmaps;
    std::vector<ComPtr<IDWriteTextFormat>> m_fonts;
    std::vector<std::shared_ptr<SoLoud::Wav>> m_sounds;

    // ---- caches ----
    std::map<std::string, int> m_pathCache;
    std::map<std::string, int> m_soundCache;

    // ---- font tracking (for GDI cleanup) ----
    std::vector<std::wstring> m_fileFonts;
    std::vector<HANDLE>       m_memFonts;
    std::map<std::string, int> m_fontCache;
};