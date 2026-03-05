#pragma once
#include <d2d1_1.h>
#include <dwrite.h>
#include <wincodec.h>
#include <d3d11.h>
#include <wrl/client.h>
#include <sol/sol.hpp>
#include <soloud_wav.h>

#include <vector>
#include <map>
#include <string>
#include <memory>

struct LuaBindContext;

using Microsoft::WRL::ComPtr;

class CustomFontFileEnumerator : public IDWriteFontFileEnumerator {
    ULONG m_refCount;
    IDWriteFactory* m_factory;
    IDWriteFontFile* m_currentFile;
    std::wstring m_filePath;
    bool m_hasNext;

public:
    CustomFontFileEnumerator(IDWriteFactory* factory, const std::wstring& path);
    ~CustomFontFileEnumerator();

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppv) override;
    ULONG STDMETHODCALLTYPE AddRef() override;
    ULONG STDMETHODCALLTYPE Release() override;
    HRESULT STDMETHODCALLTYPE MoveNext(BOOL* hasNext) override;
    HRESULT STDMETHODCALLTYPE GetCurrentFontFile(IDWriteFontFile** fontFile) override;
};

// Loader 선언
class CustomFontCollectionLoader : public IDWriteFontCollectionLoader {
public:
    // 헤더에는 선언만 합니다.
    static CustomFontCollectionLoader* Instance;

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppv) override;
    ULONG STDMETHODCALLTYPE AddRef() override;
    ULONG STDMETHODCALLTYPE Release() override;
    HRESULT STDMETHODCALLTYPE CreateEnumeratorFromKey(
        IDWriteFactory* factory,
        const void* key,
        UINT32 keySize,
        IDWriteFontFileEnumerator** enumerator) override;
};
class ResourceHub {
public:
    static ResourceHub& Instance();

    void Init(ID2D1DeviceContext* dc,
        IDWriteFactory* dwrite,
        IWICImagingFactory* wic,
        ID3D11Device* d3d,
        ID3D11DeviceContext* d3dCtx);

    void Shutdown();

    void BindLua(LuaBindContext& ctx);

    ID2D1Bitmap* GetBitmap(int id);
    IDWriteTextFormat* GetFont(int id);
    SoLoud::Wav* GetSound(int id);
    ID3D11PixelShader* GetPixelShader(int id);
    ID3D11Device* GetD3DDevice();
    ID3D11DeviceContext* GetD3DContext();

private:
    ResourceHub() = default;
    ~ResourceHub();

    // ---- internal helpers ----
    ComPtr<ID2D1Bitmap> LoadBitmapFromFile(const std::string& path);
    int LoadPixelShader(const std::string& path, const std::string& entryPoint);

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
    ComPtr<ID3D11Device> m_d3d;
    ComPtr<ID3D11DeviceContext> m_d3dCtx;

    // ---- resource tables ----
    std::vector<ComPtr<ID2D1Bitmap>> m_bitmaps;
    std::vector<ComPtr<IDWriteTextFormat>> m_fonts;
    std::vector<std::shared_ptr<SoLoud::Wav>> m_sounds;
    std::vector<ComPtr<ID3D11PixelShader>> m_pixelShaders;

    // ---- caches ----
    std::map<std::string, int> m_pathCache;
    std::map<std::string, int> m_soundCache;
    std::map<std::string, int> m_shaderCache;

    // ---- font tracking (for GDI cleanup) ----
    std::vector<std::wstring> m_fileFonts;
    std::vector<HANDLE>       m_memFonts;
    std::map<std::string, int> m_fontCache;
    IDWriteFontCollection* m_customFontCollection = nullptr;
};