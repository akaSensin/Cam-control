/*
	Cam Control - made by sensin
	DirectWrite overlay - see dxtext.h.
*/

#include "dxtext.h"
#include "lang.h"
#include "log.h"

#include "main.h"

#include <windows.h>
#include <d3d11.h>
#include <dxgi.h>
#include <d2d1.h>
#include <d2d1helper.h>
#include <dwrite.h>
#include <wincodec.h>
#include <d3dcompiler.h>

#include <cstring>
#include <map>
#include <mutex>
#include <set>
#include <vector>
#include <cmath>

// Resolved from the swapchain each frame: the render thread cannot call natives,
// and the back buffer is the authority on the real size anyway.
static int g_screenWidth = 1920;
static int g_screenHeight = 1080;

namespace
{
	// ----------------------------------------------------------------------
	// Shaders. One quad per text run, positioned from a constant buffer. The
	// vertex is generated from SV_VertexID so no vertex buffer or input layout
	// is needed at all.
	// ----------------------------------------------------------------------
	const char *kShaderSource =
		"cbuffer Params : register(b0)                                  \n"
		"{                                                              \n"
		"    float4 gRect;   // xy = top-left in NDC, zw = size in NDC   \n"
		"    float4 gColor;                                              \n"
		"};                                                             \n"
		"static const float2 kUV[6] =                                   \n"
		"{                                                              \n"
		"    float2(0,0), float2(1,0), float2(0,1),                     \n"
		"    float2(0,1), float2(1,0), float2(1,1)                      \n"
		"};                                                             \n"
		"struct VSOut { float4 pos : SV_POSITION; float2 uv : TEXCOORD0; }; \n"
		"VSOut VSMain(uint id : SV_VertexID)                            \n"
		"{                                                              \n"
		"    VSOut o;                                                   \n"
		"    o.uv = kUV[id];                                            \n"
		"    o.pos = float4(gRect.xy + o.uv * gRect.zw, 0.0, 1.0);      \n"
		"    return o;                                                  \n"
		"}                                                              \n"
		"Texture2D    gTex  : register(t0);                             \n"
		"SamplerState gSamp : register(s0);                             \n"
		"float4 PSMain(VSOut i) : SV_TARGET                             \n"
		"{                                                              \n"
		"    return gTex.Sample(gSamp, i.uv) * gColor;                  \n"
		"}                                                              \n";

	// GTA text scale -> pixel height. Tuned against the native path so a
	// translated row keeps roughly the size it had before.
	const float SCALE_TO_PX = 0.062f;

	// Extra room around the glyphs. Arabic needs the vertical room: vowels and
	// descenders sit well above and below the nominal line.
	const float PAD_X = 3.0f;
	const float PAD_Y_FRAC = 0.42f;

	const size_t MAX_CACHE_ENTRIES = 192;

	struct Queued
	{
		std::string text;
		float x;
		float y;
		float scale;
		int r, g, b, a;
		int align;
	};

	std::mutex g_queueMutex;
	std::mutex g_renderMutex;
	std::vector<Queued> g_next;     // filled by the script thread
	std::vector<Queued> g_current;  // drained by the render thread

	// Frame handshake. The script thread bumps g_frameId once per tick; the
	// render thread only takes a new queue when the id changed. Without this,
	// a second Present between two ticks found the queue already drained and
	// drew nothing, so the overlay flickered (or looked missing) whenever the
	// game presented faster than the script ticked.
	unsigned g_frameId = 0;
	unsigned g_drawnFrameId = 0;

	bool g_registered = false;
	bool g_failed = false;
	bool g_reportedFailure = false;

	// What the overlay last drew, so the log reports each language that reaches
	// the draw stage (see the line in OnPresent).
	const wchar_t *g_logFont = nullptr;
	bool g_logRtl = false;

	// ---- D3D --------------------------------------------------------------
	ID3D11Device        *g_device = nullptr;
	ID3D11DeviceContext *g_context = nullptr;
	ID3D11VertexShader  *g_vs = nullptr;
	ID3D11PixelShader   *g_ps = nullptr;
	ID3D11Buffer        *g_cbuffer = nullptr;
	ID3D11SamplerState  *g_sampler = nullptr;
	ID3D11BlendState    *g_blend = nullptr;
	ID3D11RasterizerState *g_raster = nullptr;

	// ---- text rasterisation ----------------------------------------------
	ID2D1Factory          *g_d2d = nullptr;
	IDWriteFactory        *g_dwrite = nullptr;
	IWICImagingFactory    *g_wic = nullptr;

	struct Cached
	{
		ID3D11ShaderResourceView *srv;
		int w;
		int h;
		float padX;
		float padY;
	};

	struct CacheKey
	{
		std::wstring text;
		std::wstring family;
		int px;
		unsigned int colour;
		bool rtl;

		bool operator<(const CacheKey &o) const
		{
			if (rtl != o.rtl) return rtl < o.rtl;
			if (px != o.px) return px < o.px;
			if (colour != o.colour) return colour < o.colour;
			if (family != o.family) return family < o.family;
			return text < o.text;
		}
	};

	std::map<CacheKey, Cached> g_cache;

	void ReleaseCache()
	{
		for (std::map<CacheKey, Cached>::iterator it = g_cache.begin(); it != g_cache.end(); ++it)
		{
			if (it->second.srv)
				it->second.srv->Release();
		}
		g_cache.clear();
	}

	void Fail(const char *what, HRESULT hr)
	{
		g_failed = true;
		if (!g_reportedFailure)
		{
			g_reportedFailure = true;
			Log::Line("dxtext: %s failed (0x%08X) - RTL/CJK text will fall back to the "
				"game renderer, which cannot show it", what, (unsigned)hr);
		}
	}

	// Reports a problem once per tag. Without this a failing string just goes
	// missing with nothing in the log to explain it.
	void WarnOnce(const char *tag, HRESULT hr, const char *detail)
	{
		static std::set<std::string> seen;
		if (!seen.insert(tag).second)
			return;

		Log::Line("dxtext: %s failed (0x%08X)%s%s", tag, (unsigned)hr,
			detail ? " - " : "", detail ? detail : "");
	}

	// ----------------------------------------------------------------------
	// Initialisation
	// ----------------------------------------------------------------------

	bool InitD3D(IDXGISwapChain *swap)
	{
		if (g_device)
			return true;

		HRESULT hr = swap->GetDevice(__uuidof(ID3D11Device), (void **)&g_device);
		if (FAILED(hr) || !g_device)
		{
			Fail("GetDevice", hr);
			return false;
		}

		g_device->GetImmediateContext(&g_context);
		if (!g_context)
		{
			g_device->Release();
			g_device = nullptr;
			Fail("GetImmediateContext", E_FAIL);
			return false;
		}

		ID3DBlob *blob = nullptr;
		ID3DBlob *errorBlob = nullptr;
		const size_t srcLen = strlen(kShaderSource);

		hr = D3DCompile(kShaderSource, srcLen, nullptr, nullptr, nullptr,
			"VSMain", "vs_4_0", 0, 0, &blob, &errorBlob);
		if (FAILED(hr))
		{
			if (errorBlob)
			{
				Log::Line("dxtext: vertex shader: %s", (const char *)errorBlob->GetBufferPointer());
				errorBlob->Release();
			}
			Fail("D3DCompile(VS)", hr);
			return false;
		}

		hr = g_device->CreateVertexShader(blob->GetBufferPointer(), blob->GetBufferSize(), nullptr, &g_vs);
		blob->Release();
		blob = nullptr;
		if (FAILED(hr))
		{
			Fail("CreateVertexShader", hr);
			return false;
		}

		hr = D3DCompile(kShaderSource, srcLen, nullptr, nullptr, nullptr,
			"PSMain", "ps_4_0", 0, 0, &blob, &errorBlob);
		if (FAILED(hr))
		{
			if (errorBlob)
			{
				Log::Line("dxtext: pixel shader: %s", (const char *)errorBlob->GetBufferPointer());
				errorBlob->Release();
			}
			Fail("D3DCompile(PS)", hr);
			return false;
		}

		hr = g_device->CreatePixelShader(blob->GetBufferPointer(), blob->GetBufferSize(), nullptr, &g_ps);
		blob->Release();
		if (FAILED(hr))
		{
			Fail("CreatePixelShader", hr);
			return false;
		}

		D3D11_BUFFER_DESC cb;
		ZeroMemory(&cb, sizeof(cb));
		cb.ByteWidth = 32; // two float4
		cb.Usage = D3D11_USAGE_DYNAMIC;
		cb.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
		cb.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
		hr = g_device->CreateBuffer(&cb, nullptr, &g_cbuffer);
		if (FAILED(hr))
		{
			Fail("CreateBuffer(CB)", hr);
			return false;
		}

		D3D11_SAMPLER_DESC sd;
		ZeroMemory(&sd, sizeof(sd));
		sd.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
		sd.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
		sd.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
		sd.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
		sd.MaxLOD = D3D11_FLOAT32_MAX;
		hr = g_device->CreateSamplerState(&sd, &g_sampler);
		if (FAILED(hr))
		{
			Fail("CreateSamplerState", hr);
			return false;
		}

		// Premultiplied alpha, matching what Direct2D writes into the bitmap.
		D3D11_BLEND_DESC bd;
		ZeroMemory(&bd, sizeof(bd));
		bd.RenderTarget[0].BlendEnable = TRUE;
		bd.RenderTarget[0].SrcBlend = D3D11_BLEND_ONE;
		bd.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
		bd.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
		bd.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
		bd.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
		bd.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
		bd.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
		hr = g_device->CreateBlendState(&bd, &g_blend);
		if (FAILED(hr))
		{
			Fail("CreateBlendState", hr);
			return false;
		}

		// Culling and scissor under our own control. Whatever state the game
		// left bound could cull the quads away (the vertical flip reverses their
		// winding) or clip them with a stale scissor rect - and either one looks
		// exactly like "the overlay draws nothing".
		D3D11_RASTERIZER_DESC rd;
		ZeroMemory(&rd, sizeof(rd));
		rd.FillMode = D3D11_FILL_SOLID;
		rd.CullMode = D3D11_CULL_NONE;
		rd.DepthClipEnable = TRUE;
		hr = g_device->CreateRasterizerState(&rd, &g_raster);
		if (FAILED(hr))
		{
			Fail("CreateRasterizerState", hr);
			return false;
		}

		Log::Line("dxtext: D3D11 pipeline ready");
		return true;
	}

	bool InitText(bool rtl)
	{
		if (!g_d2d)
		{
			HRESULT hr = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &g_d2d);
			if (FAILED(hr) || !g_d2d)
			{
				Fail("D2D1CreateFactory", hr);
				return false;
			}
		}

		if (!g_dwrite)
		{
			HRESULT hr = DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory),
				reinterpret_cast<IUnknown **>(&g_dwrite));
			if (FAILED(hr) || !g_dwrite)
			{
				Fail("DWriteCreateFactory", hr);
				return false;
			}
		}

		if (!g_wic)
		{
			// CoCreateInstance needs COM up on THIS thread. GTA never initialises
			// it on the render thread, so asking for WIC there fails with
			// CO_E_NOTINITIALIZED and the whole overlay used to die quietly.
			// RPC_E_CHANGED_MODE just means it is already up in another mode.
			const HRESULT comHr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
			if (FAILED(comHr) && comHr != RPC_E_CHANGED_MODE)
				Log::Line("dxtext: CoInitializeEx on the render thread returned 0x%08X", (unsigned)comHr);

			HRESULT hr = CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
				__uuidof(IWICImagingFactory), reinterpret_cast<void **>(&g_wic));
			if (FAILED(hr) || !g_wic)
			{
				Fail("CoCreateInstance(WIC)", hr);
				return false;
			}
		}

		(void)rtl;
		return true;
	}

	// ----------------------------------------------------------------------
	// Rasterisation
	// ----------------------------------------------------------------------

	// Lays out and rasterises one string. Returns the entry to draw, or nullptr.
	const Cached *GetOrCreate(const Queued &q, const wchar_t *family, bool rtl, float padYFrac)
	{
		CacheKey key;
		key.text = Lang::Widen(q.text.c_str());
		key.family = family ? family : L"";
		key.colour = ((unsigned)q.r << 24) | ((unsigned)q.g << 16) | ((unsigned)q.b << 8) | (unsigned)q.a;
		key.rtl = rtl;

		if (key.text.empty())
			return nullptr;

		// The pixel size has to be part of the key, so it is resolved first.
		const int px = (int)(q.scale * SCALE_TO_PX * (float)g_screenHeight + 0.5f);
		key.px = px < 6 ? 6 : px;

		std::map<CacheKey, Cached>::iterator found = g_cache.find(key);
		if (found != g_cache.end())
			return &found->second;

		if (g_cache.size() >= MAX_CACHE_ENTRIES)
			ReleaseCache();

		IDWriteTextFormat *format = nullptr;
		HRESULT hr = g_dwrite->CreateTextFormat(family, nullptr,
			DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL,
			(float)key.px, L"", &format);
		if (FAILED(hr))
		{
			char detail[128];
			_snprintf_s(detail, sizeof(detail), _TRUNCATE, "font family '%ls' not available?",
				family ? family : L"?");
			WarnOnce("CreateTextFormat", hr, detail);
			return nullptr;
		}

		format->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
		format->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_NEAR);
		// One line per run. Wrapping could re-anchor the text inside the box
		// once the box is tightened to the text width below.
		format->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);

		IDWriteTextLayout *layout = nullptr;
		hr = g_dwrite->CreateTextLayout(key.text.c_str(), (UINT32)key.text.size(), format,
			(float)g_screenWidth, (float)g_screenHeight, &layout);
		format->Release();
		if (FAILED(hr))
		{
			WarnOnce("CreateTextLayout", hr, nullptr);
			return nullptr;
		}

		if (rtl)
			layout->SetReadingDirection(DWRITE_READING_DIRECTION_RIGHT_TO_LEFT);

		DWRITE_TEXT_METRICS metrics;
		if (FAILED(layout->GetMetrics(&metrics)))
		{
			layout->Release();
			return nullptr;
		}

		// Tighten the layout box to the width the text actually needs.
		//
		// With LEADING alignment the text is anchored to the box's LEADING edge,
		// and for a right-to-left reading direction that edge is on the RIGHT.
		// The box above is screen-wide, so every Arabic run was laid out at
		// x ~= screen width - far outside the small bitmap that gets rasterised -
		// and the overlay drew an empty texture. Arabic therefore looked like it
		// simply did not render, while the left-to-right languages were fine.
		// A box exactly the size of the text puts the glyphs back at x = 0 for
		// both directions.
		const float textW = ceilf(metrics.widthIncludingTrailingWhitespace) + 1.0f;
		if (textW > 0.0f)
		{
			layout->SetMaxWidth(textW);
			if (FAILED(layout->GetMetrics(&metrics)))
			{
				layout->Release();
				return nullptr;
			}
		}

		const float padX = PAD_X;
		const float padY = (float)key.px * padYFrac;

		const int w = (int)ceilf(metrics.widthIncludingTrailingWhitespace + padX * 2.0f);
		const int h = (int)ceilf(metrics.height + padY * 2.0f);

		if (w <= 0 || h <= 0 || w > 4096 || h > 1024)
		{
			WarnOnce("layout size", E_FAIL, "string too large to rasterise");
			layout->Release();
			return nullptr;
		}

		IWICBitmap *bitmap = nullptr;
		hr = g_wic->CreateBitmap((UINT)w, (UINT)h, GUID_WICPixelFormat32bppPBGRA,
			WICBitmapCacheOnLoad, &bitmap);
		if (FAILED(hr))
		{
			WarnOnce("CreateBitmap", hr, nullptr);
			layout->Release();
			return nullptr;
		}

		// Software rendering on purpose: GTA's D3D11 device has no BGRA support,
		// so Direct2D cannot be bound to it.
		ID2D1RenderTarget *rt = nullptr;
		D2D1_RENDER_TARGET_PROPERTIES props = D2D1::RenderTargetProperties(
			D2D1_RENDER_TARGET_TYPE_SOFTWARE,
			D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED),
			96.0f, 96.0f);

		hr = g_d2d->CreateWicBitmapRenderTarget(bitmap, props, &rt);
		if (FAILED(hr))
		{
			WarnOnce("CreateWicBitmapRenderTarget", hr, nullptr);
			bitmap->Release();
			layout->Release();
			return nullptr;
		}

		ID2D1SolidColorBrush *brush = nullptr;
		const float a = (float)q.a / 255.0f;
		hr = rt->CreateSolidColorBrush(
			D2D1::ColorF((float)q.r / 255.0f, (float)q.g / 255.0f, (float)q.b / 255.0f, a), &brush);
		if (FAILED(hr))
		{
			rt->Release();
			bitmap->Release();
			layout->Release();
			return nullptr;
		}

		rt->BeginDraw();
		rt->Clear(D2D1::ColorF(0, 0.0f));
		rt->DrawTextLayout(D2D1::Point2F(padX, padY), layout, brush, D2D1_DRAW_TEXT_OPTIONS_NONE);
		hr = rt->EndDraw();

		if (FAILED(hr))
		{
			WarnOnce("EndDraw", hr, nullptr);
			brush->Release();
			rt->Release();
			bitmap->Release();
			layout->Release();
			return nullptr;
		}

		// ---- pixels -> D3D11 texture ------------------------------------
		ID3D11ShaderResourceView *srv = nullptr;
		IWICBitmapLock *lock = nullptr;
		WICRect rc = { 0, 0, w, h };

		if (SUCCEEDED(bitmap->Lock(&rc, WICBitmapLockRead, &lock)))
		{
			UINT stride = 0;
			UINT size = 0;
			BYTE *pixels = nullptr;

			if (SUCCEEDED(lock->GetStride(&stride)) &&
				SUCCEEDED(lock->GetDataPointer(&size, &pixels)) && pixels)
			{
				D3D11_TEXTURE2D_DESC td;
				ZeroMemory(&td, sizeof(td));
				td.Width = (UINT)w;
				td.Height = (UINT)h;
				td.MipLevels = 1;
				td.ArraySize = 1;
				td.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
				td.SampleDesc.Count = 1;
				td.Usage = D3D11_USAGE_DEFAULT;
				td.BindFlags = D3D11_BIND_SHADER_RESOURCE;

				D3D11_SUBRESOURCE_DATA init;
				ZeroMemory(&init, sizeof(init));
				init.pSysMem = pixels;
				init.SysMemPitch = stride;

				ID3D11Texture2D *tex = nullptr;
				if (SUCCEEDED(g_device->CreateTexture2D(&td, &init, &tex)) && tex)
				{
					g_device->CreateShaderResourceView(tex, nullptr, &srv);
					tex->Release();
				}
			}
			lock->Release();
		}

		brush->Release();
		rt->Release();
		bitmap->Release();
		layout->Release();

		if (!srv)
		{
			WarnOnce("texture upload", E_FAIL, nullptr);
			return nullptr;
		}

		Cached entry;
		entry.srv = srv;
		entry.w = w;
		entry.h = h;
		entry.padX = padX;
		entry.padY = padY;

		std::pair<std::map<CacheKey, Cached>::iterator, bool> ins = g_cache.insert(std::make_pair(key, entry));
		return &ins.first->second;
	}

	void DrawQuad(float nx, float ny, float nw, float nh, ID3D11ShaderResourceView *srv,
		float r, float g, float b, float a)
	{
		struct
		{
			float rect[4];
			float colour[4];
		} params;

		params.rect[0] = nx;
		params.rect[1] = ny;
		params.rect[2] = nw;
		params.rect[3] = nh;
		params.colour[0] = r;
		params.colour[1] = g;
		params.colour[2] = b;
		params.colour[3] = a;

		D3D11_MAPPED_SUBRESOURCE mapped;
		if (SUCCEEDED(g_context->Map(g_cbuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
		{
			memcpy(mapped.pData, &params, sizeof(params));
			g_context->Unmap(g_cbuffer, 0);
		}

		g_context->PSSetShaderResources(0, 1, &srv);
		g_context->Draw(6, 0);
	}

	// ----------------------------------------------------------------------
	// Pipeline state guard.
	//
	// The overlay draws inside the game's Present callback, on the game's own
	// immediate context. Anything changed there stays changed for whatever runs
	// next unless it is put back, and that includes the passes that do not
	// re-issue every piece of state themselves - particles, light and glow
	// effects, and parts of the deferred geometry.
	//
	// Without this, one drawn string was enough to leave the overlay's blend
	// state, viewport, shaders and render targets bound: the frame after it
	// could then lose exactly those effects, and the loss did not heal until
	// the game happened to rebuild the state on its own. This is the reason the
	// picture broke as soon as a language that draws through here was selected.
	//
	// Every D3D11 getter hands back a reference, so the saved pointers are
	// released after they are bound again.
	// ----------------------------------------------------------------------
	struct PipelineState
	{
		ID3D11RenderTargetView *rtv[D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT];
		ID3D11DepthStencilView *dsv;
		ID3D11BlendState *blend;
		float blendFactor[4];
		UINT sampleMask;
		ID3D11RasterizerState *raster;
		UINT viewportCount;
		D3D11_VIEWPORT viewport[D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE];
		ID3D11VertexShader *vs;
		ID3D11PixelShader *ps;
		ID3D11InputLayout *inputLayout;
		D3D11_PRIMITIVE_TOPOLOGY topology;
		ID3D11Buffer *vsConstant;
		ID3D11Buffer *psConstant;
		ID3D11SamplerState *psSampler;
		ID3D11ShaderResourceView *psResource;
	};

	void SavePipelineState(PipelineState &s)
	{
		ZeroMemory(&s, sizeof(s));

		g_context->OMGetRenderTargets(D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT, s.rtv, &s.dsv);
		g_context->OMGetBlendState(&s.blend, s.blendFactor, &s.sampleMask);
		g_context->RSGetState(&s.raster);

		UINT viewports = D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE;
		g_context->RSGetViewports(&viewports, s.viewport);
		s.viewportCount = viewports;

		g_context->VSGetShader(&s.vs, nullptr, nullptr);
		g_context->PSGetShader(&s.ps, nullptr, nullptr);
		g_context->IAGetInputLayout(&s.inputLayout);
		g_context->IAGetPrimitiveTopology(&s.topology);
		g_context->VSGetConstantBuffers(0, 1, &s.vsConstant);
		g_context->PSGetConstantBuffers(0, 1, &s.psConstant);
		g_context->PSGetSamplers(0, 1, &s.psSampler);
		g_context->PSGetShaderResources(0, 1, &s.psResource);
	}

	void RestorePipelineState(PipelineState &s)
	{
		g_context->OMSetRenderTargets(D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT, s.rtv, s.dsv);
		g_context->OMSetBlendState(s.blend, s.blendFactor, s.sampleMask);
		g_context->RSSetState(s.raster);
		if (s.viewportCount > 0)
			g_context->RSSetViewports(s.viewportCount, s.viewport);
		g_context->VSSetShader(s.vs, nullptr, 0);
		g_context->PSSetShader(s.ps, nullptr, 0);
		g_context->IASetInputLayout(s.inputLayout);
		g_context->IASetPrimitiveTopology(s.topology);
		g_context->VSSetConstantBuffers(0, 1, &s.vsConstant);
		g_context->PSSetConstantBuffers(0, 1, &s.psConstant);
		g_context->PSSetSamplers(0, 1, &s.psSampler);
		g_context->PSSetShaderResources(0, 1, &s.psResource);

		for (int i = 0; i < D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT; i++)
		{
			if (s.rtv[i])
				s.rtv[i]->Release();
		}
		if (s.dsv) s.dsv->Release();
		if (s.blend) s.blend->Release();
		if (s.raster) s.raster->Release();
		if (s.vs) s.vs->Release();
		if (s.ps) s.ps->Release();
		if (s.inputLayout) s.inputLayout->Release();
		if (s.vsConstant) s.vsConstant->Release();
		if (s.psConstant) s.psConstant->Release();
		if (s.psSampler) s.psSampler->Release();
		if (s.psResource) s.psResource->Release();
	}
}

namespace DxText
{
	bool Failed() { return g_failed; }

	void Register()
	{
		if (g_registered)
			return;
		g_registered = true;
		presentCallbackRegister(OnPresent);
		Log::Line("dxtext: present callback registered");
	}

	void Unregister()
	{
		if (!g_registered)
			return;
		presentCallbackUnregister(OnPresent);

		std::lock_guard<std::mutex> renderLock(g_renderMutex);
		g_registered = false;
		ReleaseCache();
		if (g_raster) { g_raster->Release(); g_raster = nullptr; }
		if (g_blend) { g_blend->Release(); g_blend = nullptr; }
		if (g_sampler) { g_sampler->Release(); g_sampler = nullptr; }
		if (g_cbuffer) { g_cbuffer->Release(); g_cbuffer = nullptr; }
		if (g_ps) { g_ps->Release(); g_ps = nullptr; }
		if (g_vs) { g_vs->Release(); g_vs = nullptr; }
		if (g_context) { g_context->Release(); g_context = nullptr; }
		if (g_device) { g_device->Release(); g_device = nullptr; }
		if (g_wic) { g_wic->Release(); g_wic = nullptr; }
		if (g_dwrite) { g_dwrite->Release(); g_dwrite = nullptr; }
		if (g_d2d) { g_d2d->Release(); g_d2d = nullptr; }
	}

	void BeginFrame()
	{
		std::lock_guard<std::mutex> lock(g_queueMutex);
		g_next.clear();
		++g_frameId;
	}

	void AddText(const std::string &utf8, float x, float y, float scale, const int col[4], int align)
	{
		if (g_failed || utf8.empty())
			return;

		Queued q;
		q.text = utf8;
		q.x = x;
		q.y = y;
		q.scale = scale;
		q.r = col[0];
		q.g = col[1];
		q.b = col[2];
		q.a = col[3];
		q.align = align;

		std::lock_guard<std::mutex> lock(g_queueMutex);
		g_next.push_back(q);
	}

	void OnPresent(void *swapChain)
	{
		std::lock_guard<std::mutex> renderLock(g_renderMutex);
		if (!g_registered || g_failed || !swapChain)
			return;

		// Take a new frame from the script thread, but keep drawing the last one
		// when this Present has no fresh tick behind it. Swapping on every
		// Present drained the queue on the first of several presents per tick,
		// and the rest drew nothing - text then flickered or vanished.
		{
			std::lock_guard<std::mutex> lock(g_queueMutex);
			if (g_drawnFrameId != g_frameId)
			{
				g_current.swap(g_next);
				g_next.clear();
				g_drawnFrameId = g_frameId;
			}
		}

		if (g_current.empty())
			return;

		IDXGISwapChain *swap = (IDXGISwapChain *)swapChain;

		if (!InitD3D(swap))
			return;

		const bool rtl = Lang::IsRtl(Lang::Current());
		if (!InitText(rtl))
			return;

		// ---- back buffer -------------------------------------------------
		// Create and release the RTV each Present rather than caching it across
		// frames: holding an outstanding RTV on the backbuffer prevents
		// IDXGISwapChain::ResizeBuffers from succeeding on resolution/fullscreen
		// changes.
		ID3D11Texture2D *back = nullptr;
		if (FAILED(swap->GetBuffer(0, __uuidof(ID3D11Texture2D), (void **)&back)) || !back)
			return;

		ID3D11RenderTargetView *rtv = nullptr;
		if (FAILED(g_device->CreateRenderTargetView(back, nullptr, &rtv)) || !rtv)
		{
			back->Release();
			return;
		}

		D3D11_TEXTURE2D_DESC bd;
		back->GetDesc(&bd);
		g_screenWidth = (int)bd.Width;
		g_screenHeight = (int)bd.Height;

		// ---- pipeline state ---------------------------------------------
		// Saved first: everything set below is the game's state being borrowed,
		// and it is handed back unchanged at the end of this function. See the
		// note on PipelineState - leaving it bound is what used to break the
		// particle and light passes of the following frames.
		PipelineState saved;
		SavePipelineState(saved);

		const float clear[4] = { 0, 0, 0, 0 };
		g_context->OMSetRenderTargets(1, &rtv, nullptr);
		g_context->OMSetBlendState(g_blend, clear, 0xFFFFFFFF);

		// The game leaves its own rasterizer state and viewport bound. Pin both
		// to the full back buffer so a stale scissor rect or a cull mode cannot
		// silently eat the quads.
		g_context->RSSetState(g_raster);
		D3D11_VIEWPORT vp;
		vp.TopLeftX = 0.0f;
		vp.TopLeftY = 0.0f;
		vp.Width = (float)g_screenWidth;
		vp.Height = (float)g_screenHeight;
		vp.MinDepth = 0.0f;
		vp.MaxDepth = 1.0f;
		g_context->RSSetViewports(1, &vp);

		g_context->VSSetShader(g_vs, nullptr, 0);
		g_context->PSSetShader(g_ps, nullptr, 0);
		// The constant buffer has to be bound to BOTH stages: the vertex shader
		// reads the quad rect, the pixel shader reads the colour. Binding it to
		// the VS only left the PS reading a null buffer, i.e. gColor == 0, which
		// made every overlay glyph multiply out to fully transparent - the whole
		// Arabic / CJK / Spanish overlay drew nothing.
		g_context->VSSetConstantBuffers(0, 1, &g_cbuffer);
		g_context->PSSetConstantBuffers(0, 1, &g_cbuffer);
		g_context->PSSetSamplers(0, 1, &g_sampler);
		g_context->IASetInputLayout(nullptr);
		g_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		const wchar_t *family = Lang::DxFontFamily(Lang::Current());
		const float bufW = (float)g_screenWidth;
		const float bufH = (float)g_screenHeight;

		for (size_t i = 0; i < g_current.size(); i++)
		{
			const Queued &q = g_current[i];
			const Cached *entry = GetOrCreate(q, family, rtl, PAD_Y_FRAC);
			if (!entry)
				continue;

			const float wN = (float)entry->w / bufW;
			const float hN = (float)entry->h / bufH;

			float left = q.x - entry->padX / bufW;
			if (q.align == AlignRight)
				left = q.x - wN + entry->padX / bufW;
			else if (q.align == AlignCenter)
				left = q.x - wN * 0.5f;

			const float top = q.y - entry->padY / bufH;

			const float nx = left * 2.0f - 1.0f;
			const float ny = 1.0f - top * 2.0f;

			DrawQuad(nx, ny, wN * 2.0f, -hN * 2.0f, entry->srv, 1.0f, 1.0f, 1.0f, 1.0f);
		}

		// Hand the game back exactly what it had bound when it called Present.
		RestorePipelineState(saved);

		if (g_logFont != family || g_logRtl != rtl)
		{
			g_logFont = family;
			g_logRtl = rtl;
			Log::Line("dxtext: overlay drawing %d run(s), font '%ls', rtl %d",
				(int)g_current.size(), family, rtl ? 1 : 0);
		}

		rtv->Release();
		back->Release();
	}
}
