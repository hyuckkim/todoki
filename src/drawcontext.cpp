#include "DrawContext.h"
#include <d2d1_1.h>
#include <d3dcompiler.h>
#include <algorithm>
#include <cstring>
#include <wrl/client.h>
#include "resourcehub.h"
#include "util.h"
#include "luabind.h"

using Microsoft::WRL::ComPtr;

namespace {
    HRESULT GetOrCreateFullscreenVS(ID3D11Device* device, ID3D11VertexShader** outVs) {
        if (!device || !outVs) return E_INVALIDARG;

        static ComPtr<ID3D11VertexShader> s_vs;
        if (s_vs) {
            *outVs = s_vs.Get();
            (*outVs)->AddRef();
            return S_OK;
        }

        static const char* kVsSrc = R"(
struct VSOut {
    float4 pos : SV_POSITION;
    float2 uv  : TEXCOORD0;
};
VSOut main(uint vid : SV_VertexID)
{
    float2 pos[3] = {
        float2(-1.0, -1.0),
        float2(-1.0,  3.0),
        float2( 3.0, -1.0)
    };
    float2 uv[3] = {
        float2(0.0, 1.0),
        float2(0.0, -1.0),
        float2(2.0, 1.0)
    };
    VSOut o;
    o.pos = float4(pos[vid], 0.0, 1.0);
    o.uv = uv[vid];
    return o;
}
)";

        ComPtr<ID3DBlob> vsBlob;
        ComPtr<ID3DBlob> errBlob;
        UINT flags = D3DCOMPILE_ENABLE_STRICTNESS;
#ifdef _DEBUG
        flags |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif
        HRESULT hr = D3DCompile(
            kVsSrc,
            strlen(kVsSrc),
            nullptr,
            nullptr,
            nullptr,
            "main",
            "vs_5_0",
            flags,
            0,
            &vsBlob,
            &errBlob);

        if (FAILED(hr) || !vsBlob) {
            return FAILED(hr) ? hr : E_FAIL;
        }

        hr = device->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), nullptr, &s_vs);
        if (FAILED(hr)) return hr;

        *outVs = s_vs.Get();
        (*outVs)->AddRef();
        return S_OK;
    }

    HRESULT GetOrCreateLinearSampler(ID3D11Device* device, ID3D11SamplerState** outSampler) {
        if (!device || !outSampler) return E_INVALIDARG;

        static ComPtr<ID3D11SamplerState> s_sampler;
        if (s_sampler) {
            *outSampler = s_sampler.Get();
            (*outSampler)->AddRef();
            return S_OK;
        }

        D3D11_SAMPLER_DESC desc = {};
        desc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
        desc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
        desc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
        desc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
        desc.MinLOD = 0;
        desc.MaxLOD = D3D11_FLOAT32_MAX;

        HRESULT hr = device->CreateSamplerState(&desc, &s_sampler);
        if (FAILED(hr)) return hr;

        *outSampler = s_sampler.Get();
        (*outSampler)->AddRef();
        return S_OK;
    }

    HRESULT GetOrCreateNoCullRasterizer(ID3D11Device* device, ID3D11RasterizerState** outRs) {
        if (!device || !outRs) return E_INVALIDARG;

        static ComPtr<ID3D11RasterizerState> s_rs;
        if (s_rs) {
            *outRs = s_rs.Get();
            (*outRs)->AddRef();
            return S_OK;
        }

        D3D11_RASTERIZER_DESC rd = {};
        rd.FillMode = D3D11_FILL_SOLID;
        rd.CullMode = D3D11_CULL_NONE;
        rd.FrontCounterClockwise = FALSE;
        rd.DepthClipEnable = TRUE;

        HRESULT hr = device->CreateRasterizerState(&rd, &s_rs);
        if (FAILED(hr)) return hr;

        *outRs = s_rs.Get();
        (*outRs)->AddRef();
        return S_OK;
    }
}

DrawContext::DrawContext(ID2D1RenderTarget* renderTarget, ID2D1Factory1* factory)
    : m_rt(renderTarget), m_factory(factory)
{
    if (m_rt) {
        m_rt->CreateSolidColorBrush(m_color, &m_brush);

        ComPtr<ID2D1DeviceContext> dc;
        if (SUCCEEDED(m_rt.As(&dc)) && dc) {
            ComPtr<ID2D1Image> target;
            dc->GetTarget(&target);
            if (target) target.As(&m_targetBitmap);
        }
    }
}

DrawContext::~DrawContext() {
    if (isBatching && m_rt) {
        m_rt->EndDraw();
        isBatching = false;
    }
    m_brush.Reset();
    m_rt.Reset();
}

void DrawContext::EnsureDrawSession() {
    if (!m_rt || isBatching || !m_targetBitmap) return;

    const auto opts = m_targetBitmap->GetOptions();
    if ((opts & D2D1_BITMAP_OPTIONS_CANNOT_DRAW) != 0) return;

    m_rt->BeginDraw();
    isBatching = true;
}

// --- 그리기 함수 ---
void DrawContext::rect(float x, float y, float w, float h, sol::optional<bool> fill) {
    EnsureDrawSession();
    if (!m_rt || !m_brush) return;
    auto r = D2D1::RectF(x, y, x + w, y + h);
    if (fill.value_or(true)) m_rt->FillRectangle(r, m_brush.Get());
    else m_rt->DrawRectangle(r, m_brush.Get(), m_strokeWidth);
}

void DrawContext::circle(float x, float y, float r, sol::optional<bool> fill) {
    EnsureDrawSession();
    if (!m_rt || !m_brush) return;
    auto e = D2D1::Ellipse(D2D1::Point2F(x, y), r, r);
    if (fill.value_or(true)) m_rt->FillEllipse(e, m_brush.Get());
    else m_rt->DrawEllipse(e, m_brush.Get(), m_strokeWidth);
}

void DrawContext::polyline(sol::table vertices, sol::optional<bool> closed) {
    EnsureDrawSession();
    if (!m_rt || !m_factory || vertices.size() < 4) return;
    
    // 좌표 쌍을 위해 배열 크기가 짝수여야 함
    if (vertices.size() % 2 != 0) return;
    
    ComPtr<ID2D1PathGeometry> path;
    m_factory->CreatePathGeometry(&path);
    ComPtr<ID2D1GeometrySink> sink;
    path->Open(&sink);

    sink->BeginFigure(D2D1::Point2F(vertices[1], vertices[2]), D2D1_FIGURE_BEGIN_FILLED);
    for (size_t i = 3; i < vertices.size(); i += 2) {
        sink->AddLine(D2D1::Point2F(vertices[i], vertices[i + 1]));
    }
    sink->EndFigure(closed.value_or(false) ? D2D1_FIGURE_END_CLOSED : D2D1_FIGURE_END_OPEN);
    sink->Close();
    m_rt->DrawGeometry(path.Get(), m_brush.Get(), m_strokeWidth);
}

void DrawContext::polygon(sol::table vertices) {
    EnsureDrawSession();
    if (!m_rt || !m_factory || vertices.size() < 6) return;
    
    // 좌표 쌍을 위해 배열 크기가 짝수여야 함
    if (vertices.size() % 2 != 0) return;
    
    ComPtr<ID2D1PathGeometry> path;
    m_factory->CreatePathGeometry(&path);
    ComPtr<ID2D1GeometrySink> sink;
    path->Open(&sink);
    sink->BeginFigure(D2D1::Point2F(vertices[1], vertices[2]), D2D1_FIGURE_BEGIN_HOLLOW);
    for (size_t i = 3; i < vertices.size(); i += 2) {
        sink->AddLine(D2D1::Point2F(vertices[i], vertices[i + 1]));
    }
    sink->EndFigure(D2D1_FIGURE_END_CLOSED);
    sink->Close();
    m_rt->FillGeometry(path.Get(), m_brush.Get());
}

// --- 폰트/텍스트 그리기 ---
void DrawContext::text(int fontId, std::string str, float x, float y) {
    EnsureDrawSession();
    if (!m_rt || !m_brush) return;

    IDWriteTextFormat* pFormat = ResourceHub::Instance().GetFont(fontId);
    if (!pFormat) return;

    std::wstring wstr = ToWString(str);
    D2D1_RECT_F layoutRect = D2D1::RectF(x, y, x + 10000.0f, y + 10000.0f);

    m_rt->DrawText(
        wstr.c_str(),
        static_cast<UINT32>(wstr.length()),
        pFormat,
        layoutRect,
        m_brush.Get());
}

// --- 이미지/비트맵 그리기 ---
void DrawContext::image(int id, float dx, float dy,
    sol::optional<float> dw, sol::optional<float> dh,
    sol::optional<float> sx, sol::optional<float> sy,
    sol::optional<float> sw, sol::optional<float> sh,
    sol::optional<float> alpha)
{
    EnsureDrawSession();
    ID2D1Bitmap* bmp = ResourceHub::Instance().GetBitmap(id);
    if (!bmp || !m_rt) return;
    auto size = bmp->GetSize();
    float sX = sx.value_or(0);
    float sY = sy.value_or(0);
    float sW = sw.value_or(size.width - sX);
    float sH = sh.value_or(size.height - sY);
    float finalA = alpha.value_or(m_globalAlpha);
    m_rt->DrawBitmap(bmp,
        D2D1::RectF(dx, dy, dx + dw.value_or(sW), dy + dh.value_or(sH)),
        finalA,
        D2D1_BITMAP_INTERPOLATION_MODE_NEAREST_NEIGHBOR,
        D2D1::RectF(sX, sY, sX + sW, sY + sH));
}
// --- 상태 관리 ---
void DrawContext::push() {
    EnsureDrawSession();
    if (!m_rt) return;
    D2D1_MATRIX_3X2_F current;
    m_rt->GetTransform(&current);
    m_stateStack.push_back({ current, m_clipCount, m_strokeWidth });
}

void DrawContext::pop() {
    if (!m_rt || m_stateStack.empty()) return;
    StateLayer last = m_stateStack.back();
    m_stateStack.pop_back();

    while (m_clipCount > last.clipDepth) {
        m_rt->PopAxisAlignedClip();
        m_clipCount--;
    }

    m_strokeWidth = last.strokeWidth;
    m_rt->SetTransform(last.matrix);
}

void DrawContext::translate(float x, float y) {
    EnsureDrawSession();
    if (!m_rt) return;
    D2D1_MATRIX_3X2_F m;
    m_rt->GetTransform(&m);
    m_rt->SetTransform(m * D2D1::Matrix3x2F::Translation(x, y));
}

void DrawContext::scale(float sx, float sy, sol::optional<float> ox, sol::optional<float> oy) {
    EnsureDrawSession();
    if (!m_rt) return;
    D2D1_MATRIX_3X2_F m;
    m_rt->GetTransform(&m);
    m_rt->SetTransform(m * D2D1::Matrix3x2F::Scale(sx, sy, D2D1::Point2F(ox.value_or(0), oy.value_or(0))));
}

void DrawContext::clip(float x, float y, float w, float h) {
    EnsureDrawSession();
    if (!m_rt) return;
    m_rt->PushAxisAlignedClip(D2D1::RectF(x, y, x + w, y + h), D2D1_ANTIALIAS_MODE_ALIASED);
    m_clipCount++;
}

// --- 속성 ---
void DrawContext::updateBrush() {
    if (!m_rt) return;
    if (!m_brush) m_rt->CreateSolidColorBrush(m_color, &m_brush);
    else m_brush->SetColor(m_color);
    m_brush->SetOpacity(m_globalAlpha);
}
void DrawContext::color(sol::object arg1, sol::optional<float> arg2, sol::optional<float> arg3, sol::optional<float> arg4) {
    auto smartColor = [](sol::object arg1, sol::optional<float> arg2, sol::optional<float> arg3, sol::optional<float> arg4) {
        // 1. 문자열 처리 (예: "#FF0000FF")
        if (arg1.is<std::string>()) {
            std::string s = arg1.as<std::string>();

            // '#' 제거
            if (!s.empty() && s[0] == '#') s = s.substr(1);

            // 6자리 또는 8자리인지 확인
            if (s.length() == 6 || s.length() == 8) {
                uint32_t hex = std::stoul(s, nullptr, 16);
                float r, g, b, a;

                if (s.length() == 8) {
                    // RRGGBBAA
                    r = ((hex >> 24) & 0xFF) / 255.f;
                    g = ((hex >> 16) & 0xFF) / 255.f;
                    b = ((hex >> 8) & 0xFF) / 255.f;
                    a = (hex & 0xFF) / 255.f;
                }
                else {
                    // RRGGBB + optional alpha
                    r = ((hex >> 16) & 0xFF) / 255.f;
                    g = ((hex >> 8) & 0xFF) / 255.f;
                    b = (hex & 0xFF) / 255.f;
                    a = arg2.value_or(1.f);
                    if (a > 1.f) a /= 255.f;
                }
                return D2D1::ColorF(r, g, b, a);
            }

            // 문자열 형식이 이상하면 Black 반환
            return D2D1::ColorF(D2D1::ColorF::Black);
        }

        // 2. 숫자 처리
        if (arg1.is<double>() || arg1.is<float>() || arg1.is<int>() || arg1.is<unsigned int>()) {
            double rNum = arg1.as<double>();
            float r = static_cast<float>(rNum);

            // 인자가 3개 이상 들어온 경우 (r, g, b, [a])
            if (arg2.has_value() && arg3.has_value()) {
                float g = arg2.value();
                float b = arg3.value();

                bool isIntRange = (r > 1.0f || g > 1.0f || b > 1.0f);
                float rf = isIntRange ? r / 255.0f : r;
                float gf = isIntRange ? g / 255.0f : g;
                float bf = isIntRange ? b / 255.0f : b;

                float av = arg4.value_or(isIntRange ? 255.0f : 1.0f);
                float af = (av > 1.0f) ? av / 255.0f : av;

                return D2D1::ColorF(rf, gf, bf, af);
            }

            // 인자가 1개 또는 2개인 경우 (Hex 처리)
            // float 변환 시 하위 비트가 깨질 수 있으므로 double 기반으로 처리
            uint64_t hex64 = static_cast<uint64_t>(rNum);
            uint32_t hex = static_cast<uint32_t>(hex64 & 0xFFFFFFFFull);

            if (hex > 0xFFFFFF) {
                // 8자리 Hex (RRGGBBAA)로 간주
                float rv = ((hex >> 24) & 0xFF) / 255.0f;
                float gv = ((hex >> 16) & 0xFF) / 255.0f;
                float bv = ((hex >> 8) & 0xFF) / 255.0f;
                float av = (hex & 0xFF) / 255.0f;
                return D2D1::ColorF(rv, gv, bv, av);
            }
            else {
                // 6자리 Hex (RRGGBB)로 간주 + 별도 Alpha 인자 확인
                float av = arg2.value_or(1.0f);
                float finalA = (av > 1.0f) ? av / 255.0f : av;
                return D2D1::ColorF(hex, finalA);
            }
        }
        return D2D1::ColorF(D2D1::ColorF::Black);
        };
    m_color = smartColor(arg1, arg2, arg3, arg4);
    updateBrush();
}
void DrawContext::setStrokeWidth(float width) { m_strokeWidth = width; }
void DrawContext::setGlobalAlpha(float alpha) { m_globalAlpha = alpha; updateBrush(); }
void DrawContext::setShader(sol::optional<int> shaderId) {
    m_shaderId = shaderId.value_or(-1);
}

void DrawContext::ApplyShaderToOffscreen(DrawContext* source, float dx, float dy, float dw, float dh,
    float sx, float sy, float sw, float sh, float alpha)
{
    if (!source || !source->m_rt || !m_rt || source->m_shaderId < 0) {
        return;
    }

    ID3D11Device* d3d = ResourceHub::Instance().GetD3DDevice();
    ID3D11DeviceContext* d3dCtx = ResourceHub::Instance().GetD3DContext();
    ID3D11PixelShader* ps = ResourceHub::Instance().GetPixelShader(source->m_shaderId);
    if (!d3d || !d3dCtx || !ps) {
        if (source->m_targetBitmap) {
            ComPtr<ID2D1DeviceContext> dstDc;
            if (SUCCEEDED(m_rt.As(&dstDc)) && dstDc) {
                dstDc->DrawImage(
                    source->m_targetBitmap.Get(),
                    D2D1::Point2F(dx, dy),
                    D2D1::RectF(sx, sy, sx + sw, sy + sh),
                    D2D1_INTERPOLATION_MODE_NEAREST_NEIGHBOR,
                    D2D1_COMPOSITE_MODE_SOURCE_OVER);
            }
            else {
                m_rt->DrawBitmap(source->m_targetBitmap.Get(),
                    D2D1::RectF(dx, dy, dx + dw, dy + dh),
                    alpha,
                    D2D1_BITMAP_INTERPOLATION_MODE_NEAREST_NEIGHBOR,
                    D2D1::RectF(sx, sy, sx + sw, sy + sh));
            }
            return;
        }

        ComPtr<ID2D1BitmapRenderTarget> bitmapRT;
        if (SUCCEEDED(source->m_rt.As(&bitmapRT))) {
            ComPtr<ID2D1Bitmap> bmp;
            if (SUCCEEDED(bitmapRT->GetBitmap(&bmp))) {
                m_rt->DrawBitmap(bmp.Get(),
                    D2D1::RectF(dx, dy, dx + dw, dy + dh),
                    alpha,
                    D2D1_BITMAP_INTERPOLATION_MODE_NEAREST_NEIGHBOR,
                    D2D1::RectF(sx, sy, sx + sw, sy + sh));
            }
        }
        return;
    }

    if (!source->m_targetBitmap) {
        ComPtr<ID2D1BitmapRenderTarget> bitmapRT;
        if (SUCCEEDED(source->m_rt.As(&bitmapRT))) {
            ComPtr<ID2D1Bitmap> bmp;
            if (SUCCEEDED(bitmapRT->GetBitmap(&bmp))) {
                m_rt->DrawBitmap(bmp.Get(),
                    D2D1::RectF(dx, dy, dx + dw, dy + dh),
                    alpha,
                    D2D1_BITMAP_INTERPOLATION_MODE_NEAREST_NEIGHBOR,
                    D2D1::RectF(sx, sy, sx + sw, sy + sh));
            }
        }
        return;
    }

    ComPtr<IDXGISurface> srcSurface;
    if (FAILED(source->m_targetBitmap->GetSurface(&srcSurface)) || !srcSurface) return;

    ComPtr<ID3D11Texture2D> srcTex;
    if (FAILED(srcSurface.As(&srcTex)) || !srcTex) return;

    D3D11_TEXTURE2D_DESC srcDesc = {};
    srcTex->GetDesc(&srcDesc);

    UINT srcX = (UINT)(std::max)(0.0f, sx);
    UINT srcY = (UINT)(std::max)(0.0f, sy);
    UINT srcW = (UINT)(std::max)(1.0f, sw);
    UINT srcH = (UINT)(std::max)(1.0f, sh);
    if (srcX + srcW > srcDesc.Width) srcW = (srcDesc.Width > srcX) ? (srcDesc.Width - srcX) : 1;
    if (srcY + srcH > srcDesc.Height) srcH = (srcDesc.Height > srcY) ? (srcDesc.Height - srcY) : 1;

    D3D11_TEXTURE2D_DESC croppedDesc = {};
    croppedDesc.Width = srcW;
    croppedDesc.Height = srcH;
    croppedDesc.MipLevels = 1;
    croppedDesc.ArraySize = 1;
    croppedDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    croppedDesc.SampleDesc.Count = 1;
    croppedDesc.Usage = D3D11_USAGE_DEFAULT;
    croppedDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

    ComPtr<ID3D11Texture2D> croppedTex;
    if (FAILED(d3d->CreateTexture2D(&croppedDesc, nullptr, &croppedTex)) || !croppedTex) return;

    D3D11_BOX srcBox = {};
    srcBox.left = srcX;
    srcBox.top = srcY;
    srcBox.front = 0;
    srcBox.right = srcX + srcW;
    srcBox.bottom = srcY + srcH;
    srcBox.back = 1;
    d3dCtx->CopySubresourceRegion(croppedTex.Get(), 0, 0, 0, 0, srcTex.Get(), 0, &srcBox);

    UINT outW = (UINT)(std::max)(1.0f, dw);
    UINT outH = (UINT)(std::max)(1.0f, dh);

    D3D11_TEXTURE2D_DESC outDesc = {};
    outDesc.Width = outW;
    outDesc.Height = outH;
    outDesc.MipLevels = 1;
    outDesc.ArraySize = 1;
    outDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    outDesc.SampleDesc.Count = 1;
    outDesc.Usage = D3D11_USAGE_DEFAULT;
    outDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;

    ComPtr<ID3D11Texture2D> outTex;
    if (FAILED(d3d->CreateTexture2D(&outDesc, nullptr, &outTex)) || !outTex) return;

    ComPtr<ID3D11RenderTargetView> outRtv;
    if (FAILED(d3d->CreateRenderTargetView(outTex.Get(), nullptr, &outRtv)) || !outRtv) return;

    ComPtr<ID3D11ShaderResourceView> srcSrv;
    if (FAILED(d3d->CreateShaderResourceView(croppedTex.Get(), nullptr, &srcSrv)) || !srcSrv) return;

    ComPtr<ID3D11VertexShader> vs;
    if (FAILED(GetOrCreateFullscreenVS(d3d, &vs))) return;

    ComPtr<ID3D11SamplerState> sampler;
    if (FAILED(GetOrCreateLinearSampler(d3d, &sampler))) return;

    ComPtr<ID3D11RasterizerState> rs;
    if (FAILED(GetOrCreateNoCullRasterizer(d3d, &rs))) return;

    D3D11_VIEWPORT vp = {};
    vp.Width = (FLOAT)outW;
    vp.Height = (FLOAT)outH;
    vp.MinDepth = 0.0f;
    vp.MaxDepth = 1.0f;

    d3dCtx->OMSetRenderTargets(1, outRtv.GetAddressOf(), nullptr);
    d3dCtx->RSSetState(rs.Get());
    d3dCtx->RSSetViewports(1, &vp);
    d3dCtx->IASetInputLayout(nullptr);
    d3dCtx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    d3dCtx->VSSetShader(vs.Get(), nullptr, 0);
    d3dCtx->PSSetShader(ps, nullptr, 0);
    d3dCtx->PSSetShaderResources(0, 1, srcSrv.GetAddressOf());
    d3dCtx->PSSetSamplers(0, 1, sampler.GetAddressOf());
    d3dCtx->Draw(3, 0);

    ID3D11ShaderResourceView* nullSrv = nullptr;
    d3dCtx->PSSetShaderResources(0, 1, &nullSrv);

    ComPtr<IDXGISurface> outSurface;
    if (FAILED(outTex.As(&outSurface)) || !outSurface) return;

    ComPtr<ID2D1DeviceContext> dc;
    if (FAILED(m_rt.As(&dc)) || !dc) return;

    D2D1_BITMAP_PROPERTIES1 outProps = D2D1::BitmapProperties1(
        D2D1_BITMAP_OPTIONS_NONE,
        D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED));

    ComPtr<ID2D1Bitmap1> outBitmap;
    if (FAILED(dc->CreateBitmapFromDxgiSurface(outSurface.Get(), &outProps, &outBitmap)) || !outBitmap) return;

    m_rt->DrawBitmap(outBitmap.Get(),
        D2D1::RectF(dx, dy, dx + dw, dy + dh),
        alpha,
        D2D1_BITMAP_INTERPOLATION_MODE_LINEAR,
        D2D1::RectF(0, 0, (float)outW, (float)outH));
}

// --- Offscreen 생성 ---
std::shared_ptr<DrawContext> DrawContext::createOffscreen(float w, float h) {
    ComPtr<ID2D1DeviceContext> dc;
    if (FAILED(m_rt.As(&dc)) || !dc) {
        return nullptr;
    }

    ComPtr<ID2D1Device> device;
    dc->GetDevice(&device);
    if (!device) {
        return nullptr;
    }

    ComPtr<ID2D1DeviceContext> offscreenDc;
    if (FAILED(device->CreateDeviceContext(D2D1_DEVICE_CONTEXT_OPTIONS_NONE, &offscreenDc)) || !offscreenDc) {
        return nullptr;
    }

    D2D1_BITMAP_PROPERTIES1 props = D2D1::BitmapProperties1(
        D2D1_BITMAP_OPTIONS_TARGET,
        D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED));

    UINT32 bw = (UINT32)(std::max)(1.0f, w);
    UINT32 bh = (UINT32)(std::max)(1.0f, h);
    ComPtr<ID2D1Bitmap1> target;
    if (FAILED(offscreenDc->CreateBitmap(D2D1::SizeU(bw, bh), nullptr, 0, &props, &target)) || !target) {
        return nullptr;
    }

    offscreenDc->SetTarget(target.Get());

    auto offscreenCtx = std::make_shared<DrawContext>(offscreenDc.Get(), m_factory.Get());
    offscreenCtx->m_targetBitmap = target;

    return offscreenCtx;
}

void DrawContext::batchBegin() {
    if (!isBatching) {
        m_rt->BeginDraw();
        isBatching = true;
    }
}

void DrawContext::batchEnd() {
    if (isBatching) {
        m_rt->EndDraw();
        isBatching = false;
    }
}

void DrawContext::draw(DrawContext* source, float x, float y,
    sol::optional<float> w, sol::optional<float> h,
    sol::optional<float> sx, sol::optional<float> sy,
    sol::optional<float> sw, sol::optional<float> sh, sol::optional<float> alpha)
{
    if (!source || !source->m_rt || !m_rt) {
        return;
    }

    if (source->isBatching) {
        source->batchEnd();
    }

    ComPtr<ID2D1DeviceContext> srcDc;
    ComPtr<ID2D1Image> prevSrcTarget;
    if (SUCCEEDED(source->m_rt.As(&srcDc)) && srcDc && source->m_targetBitmap) {
        srcDc->GetTarget(&prevSrcTarget);
        srcDc->SetTarget(nullptr);
    }

    if (source->m_targetBitmap) {
        auto size = source->m_targetBitmap->GetSize();

        float sX = sx.value_or(0);
        float sY = sy.value_or(0);
        float sW = sw.value_or(size.width - sX);
        float sH = sh.value_or(size.height - sY);
        float dW = w.value_or(sW);
        float dH = h.value_or(sH);
        float a = alpha.value_or(source->m_globalAlpha);

        if (source->m_shaderId >= 0) {
            ApplyShaderToOffscreen(source, x, y, dW, dH, sX, sY, sW, sH, a);
        }
        else {
            ComPtr<ID2D1DeviceContext> dstDc;
            if (SUCCEEDED(m_rt.As(&dstDc)) && dstDc) {
                dstDc->DrawImage(
                    source->m_targetBitmap.Get(),
                    D2D1::Point2F(x, y),
                    D2D1::RectF(sX, sY, sX + sW, sY + sH),
                    D2D1_INTERPOLATION_MODE_NEAREST_NEIGHBOR,
                    D2D1_COMPOSITE_MODE_SOURCE_OVER);
            }
            else {
                m_rt->DrawBitmap(source->m_targetBitmap.Get(),
                    D2D1::RectF(x, y, x + dW, y + dH),
                    a,
                    D2D1_BITMAP_INTERPOLATION_MODE_NEAREST_NEIGHBOR,
                    D2D1::RectF(sX, sY, sX + sW, sY + sH));
            }
        }

        if (srcDc && prevSrcTarget) {
            srcDc->SetTarget(prevSrcTarget.Get());
        }
        return;
    }

    ComPtr<ID2D1BitmapRenderTarget> bitmapRT;
    if (SUCCEEDED(source->m_rt.As(&bitmapRT)) && bitmapRT) {
        ComPtr<ID2D1Bitmap> bmp;
        if (SUCCEEDED(bitmapRT->GetBitmap(&bmp)) && bmp) {
            auto size = bmp->GetSize();

            float sX = sx.value_or(0);
            float sY = sy.value_or(0);
            float sW = sw.value_or(size.width - sX);
            float sH = sh.value_or(size.height - sY);
            float dW = w.value_or(sW);
            float dH = h.value_or(sH);
            float a = alpha.value_or(source->m_globalAlpha);

            m_rt->DrawBitmap(bmp.Get(),
                D2D1::RectF(x, y, x + dW, y + dH),
                a,
                D2D1_BITMAP_INTERPOLATION_MODE_NEAREST_NEIGHBOR,
                D2D1::RectF(sX, sY, sX + sW, sY + sH));
        }
    }
}

void DrawContext::BindGlobal(LuaBindContext& ctx) {
    LuaNamespaceBinder binder(ctx);

    // --- 기본 도형 ---


    binder.func("rect", [this](float x, float y, float w, float h, sol::optional<bool> f) { 
        rect(x, y, w, h, f); 
    })
    .names({"x", "y", "w", "h", "fill"})
    .desc("Draw a rectangle at (x, y) with size (w, h). Set fill=true for filled");

    binder.func("circle", [this](float x, float y, float r, sol::optional<bool> f) { 
        circle(x, y, r, f); 
    })
    .names({"x", "y", "r", "fill"})
    .desc("Draw a circle at (x, y) with radius r. Set fill=true for filled");

    // --- 폴리곤 / 폴리라인 (테이블 인자) ---


    binder.func("polyline", [this](sol::table t, sol::optional<bool> c) { 
        polyline(t, c); 
    })
    .names({"vertices", "closed"})
    .desc("Draw a polyline through vertices {x1,y1, x2,y2, ...}. Set closed=true to close the path");

    binder.func("polygon", [this](sol::table t) { 
        polygon(t); 
    })
    .names({"vertices"})
    .desc("Draw a filled polygon with vertices {x1,y1, x2,y2, ...}");

    // --- 텍스트 / 이미지 ---


    binder.func("text", [this](int fontId, std::string s, float x, float y) { 
        text(fontId, s, x, y); 
    })
    .names({"fontId", "str", "x", "y"})
    .desc("Draw text string at (x, y) using the specified font");

    binder.func("image", [this](int id, float dx, float dy,
        sol::optional<float> dw, sol::optional<float> dh,
        sol::optional<float> sx, sol::optional<float> sy,
        sol::optional<float> sw, sol::optional<float> sh,
        sol::optional<float> a) {
            image(id, dx, dy, dw, dh, sx, sy, sw, sh, a);
    })
    .names({"id", "dx", "dy", "dw", "dh", "sx", "sy", "sw", "sh", "alpha"})
    .desc("Draw image at (dx, dy) with optional size, source rect, and alpha");

    // --- 오프스크린 & 드로우 ---


    binder.func("offscreen", [this](float w, float h) { 
        return createOffscreen(w, h); 
    })
    .names({"w", "h"})
    .returns("any")
    .desc("Create an offscreen canvas with size (w, h)");

    binder.func("draw", [this](DrawContext* src, float x, float y,
        sol::optional<float> w, sol::optional<float> h,
        sol::optional<float> sx, sol::optional<float> sy,
        sol::optional<float> sw, sol::optional<float> sh,
        sol::optional<float> a) {
            draw(src, x, y, w, h, sx, sy, sw, sh, a);
    })
    .names({"source", "x", "y", "w", "h", "sx", "sy", "sw", "sh", "alpha"})
    .desc("Draw an offscreen canvas at (x, y) with optional size, source rect, and alpha");

    // --- 상태 관리 및 속성 ---


    binder.func("color", [this](sol::object arg1, sol::optional<float> arg2, sol::optional<float> arg3, sol::optional<float> arg4) {
        color(arg1, arg2, arg3, arg4);
    })
    .names({"arg1", "arg2", "arg3", "arg4"})
    .desc("Set drawing color. Use color(r,g,b,a) or color(0xRRGGBB) or color(0xRRGGBBAA)");

    binder.func("lineWidth", [this](float w) { 
        setStrokeWidth(w); 
    })
    .names({"w"})
    .desc("Set line width for stroke drawing");

    binder.func("push", [this]() { 
        push(); 
    })
    .desc("Push current transform and clip state onto stack");

    binder.func("pop", [this]() { 
        pop(); 
    })
    .desc("Pop transform and clip state from stack");

    binder.func("translate", [this](float x, float y) { 
        translate(x, y); 
    })
    .names({"x", "y"})
    .desc("Translate (move) the coordinate system by (x, y)");

    binder.func("scale", [this](float sx, float sy, sol::optional<float> ox, sol::optional<float> oy) { 
        scale(sx, sy, ox, oy); 
    })
    .names({"sx", "sy", "ox", "oy"})
    .desc("Scale the coordinate system by (sx, sy) around origin (ox, oy)");

    binder.func("clip", [this](float x, float y, float w, float h) { 
        clip(x, y, w, h); 
    })
    .names({"x", "y", "w", "h"})
    .desc("Set clipping rectangle to (x, y, w, h)");
}

void DrawContext::BindClass(LuaBindContext& ctx) {
    LuaClassBinder::Builder<DrawContext> binder(ctx, "DrawContext");
    
    // --- 기본 도형 ---


    binder.method("rect", [](DrawContext& self, float x, float y, float w, float h, sol::optional<bool> f) {
        self.rect(x, y, w, h, f);
    })
    .names({"x", "y", "w", "h", "fill"})
    .desc("Draw a rectangle at (x, y) with size (w, h). Set fill=true for filled");

    binder.method("circle", [](DrawContext& self, float x, float y, float r, sol::optional<bool> f) {
        self.circle(x, y, r, f);
    })
    .names({"x", "y", "r", "fill"})
    .desc("Draw a circle at (x, y) with radius r. Set fill=true for filled");

    binder.method("polyline", [](DrawContext& self, sol::table t, sol::optional<bool> c) {
        self.polyline(t, c);
    })
    .names({"vertices", "closed"})
    .desc("Draw a polyline through vertices {x1,y1, x2,y2, ...}. Set closed=true to close the path");

    binder.method("polygon", [](DrawContext& self, sol::table t) {
        self.polygon(t);
    })
    .names({"vertices"})
    .desc("Draw a filled polygon with vertices {x1,y1, x2,y2, ...}");

    // --- 텍스트 / 이미지 ---


    binder.method("text", [](DrawContext& self, int fontId, std::string s, float x, float y) {
        self.text(fontId, s, x, y);
    })
    .names({"fontId", "str", "x", "y"})
    .desc("Draw text string at (x, y) using the specified font");

    binder.method("image", [](DrawContext& self, int id, float dx, float dy,
        sol::optional<float> dw, sol::optional<float> dh,
        sol::optional<float> sx, sol::optional<float> sy,
        sol::optional<float> sw, sol::optional<float> sh,
        sol::optional<float> a) {
        self.image(id, dx, dy, dw, dh, sx, sy, sw, sh, a);
    })
    .names({"id", "dx", "dy", "dw", "dh", "sx", "sy", "sw", "sh", "alpha"})
    .desc("Draw image at (dx, dy) with optional size, source rect, and alpha");

    // --- 오프스크린 & 드로우 ---


    binder.method("offscreen", [](DrawContext& self, float w, float h) {
        return self.createOffscreen(w, h);
    })
    .names({"w", "h"})
    .desc("Create an offscreen canvas with size (w, h)");

    binder.method("draw", [](DrawContext& self, DrawContext* src, float x, float y,
        sol::optional<float> w, sol::optional<float> h,
        sol::optional<float> sx, sol::optional<float> sy,
        sol::optional<float> sw, sol::optional<float> sh,
        sol::optional<float> a) {
        self.draw(src, x, y, w, h, sx, sy, sw, sh, a);
    })
    .names({"source", "x", "y", "w", "h", "sx", "sy", "sw", "sh", "alpha"})
    .desc("Draw an offscreen canvas at (x, y) with optional size, source rect, and alpha");

    // --- 상태 관리 및 속성 ---


    binder.method("color", [](DrawContext& self, sol::object arg1, sol::optional<float> arg2, sol::optional<float> arg3, sol::optional<float> arg4) {
        self.color(arg1, arg2, arg3, arg4);
    })
    .names({"arg1", "arg2", "arg3", "arg4"})
    .desc("Set drawing color. Use color(r,g,b,a) or color(0xRRGGBB) or color(0xRRGGBBAA)");

    binder.method("lineWidth", [](DrawContext& self, float w) {
        self.setStrokeWidth(w);
    })
    .names({"w"})
    .desc("Set line width for stroke drawing");

    binder.method("push", [](DrawContext& self) {
        self.push();
    })
    .desc("Push current transform and clip state onto stack");

    binder.method("pop", [](DrawContext& self) {
        self.pop();
    })
    .desc("Pop transform and clip state from stack");

    binder.method("translate", [](DrawContext& self, float x, float y) {
        self.translate(x, y);
    })
    .names({"x", "y"})
    .desc("Translate (move) the coordinate system by (x, y)");

    binder.method("scale", [](DrawContext& self, float sx, float sy, sol::optional<float> ox, sol::optional<float> oy) {
        self.scale(sx, sy, ox, oy);
    })
    .names({"sx", "sy", "ox", "oy"})
    .desc("Scale the coordinate system by (sx, sy) around origin (ox, oy)");

    binder.method("clip", [](DrawContext& self, float x, float y, float w, float h) {
        self.clip(x, y, w, h);
    })
    .names({"x", "y", "w", "h"})
    .desc("Set clipping rectangle to (x, y, w, h)");

    // --- 셰이더 & 배치 ---
    binder.method("setShader", [](DrawContext& self, sol::optional<int> shaderId) {
        self.setShader(shaderId);
    })
    .names({"shaderId"})
    .desc("Set shader effect for this offscreen canvas. Pass nil to disable shader");

    binder.method("batchBegin", [](DrawContext& self) {
        self.batchBegin();
    })
    .desc("Begin draw batching (calls BeginDraw on render target)");

    binder.method("batchEnd", [](DrawContext& self) {
        self.batchEnd();
    })
    .desc("End draw batching (calls EndDraw on render target)");
}