#pragma once

#include <FrameWork/FrameWork.hpp>

#include <d3d11.h>
#include <d3dx11.h>

namespace FrameWork
{
	class Interface
	{
	public:
		Interface() { }
		Interface(HWND Window, HWND TargetWindow, ID3D11Device* Device, ID3D11DeviceContext* DeviceContext) { Initialize(Window, TargetWindow, Device, DeviceContext); }
		~Interface() { ShutDown(); }

		void Initialize(HWND Window, HWND TargetWindow, ID3D11Device* Device, ID3D11DeviceContext* DeviceContext);
		void UpdateStyle();
		void RenderGui();
		void WindowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
		void HandleMenuKey();
		void ShutDown();
		bool GetMenuOpen()          { return bIsMenuOpen; }
		bool HandleReloadIfPending();

	private:
		HWND          hWindow;
		HWND          hTargetWindow;
		ID3D11Device* IDevice;

		int  CurrentTab    = 0;
		bool bIsMenuOpen   = true;
		bool bReloadPending= false;

	private:
		// 8 tabs: AIM(3) + VISUALS(3) + SYSTEM(2)
		void AimbotTab();
		void TriggerTab();
		void PlayersESPTab();
		void VehiclesESPTab();   // includes Radar section
		void WorldTab();
		void ExploitsTab();
		void SettingsTab();

		void DrawHitBoxPanel(int* pHitBox);
		void DrawESPPreview();

	public:
		UINT ResizeWidht;
		UINT ResizeHeight;
	};
}
