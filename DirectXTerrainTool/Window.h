#pragma once
#include "Common.h"

#include <functional>
#include <string>

// Win32 창의 생성과 메시지 처리를 감싼다.
// 키보드와 마우스 입력은 직접 해석하지 않고 콜백으로 넘겨서,
// 메뉴 화면이냐 샘플 화면이냐에 따라 main 이 다른 대상에게 전달할 수 있게 한다.
class Window
{
public:
	Window() = default;
	~Window();

	Window(const Window&) = delete;
	Window& operator=(const Window&) = delete;

	// width/height 는 테두리를 뺀 실제 그리기 영역(클라이언트 영역) 크기다.
	bool Initialize(HINSTANCE hInstance, int nCmdShow, UINT width, UINT height, const wchar_t* title);
	void Show(int nCmdShow) const;

	HWND GetHwnd() const { return m_hWnd; }
	HINSTANCE GetInstance() const { return m_hInstance; }
	UINT GetClientWidth() const { return m_clientWidth; }
	UINT GetClientHeight() const { return m_clientHeight; }

	// 최소화 상태이거나 크기 조절 드래그 중이면 렌더링을 건너뛰는 데 쓴다.
	bool IsMinimized() const { return m_minimized; }
	bool IsResizing() const { return m_resizing; }

	// 크기 변경이 있었는지 확인하고 플래그를 내린다.
	// 한 번의 변경에 대해 스왑체인 재생성이 한 번만 일어나도록 하기 위함이다.
	bool ConsumeSizeChanged();

	void SetKeyDownHandler(std::function<void(WPARAM)> handler) { m_onKeyDown = std::move(handler); }
	void SetMouseMoveHandler(std::function<void(int, int)> handler) { m_onMouseMove = std::move(handler); }
	void SetMouseDownHandler(std::function<void(int, int)> handler) { m_onMouseDown = std::move(handler); }

	void SetTitle(const std::wstring& title) const { SetWindowTextW(m_hWnd, title.c_str()); }

private:
	// Win32 는 C 함수 포인터만 받으므로 정적 함수를 창 프로시저로 등록하고,
	// 거기서 인스턴스를 찾아 아래 HandleMessage 로 넘긴다.
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

	// 크기 조절 드래그가 진행 중인지. 드래그 도중에는 스왑체인을 다시 만들지 않는다.
	bool m_resizing = false;

	// 크기 변경이 확정되었고 아직 처리되지 않았음을 뜻한다.
	bool m_sizeChanged = false;

	std::function<void(WPARAM)> m_onKeyDown;
	std::function<void(int, int)> m_onMouseMove;
	std::function<void(int, int)> m_onMouseDown;
};
