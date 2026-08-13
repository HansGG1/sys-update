#include "Overlay.hpp"

struct WndRECT : public RECT
{
	int Width() { return right - left; }
	int Height() { return bottom - top; }
};

static inline std::function<void(HWND, UINT, WPARAM, LPARAM)> pWindowProc;

bool bSettuped = false;
bool bInitialized = false;

bool bDeviceInitialized;
bool bRenderTargetInitialized;

static int s_GameX = 0, s_GameY = 0, s_GameW = 1920, s_GameH = 1080;
static int s_MonitorIndex = 0;

static BOOL CALLBACK _OverlayMonitorEnum(HMONITOR, HDC, LPRECT r, LPARAM p)
{
	auto* v = reinterpret_cast<std::vector<RECT>*>(p);
	v->push_back(*r);
	return TRUE;
}

HWND hWindow;
WNDCLASSEX WindowClass;
HWND hTargetWindow;
WndRECT wTargetWindowRect;
DWORD sTargetPid;

ID3D11Device* ID3dDevice;
ID3D11DeviceContext* ID3dDeviceContext;
IDXGISwapChain* ID3dSwapChain;
ID3D11RenderTargetView* ID3dRenderTargetView;

void CreateDeviceD3D()
{
	DXGI_SWAP_CHAIN_DESC SwapChainDesc;
	ZeroMemory(&SwapChainDesc, sizeof(SwapChainDesc));
	SwapChainDesc.BufferDesc.Width = 0;
	SwapChainDesc.BufferDesc.Height = 0;
	SwapChainDesc.BufferDesc.RefreshRate.Numerator = 60;
	SwapChainDesc.BufferDesc.RefreshRate.Denominator = 1;
	SwapChainDesc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	SwapChainDesc.SampleDesc.Count = 1;
	SwapChainDesc.SampleDesc.Quality = 0;
	SwapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	SwapChainDesc.BufferCount = 2;
	SwapChainDesc.OutputWindow = hWindow;
	SwapChainDesc.Windowed = TRUE;
	SwapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
	SwapChainDesc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;

	D3D_FEATURE_LEVEL FeatureLevel;
	const D3D_FEATURE_LEVEL FeatureLevelArray[2] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0, };
	if (D3D11CreateDeviceAndSwapChain(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, 0, FeatureLevelArray, 2, D3D11_SDK_VERSION, &SwapChainDesc, &ID3dSwapChain, &ID3dDevice, &FeatureLevel, &ID3dDeviceContext) != S_OK)
	{
#ifdef _DEBUG
		std::cout << XorStr("[ERROR : FrameWork::Window::InitializeDirectX11::D3D11CreateDeviceAndSwapChain] Error: ") << SafeCall(GetLastError)() << std::endl;
#endif // _DEBUG

		return;
	}

	bDeviceInitialized = true;
}

LRESULT CALLBACK WindowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	if (pWindowProc)
		pWindowProc(hWnd, uMsg, wParam, lParam);

	return DefWindowProc(hWnd, uMsg, wParam, lParam);
}

namespace FrameWork
{
	void Overlay::Setup(DWORD TargetPid)
	{
		sTargetPid = TargetPid;

		hTargetWindow = Memory::GetWindowHandleByPID(TargetPid);
		if (hTargetWindow)
		{
			// Get Target Window Size
			SafeCall(GetClientRect)(hTargetWindow, &wTargetWindowRect);
			SafeCall(MapWindowPoints)(hTargetWindow, nullptr, reinterpret_cast<LPPOINT>(&wTargetWindowRect), 2);

			bSettuped = true;
		}
		else
		{
#ifdef _DEBUG
			std::cout << XorStr("[ERROR : FrameWork::Window::Setup] Window Not Found!") << std::endl;
#endif // _DEBUG
		}

		DWORD dwErr;
		HANDLE hTokenUIAccess;
		BOOL fUIAccess;
	}

	void Overlay::Initialize()
	{
		if (!bSettuped)
		{
#ifdef _DEBUG
			std::cout << XorStr("[ERROR : FrameWork::Window::Initialize] Overlay Not Settuped!") << std::endl;
#endif // _DEBUG

			return;
		}

		WindowClass.cbSize = sizeof(WindowClass);
		WindowClass.style = CS_HREDRAW | CS_VREDRAW;
		WindowClass.lpfnWndProc = WindowProc;
		WindowClass.cbClsExtra = 0;
		WindowClass.cbWndExtra = 0;
		WindowClass.hInstance = GetModuleHandle(NULL);
		WindowClass.hIcon = NULL;
		WindowClass.hCursor = LoadCursor(NULL, IDC_ARROW);
		WindowClass.hbrBackground = HBRUSH(RGB(0, 0, 0));
		WindowClass.lpszMenuName = NULL;
		WindowClass.lpszClassName = XorStr(L"Window");
		WindowClass.hIconSm = NULL;

		ATOM Class = SafeCall(RegisterClassEx)(&WindowClass);

		if (!Class)
		{
#ifdef _DEBUG
			std::cout << XorStr("[ERROR : FrameWork::Window::Initialize::RegisterClassEx] Error: ") << SafeCall(GetLastError)() << std::endl;
#endif // _DEBUG

			return;
		}

		hWindow = CreateWindowEx(WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_TRANSPARENT | WS_EX_NOACTIVATE, WindowClass.lpszClassName, WindowClass.lpszMenuName, WS_POPUP | WS_VISIBLE, wTargetWindowRect.left, wTargetWindowRect.top, wTargetWindowRect.Width(), wTargetWindowRect.Height(), NULL, NULL, GetModuleHandle(NULL), NULL);

		if (!hWindow)
		{
#ifdef _DEBUG
			std::cout << XorStr("[ERROR : FrameWork::Window::Initialize::CreateWindowEx] Error: ") << SafeCall(GetLastError)() << std::endl;
#endif // _DEBUG

			return;
		}

		// Desktop Window Manager — extend into entire client area for full transparency
		MARGINS Margins = { -1 };
		DwmExtendFrameIntoClientArea(hWindow, &Margins);
		
		// Apply Transparent
		SafeCall(SetLayeredWindowAttributes)(hWindow, RGB(0, 0, 0), 255, LWA_ALPHA);

		SafeCall(ShowWindow)(hWindow, SW_SHOWDEFAULT);
		SafeCall(UpdateWindow)(hWindow);

		bInitialized = true;

		dxInitialize();
	}

	void Overlay::ShutDown()
	{
		SafeCall(DestroyWindow)(hWindow);
		SafeCall(UnregisterClass)(WindowClass.lpszClassName, WindowClass.hInstance);

		bInitialized = false; bSettuped = false;
	}

	void Overlay::SetMonitorIndex(int idx) { s_MonitorIndex = idx; }

	void Overlay::UpdateWindowPos()
	{
		// Only re-enumerate monitors every 60 frames — EnumDisplayMonitors + MoveWindow
		// called every frame forces DWM recomposition each time, causing visible blinking.
		static int s_Tick = 0;
		if (++s_Tick < 60) return;
		s_Tick = 0;

		std::vector<RECT> monitors;
		EnumDisplayMonitors(nullptr, nullptr, _OverlayMonitorEnum, (LPARAM)&monitors);
		if (monitors.empty()) return;

		int idx = (s_MonitorIndex < (int)monitors.size()) ? s_MonitorIndex : 0;
		const RECT& r = monitors[idx];

		int nx = r.left, ny = r.top;
		int nw = r.right - r.left, nh = r.bottom - r.top;

		// Only call MoveWindow if the geometry actually changed
		if (nx == s_GameX && ny == s_GameY && nw == s_GameW && nh == s_GameH)
			return;

		s_GameX = nx; s_GameY = ny;
		s_GameW = nw; s_GameH = nh;
		MoveWindow(hWindow, s_GameX, s_GameY, s_GameW, s_GameH, false);
	}

	void Overlay::SetupWindowProcHook(std::function<void(HWND, UINT, WPARAM, LPARAM)> Funtion)
	{
		pWindowProc = Funtion;
	}

	void Overlay::dxInitialize()
	{
		CreateDeviceD3D();
		if (bDeviceInitialized)
		{
			dxCreateRenderTarget();
		}
	}

	void Overlay::dxRefresh()
	{
		ID3dDeviceContext->OMSetRenderTargets(1, &ID3dRenderTargetView, nullptr);
		static float TransparentColor[4] = { 0, 0, 0, 0 };
		ID3dDeviceContext->ClearRenderTargetView(ID3dRenderTargetView, TransparentColor);
	}

	void Overlay::dxShutDown()
	{
		dxCleanupRenderTarget();
		dxCleanupDeviceD3D();
	}

	void Overlay::dxCreateRenderTarget()
	{
		ID3D11Texture2D* pBackBuffer;
		ID3dSwapChain->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer));
		ID3dDevice->CreateRenderTargetView(pBackBuffer, NULL, &ID3dRenderTargetView);
		pBackBuffer->Release();

		bRenderTargetInitialized = true;
	}

	void Overlay::dxCleanupRenderTarget()
	{
		if (ID3dRenderTargetView) { ID3dRenderTargetView->Release(); ID3dRenderTargetView = NULL; }

		bRenderTargetInitialized = false;
	}

	void Overlay::dxCleanupDeviceD3D()
	{
		if (ID3dSwapChain) { ID3dSwapChain->Release(); ID3dSwapChain = NULL; }
		if (ID3dDeviceContext) { ID3dDeviceContext->Release(); ID3dDeviceContext = NULL; }
		if (ID3dDevice) { ID3dDevice->Release(); ID3dDevice = NULL; }

		bDeviceInitialized = false;
	}

	bool Overlay::IsSettuped() { return bSettuped; }
	bool Overlay::IsInitialized() { return bInitialized; }
	HWND Overlay::GetOverlayWindow() { return hWindow; }
	HWND Overlay::GetTargetWindow() { return hTargetWindow; }

	void Overlay::GetGameRect(int& outX, int& outY, int& outW, int& outH) {
		// Overlay covers exactly the selected monitor; ImGui origin = monitor top-left
		outX = 0; outY = 0; outW = s_GameW; outH = s_GameH;
	}

	ID3D11Device* Overlay::dxGetDevice() { return ID3dDevice; }
	ID3D11DeviceContext* Overlay::dxGetDeviceContext() { return ID3dDeviceContext; }
	IDXGISwapChain* Overlay::dxGetSwapChain() { return ID3dSwapChain; }
	ID3D11RenderTargetView* Overlay::dxGetRenderTarget() { return ID3dRenderTargetView; }
}