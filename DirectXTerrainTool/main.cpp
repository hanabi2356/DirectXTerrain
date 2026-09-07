#include "Window.h"
#include "GraphicsCore.h"
#include "TextRenderer.h"
#include "MenuScene.h"
#include "SceneManager.h"
#include "Camera.h"

#include <cstdio>

namespace
{
	constexpr UINT DefaultWidth = 1280;
	constexpr UINT DefaultHeight = 720;
	constexpr int FontHeight = 18;
	constexpr const wchar_t* AppTitle = L"DirectX Terrain Tool";

	const std::wstring HudHint =
		L"ESC: 메뉴로   F1: 와이어프레임   우클릭 드래그: 시점   WASD / Q,E: 이동   Shift: 가속";
	const std::wstring NotReadyText =
		L"이 샘플은 아직 준비 중입니다. ESC 를 눌러 메뉴로 돌아가세요.";

	const DirectX::XMFLOAT4 HudColor = { 0.92f, 0.94f, 1.00f, 1.0f };
	const DirectX::XMFLOAT4 HintColor = { 0.70f, 0.78f, 0.95f, 1.0f };
	const DirectX::XMFLOAT4 WarnColor = { 1.00f, 0.72f, 0.42f, 1.0f };

	float AspectOf(const GraphicsCore& graphics)
	{
		return static_cast<float>(graphics.GetWidth()) / static_cast<float>(graphics.GetHeight());
	}
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
	if (!text.Initialize(graphics, L"맑은 고딕", FontHeight,
			menu.BuildCharset() + HudHint + NotReadyText))
	{
		MessageBoxW(nullptr, L"텍스트 렌더러 초기화에 실패했습니다.", AppTitle, MB_OK | MB_ICONERROR);
		return -1;
	}

	SceneManager scenes;
	Camera camera;
	camera.SetPerspective(DirectX::XM_PIDIV4, AspectOf(graphics), 0.5f, 2000.0f);
	camera.LookAt({ 0.0f, 0.0f, 0.0f });

	bool inMenu = true;
	bool wireframe = false;
	bool sampleReady = false;

	window.SetKeyDownHandler([&](WPARAM key)
	{
		if (key == VK_ESCAPE)
		{
			if (inMenu)
			{
				PostQuitMessage(0);
			}
			else
			{
				scenes.Clear(graphics);
				inMenu = true;
				sampleReady = false;
			}
			return;
		}

		if (key == VK_F1)
		{
			wireframe = !wireframe;
			return;
		}

		if (inMenu)
		{
			menu.OnKeyDown(key);
		}
	});

	window.SetMouseMoveHandler([&](int x, int y)
	{
		if (inMenu)
		{
			menu.OnMouseMove(x, y);
		}
		else
		{
			camera.OnMouseMove(x, y);
		}
	});

	window.SetMouseDownHandler([&](int x, int y)
	{
		if (inMenu)
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
				camera.SetAspect(AspectOf(graphics));
			}
		}

		if (const SampleId picked = menu.ConsumeSelection(); picked != SampleId::None)
		{
			sampleReady = scenes.Switch(graphics, picked);
			inMenu = false;

			camera.SetPosition({ 0.0f, 45.0f, -95.0f });
			camera.LookAt({ 0.0f, 0.0f, 0.0f });
			camera.EndDrag();
		}

		if (!inMenu)
		{
			camera.Update(deltaTime);
			scenes.Update(deltaTime);
		}

		// 타이틀바는 매 프레임 갱신하면 낭비이므로 0.5초 간격으로만 쓴다.
		++framesInInterval;
		titleTimer += deltaTime;
		if (titleTimer >= 0.5f)
		{
			wchar_t titleBuffer[256] = {};
			swprintf_s(titleBuffer,
				L"[%s] | Wireframe: %s | FPS: %.1f",
				MenuScene::GetSampleName(inMenu ? SampleId::None : scenes.GetCurrent()),
				wireframe ? L"ON" : L"OFF",
				static_cast<float>(framesInInterval) / titleTimer);
			window.SetTitle(titleBuffer);

			framesInInterval = 0;
			titleTimer = 0.0f;
		}

		graphics.BeginFrame();

		if (!inMenu)
		{
			scenes.Render(graphics, camera, wireframe);
		}

		text.Begin();

		if (inMenu)
		{
			menu.Render(text, graphics.GetWidth(), graphics.GetHeight());
		}
		else
		{
			const float lineHeight = text.GetLineHeight();
			text.Draw(MenuScene::GetSampleName(scenes.GetCurrent()), 20.0f, 20.0f, HudColor);
			text.Draw(HudHint, 20.0f, 20.0f + lineHeight + 4.0f, HintColor);

			if (!sampleReady)
			{
				text.Draw(NotReadyText, 20.0f, 20.0f + (lineHeight + 4.0f) * 2.0f, WarnColor);
			}
		}

		text.Record(graphics.GetCommandList(), graphics.GetFrameIndex(),
			graphics.GetWidth(), graphics.GetHeight());

		graphics.EndFrame();
	}

	scenes.Clear(graphics);
	graphics.WaitForGpu();
	return static_cast<int>(msg.wParam);
}
