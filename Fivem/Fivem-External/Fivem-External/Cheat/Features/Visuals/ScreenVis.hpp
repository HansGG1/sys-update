#pragma once
#include <cstdint>
#include <vector>
#include <d3d11.h>
#include <FrameWork/Dependencies/ImGui/imgui.h>

// Forward-declare DXGI 1.2 type so we don't pull in <dxgi1_2.h> here.
// The local DirectX SDK DXGIType.h pre-empts the Windows SDK one and lacks
// DXGI_RGBA, causing compile errors in any TU that includes dxgi1_2.h.
// Full definition lives only in ScreenVis.cpp.
struct IDXGIOutputDuplication;

namespace Cheat
{
	class ScreenVis
	{
	public:
		static bool Init(ID3D11Device* pDevice, HWND hOverlay);
		static void Shutdown();
		static void CaptureFrame();
		static bool IsVisible(ImVec2 pos, float bboxHeight, bool sidesOnly = false);

	private:
		static IDXGIOutputDuplication*  s_pDupl;
		static ID3D11Texture2D*         s_pStaging;
		static ID3D11DeviceContext*     s_pCtx;
		static std::vector<uint8_t>     s_pixelBuf;  // CPU copy — always valid, no D3D lifetime issues
		static int                      s_width;
		static int                      s_height;
		static int                      s_pitch;
		static bool                     s_valid;
	};
}
