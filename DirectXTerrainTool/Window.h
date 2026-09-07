#pragma once
#include "Common.h"

#include <functional>
#include <string>

class Window
{
public:
	Window() = default;
	~Window();

	Window(const Window&) = delete;
	Window& operator=(const Window&) = delete;

	bool Initialize(HINSTANCE hInstance, int nCmdShow, UINT width, UINT height, const wchar_t* title);
	void Show(int nCmdShow) const;

	HWND GetHwnd() const { return m_hWnd; }
	HINSTANCE GetInstance() const { return m_hInstance; }
	UINT GetClientWidth() const { return m_clientWidth; }
	UINT GetClientHeight() const { return m_clientHeight; }
	bool IsMinimized() const { return m_minimized; }
	bool IsResizing() const { return m_resizing; }
	bool ConsumeSizeChanged();

	void SetKeyDownHandler(std::function<void(WPARAM)> handler) { m_onKeyDown = std::move(handler); }
	void SetMouseMoveHandler(std::function<void(int, int)> handler) { m_onMouseMove = std::move(handler); }
	void SetMouseDownHandler(std::function<void(int, int)> handler) { m_onMouseDown = std::move(handler); }
	void SetTitle(const std::wstring& title) const { SetWindowTextW(m_hWnd, title.c_str()); }

private:
	static LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
	LRESULT HandleMessage(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
	bool RegisterWindowClass();
	bool CreateAppWindow(UINT width, UINT height, const wchar_t* title);
	void UpdateClientSize();

	static constexpr const wchar_t* ClassName = L"DirectXTerrainToolWindow";

	HINSTANCE m_hInstance = nullptr;
	HWND m_hWnd = nullptr;
	UINT m_clientWidth = 0;
	UINT m_clientHeight = 0;
	bool m_minimized = false;
	bool m_maximized = false;
	bool m_resizing = false;
	bool m_sizeChanged = false;

	std::function<void(WPARAM)> m_onKeyDown;
	std::function<void(int, int)> m_onMouseMove;
	std::function<void(int, int)> m_onMouseDown;
};
