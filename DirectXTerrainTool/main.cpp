#include "Window.h"
#include "GraphicsCore.h"
#include "TextRenderer.h"
#include "MenuScene.h"

#include <cstdio>

namespace
{
	constexpr UINT DefaultWidth = 1280;
	constexpr UINT DefaultHeight = 720;
	constexpr int FontHeight = 18;
	constexpr const wchar_t* AppTitle = L"DirectX Terrain Tool";

	const std::wstring HudHint = L"ESC: 메뉴로 돌아가기   W: 와이어프레임 토글";

	const DirectX::XMFLOAT4 HudColor = { 0.92f, 0.94f, 1.00f, 1.0f };
	const DirectX::XMFLOAT4 HintColor = { 0.70f, 0.78f, 0.95f, 1.0f };
}

int APIENTRY wWinMain(
	_In_ HINSTANCE hInstance,
	_In_opt_ HINSTANCE hPrevInstance,
	_In_ LPWSTR lpCmdLine,
	_In_ int nCmdShow)
{
	UNREFERENCED_PARAMETER(hPrevInstance);
	UNREFERENCED_PARAMETER(lpCmdLine);

	Window window;
	if (!window.Initialize(hInstance, nCmdShow, DefaultWidth, DefaultHeight, AppTitle))
	{
		MessageBoxW(nullptr, L"창 생성에 실패했습니다.", AppTitle, MB_OK | MB_ICONERROR);
		return -1;
	}

	GraphicsCore graphics;
	if (!graphics.Initialize(window.GetHwnd(), window.GetClientWidth(), window.GetClientHeight()))
	{
		MessageBoxW(nullptr, L"DirectX 12 초기화에 실패했습니다.", AppTitle, MB_OK | MB_ICONERROR);
		return -1;
	}

	// 폰트 아틀라스를 구우려면 화면에 그릴 문자 집합이 먼저 필요하다.
	MenuScene menu;

	TextRenderer text;
	if (!text.Initialize(graphics, L"맑은 고딕", FontHeight, menu.BuildCharset() + HudHint))
	{
		MessageBoxW(nullptr, L"텍스트 렌더러 초기화에 실패했습니다.", AppTitle, MB_OK | MB_ICONERROR);
		return -1;
	}

	SampleId currentSample = SampleId::None;   // None 이면 메인 메뉴 화면
	bool wireframe = false;

	window.SetKeyDownHandler([&](WPARAM key)
	{
		switch (key)
		{
		case VK_ESCAPE:
			if (currentSample == SampleId::None)
			{
				PostQuitMessage(0);                 // 메뉴에서 ESC 는 종료
			}
			else
			{
				currentSample = SampleId::None;     // 샘플에서 ESC 는 메뉴 복귀
			}
			return;

		case 'W':
			wireframe = !wireframe;
			return;

		default:
			if (currentSample == SampleId::None)
			{
				menu.OnKeyDown(key);
			}
			return;
		}
	});

	window.SetMouseMoveHandler([&](int x, int y)
	{
		if (currentSample == SampleId::None)
		{
			menu.OnMouseMove(x, y);
		}
	});

	window.SetMouseDownHandler([&](int x, int y)
	{
		if (currentSample == SampleId::None)
		{
			menu.OnMouseDown(x, y);
		}
	});

	LARGE_INTEGER frequency = {};
	LARGE_INTEGER previousTime = {};
	QueryPerformanceFrequency(&frequency);
	QueryPerformanceCounter(&previousTime);

	float titleTimer = 0.0f;
	UINT framesInInterval = 0;

	MSG msg = {};
	bool running = true;

	while (running)
	{
		while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE))
		{
			if (msg.message == WM_QUIT)
			{
				running = false;
				break;
			}

			TranslateMessage(&msg);
			DispatchMessageW(&msg);
		}

		if (!running)
		{
			break;
		}

		if (window.IsMinimized() || window.IsResizing())
		{
			Sleep(16);
			continue;
		}

		LARGE_INTEGER currentTime = {};
		QueryPerformanceCounter(&currentTime);
		const float deltaTime =
			static_cast<float>(currentTime.QuadPart - previousTime.QuadPart) /
			static_cast<float>(frequency.QuadPart);
		previousTime = currentTime;

		if (window.ConsumeSizeChanged())
		{
			const UINT width = window.GetClientWidth();
			const UINT height = window.GetClientHeight();
			if (width > 0 && height > 0)
			{
				graphics.Resize(width, height);
			}
		}

		if (const SampleId picked = menu.ConsumeSelection(); picked != SampleId::None)
		{
			currentSample = picked;
			// TODO: 선택된 샘플 씬 로드 / 리소스 생성
		}

		// 타이틀바는 매 프레임 갱신하면 낭비이므로 0.5초 간격으로만 쓴다.
		++framesInInterval;
		titleTimer += deltaTime;
		if (titleTimer >= 0.5f)
		{
			wchar_t titleBuffer[256] = {};
			swprintf_s(titleBuffer,
				L"[%s] | Wireframe: %s | FPS: %.1f",
				MenuScene::GetSampleName(currentSample),
				wireframe ? L"ON" : L"OFF",
				static_cast<float>(framesInInterval) / titleTimer);
			window.SetTitle(titleBuffer);

			framesInInterval = 0;
			titleTimer = 0.0f;
		}

		graphics.BeginFrame();

		if (currentSample != SampleId::None)
		{
			// TODO: graphics.GetCommandList() 로 지형 샘플 렌더링
		}

		text.Begin();

		if (currentSample == SampleId::None)
		{
			menu.Render(text, graphics.GetWidth(), graphics.GetHeight());
		}
		else
		{
			text.Draw(MenuScene::GetSampleName(currentSample), 20.0f, 20.0f, HudColor);
			text.Draw(HudHint, 20.0f, 20.0f + text.GetLineHeight() + 4.0f, HintColor);
		}

		text.Record(graphics.GetCommandList(), graphics.GetFrameIndex(),
			graphics.GetWidth(), graphics.GetHeight());

		graphics.EndFrame();
	}

	graphics.WaitForGpu();
	return static_cast<int>(msg.wParam);
}
