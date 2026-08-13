#include "ScreenVis.hpp"
#include <cstring>

// The project's local DirectX SDK DXGIType.h sets __dxgitype_h__ without
// defining DXGI_RGBA or D3DCOLORVALUE, which the Windows SDK dxgi1_2.h needs.
// Pre-define them here so the include below compiles cleanly.
#ifndef D3DCOLORVALUE_DEFINED
typedef struct _D3DCOLORVALUE { float r, g, b, a; } D3DCOLORVALUE;
#define D3DCOLORVALUE_DEFINED
#endif
typedef D3DCOLORVALUE DXGI_RGBA;

#include <dxgi1_2.h>
#pragma comment(lib, "dxgi.lib")

namespace Cheat
{
	IDXGIOutputDuplication* ScreenVis::s_pDupl     = nullptr;
	ID3D11Texture2D*        ScreenVis::s_pStaging   = nullptr;
	ID3D11DeviceContext*    ScreenVis::s_pCtx       = nullptr;
	std::vector<uint8_t>    ScreenVis::s_pixelBuf;
	int                     ScreenVis::s_width      = 0;
	int                     ScreenVis::s_height     = 0;
	int                     ScreenVis::s_pitch      = 0;
	bool                    ScreenVis::s_valid      = false;

	static constexpr float k_VisThresholdSq = 20.0f * 20.0f;  // 20 per channel

	bool ScreenVis::Init(ID3D11Device* pDevice, HWND /*hOverlay*/)
	{
		if (!pDevice) return false;
		pDevice->GetImmediateContext(&s_pCtx);

		IDXGIDevice*  pDxgiDev = nullptr;
		IDXGIAdapter* pAdapter = nullptr;
		IDXGIOutput*  pOutput  = nullptr;
		IDXGIOutput1* pOutput1 = nullptr;
		bool ok = false;

		do
		{
			if (FAILED(pDevice->QueryInterface(__uuidof(IDXGIDevice),  (void**)&pDxgiDev))) break;
			if (FAILED(pDxgiDev->GetAdapter(&pAdapter)))                                    break;
			if (FAILED(pAdapter->EnumOutputs(0, &pOutput)))                                 break;
			if (FAILED(pOutput->QueryInterface(__uuidof(IDXGIOutput1), (void**)&pOutput1))) break;
			if (FAILED(pOutput1->DuplicateOutput(pDevice, &s_pDupl)))                       break;

			DXGI_OUTPUT_DESC outDesc{};
			pOutput->GetDesc(&outDesc);
			s_width  = outDesc.DesktopCoordinates.right  - outDesc.DesktopCoordinates.left;
			s_height = outDesc.DesktopCoordinates.bottom - outDesc.DesktopCoordinates.top;

			DXGI_OUTDUPL_DESC duplDesc{};
			s_pDupl->GetDesc(&duplDesc);

			D3D11_TEXTURE2D_DESC td{};
			td.Width          = s_width;
			td.Height         = s_height;
			td.MipLevels      = 1;
			td.ArraySize      = 1;
			td.Format         = duplDesc.ModeDesc.Format;
			td.SampleDesc     = { 1, 0 };
			td.Usage          = D3D11_USAGE_STAGING;
			td.CPUAccessFlags = D3D11_CPU_ACCESS_READ;

			if (FAILED(pDevice->CreateTexture2D(&td, nullptr, &s_pStaging))) break;
			ok = true;
		} while (false);

		if (pOutput1) pOutput1->Release();
		if (pOutput)  pOutput->Release();
		if (pAdapter) pAdapter->Release();
		if (pDxgiDev) pDxgiDev->Release();

		if (!ok)
		{
			if (s_pStaging) { s_pStaging->Release(); s_pStaging = nullptr; }
			if (s_pDupl)    { s_pDupl->Release();    s_pDupl    = nullptr; }
			if (s_pCtx)     { s_pCtx->Release();     s_pCtx     = nullptr; }
		}
		return ok;
	}

	void ScreenVis::Shutdown()
	{
		s_valid = false;
		s_pixelBuf.clear();
		s_pixelBuf.shrink_to_fit();

		if (s_pStaging) { s_pStaging->Release(); s_pStaging = nullptr; }
		if (s_pDupl)    { s_pDupl->Release();    s_pDupl    = nullptr; }
		if (s_pCtx)     { s_pCtx->Release();     s_pCtx     = nullptr; }
	}

	// Capture the latest desktop frame into a plain CPU buffer.
	//
	// The staging texture is mapped and immediately unmapped after memcpy —
	// it is never held mapped across frames.  This avoids crashes from D3D11
	// device resets or DWM compositor changes invalidating the mapped pointer.
	//
	// On WAIT_TIMEOUT (no new game frame) the previous CPU buffer is reused at
	// zero cost — no D3D work at all.
	void ScreenVis::CaptureFrame()
	{
		if (!s_pDupl || !s_pStaging || !s_pCtx) return;

		DXGI_OUTDUPL_FRAME_INFO fi{};
		IDXGIResource* pRes = nullptr;
		HRESULT hr = s_pDupl->AcquireNextFrame(0, &fi, &pRes);

		if (hr == DXGI_ERROR_WAIT_TIMEOUT)
		{
			// No new game frame — previous CPU buffer stays valid.
			return;
		}

		if (hr == DXGI_ERROR_ACCESS_LOST)
		{
			s_valid = false;
			s_pDupl->Release(); s_pDupl = nullptr;
			return;
		}

		if (FAILED(hr) || !pRes)
		{
			s_valid = false;
			return;
		}

		ID3D11Texture2D* pTex = nullptr;
		if (SUCCEEDED(pRes->QueryInterface(__uuidof(ID3D11Texture2D), (void**)&pTex)))
		{
			s_pCtx->CopyResource(s_pStaging, pTex);
			pTex->Release();
		}
		pRes->Release();
		s_pDupl->ReleaseFrame();

		D3D11_MAPPED_SUBRESOURCE mapped{};
		if (SUCCEEDED(s_pCtx->Map(s_pStaging, 0, D3D11_MAP_READ, 0, &mapped)))
		{
			const size_t needed = static_cast<size_t>(s_height) * mapped.RowPitch;
			if (s_pixelBuf.size() != needed)
				s_pixelBuf.resize(needed);

			std::memcpy(s_pixelBuf.data(), mapped.pData, needed);
			s_pitch = static_cast<int>(mapped.RowPitch);
			s_pCtx->Unmap(s_pStaging, 0);  // unmap immediately — never held across frames
			s_valid = true;
		}
		else
		{
			s_valid = false;
		}
	}

	bool ScreenVis::IsVisible(ImVec2 headPos, float bboxHeight, bool sidesOnly)
	{
		if (!s_valid || s_pixelBuf.empty()) return true;

		const int          cx    = static_cast<int>(headPos.x);
		const int          cy    = static_cast<int>(headPos.y);
		const int          pitch = s_pitch;
		const int          width = s_width;
		const int          height = s_height;
		const uint8_t*     bits  = s_pixelBuf.data();

		int r = static_cast<int>(bboxHeight / 12.0f);
		if (r < 5)  r = 5;
		if (r > 14) r = 14;  // smaller cap → ~4x fewer pixels per sample

		auto avgColor = [&](int offX, int offY, float& outB, float& outG, float& outR) -> bool
		{
			long long sB = 0, sG = 0, sR = 0;
			int cnt = 0;
			const int x0 = cx + offX - r, x1 = cx + offX + r;
			const int y0 = cy + offY - r, y1 = cy + offY + r;
			for (int py = y0; py <= y1; ++py)
			{
				if (py < 0 || py >= height) continue;
				const uint8_t* row = bits + py * pitch;
				for (int px = x0; px <= x1; ++px)
				{
					if (px < 0 || px >= width) continue;
					const uint8_t* p = row + px * 4;
					sB += p[0]; sG += p[1]; sR += p[2];
					++cnt;
				}
			}
			if (!cnt) return false;
			const float inv = 1.0f / static_cast<float>(cnt);
			outB = static_cast<float>(sB) * inv;
			outG = static_cast<float>(sG) * inv;
			outR = static_cast<float>(sR) * inv;
			return true;
		};

		float fB, fG, fR;
		if (!avgColor(0, 0, fB, fG, fR)) return true;

		// 4 background samples: left/right at same height, above/below outside bbox.
		// Using all 4 handles vertical camera movement (above/below help) and
		// horizontal variation (left/right at same height help).
		const int sideOff  = static_cast<int>(bboxHeight * 0.38f) + r + 10;
		const int aboveOff = -(r * 2 + 8);
		const int belowOff = static_cast<int>(bboxHeight) + r * 2 + 8;

		// Head: 4 cardinal directions. Chest (sidesOnly): left/right only.
		float bgB[4]{}, bgG[4]{}, bgR[4]{};
		bool  bgOk[4]{};
		int   bgCount = 0;

		bgOk[bgCount] = avgColor(-sideOff, 0,        bgB[bgCount], bgG[bgCount], bgR[bgCount]); ++bgCount;
		bgOk[bgCount] = avgColor(+sideOff, 0,        bgB[bgCount], bgG[bgCount], bgR[bgCount]); ++bgCount;

		if (!sidesOnly)
		{
			bgOk[bgCount] = avgColor(0, aboveOff, bgB[bgCount], bgG[bgCount], bgR[bgCount]); ++bgCount;
			bgOk[bgCount] = avgColor(0, belowOff, bgB[bgCount], bgG[bgCount], bgR[bgCount]); ++bgCount;
		}

		// Minimum distance: if center matches ANY background sample, it IS that
		// surface (hidden). Averaging mixed wall-section colors was creating a
		// fictional "middle" color that no surface actually had, making everything
		// look different from its neighbours — i.e. visible.
		float minDistSq = 1e30f;
		for (int i = 0; i < bgCount; ++i)
		{
			if (!bgOk[i]) continue;
			const float dB = fB - bgB[i], dG = fG - bgG[i], dR = fR - bgR[i];
			const float distSq = dB*dB + dG*dG + dR*dR;
			if (distSq < minDistSq) minDistSq = distSq;
		}
		if (minDistSq >= 1e30f) return true;
		return minDistSq > k_VisThresholdSq;
	}
}
