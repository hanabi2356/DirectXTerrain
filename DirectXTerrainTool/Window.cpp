#include "Window.h"
#include "Resource.h"
#include<windowsx.h>
Window::~Window()
{
	if (m_hWnd)
	{
		DestroyWindow(m_hWnd);
		m_hWnd = nullptr;
	}

	if (m_hInstance)
	{
		UnregisterClassW(ClassName, m_hInstance);
	}
}

bool Window::Initialize(HINSTANCE hInstance, int nCmdShow, UINT width, UINT height, const wchar_t* title)
{
	m_hInstance = hInstance;
	m_clientWidth = width;
	m_clientHeight = height;

	if (!RegisterWindowClass())
	{
		return false;
	}

	if (!CreateAppWindow(width, height, title))
	{
		return false;
	}

	Show(nCmdShow);
	return true;
}

void Window::Show(int nCmdShow) const
{
	ShowWindow(m_hWnd, nCmdShow);
	UpdateWindow(m_hWnd);
}

bool Window::ConsumeSizeChanged()
{
	const bool changed = m_sizeChanged;
	m_sizeChanged = false;
	return changed;
}

bool Window::RegisterWindowClass()
{
	WNDCLASSEXW wcex = {};
	wcex.cbSize = sizeof(WNDCLASSEXW);
	wcex.style = CS_HREDRAW | CS_VREDRAW;
	wcex.lpfnWndProc = WndProc;
	wcex.hInstance = m_hInstance;
	wcex.hIcon = LoadIconW(m_hInstance, MAKEINTRESOURCEW(IDI_DIRECTXTERRAINTOOL));
	wcex.hCursor = LoadCursorW(nullptr, IDC_ARROW);
	wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
	wcex.lpszClassName = ClassName;
	wcex.hIconSm = LoadIconW(m_hInstance, MAKEINTRESOURCEW(IDI_SMALL));

	return RegisterClassExW(&wcex) != 0;
}

bool Window::CreateAppWindow(UINT width, UINT height, const wchar_t* title)
{
	RECT windowRect = { 0, 0, static_cast<LONG>(width), static_cast<LONG>(height) };
	AdjustWindowRect(&windowRect, WS_OVERLAPPEDWINDOW, FALSE);

	const int windowWidth = windowRect.right - windowRect.left;
	const int windowHeight = windowRect.bottom - windowRect.top;

	m_hWnd = CreateWindowExW(
		0,
		ClassName,
		title,
		WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT,
		CW_USEDEFAULT,
		windowWidth,
		windowHeight,
		nullptr,
		nullptr,
		m_hInstance,
		this);

	return m_hWnd != nullptr;
}

void Window::UpdateClientSize()
{
	RECT clientRect = {};
	GetClientRect(m_hWnd, &clientRect);
	m_clientWidth = static_cast<UINT>(clientRect.right - clientRect.left);
	m_clientHeight = static_cast<UINT>(clientRect.bottom - clientRect.top);
}

LRESULT CALLBACK Window::WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	Window* window = nullptr;

	if (message == WM_NCCREATE)
	{
		const auto* createStruct = reinterpret_cast<CREATESTRUCTW*>(lParam);
		window = static_cast<Window*>(createStruct->lpCreateParams);
		SetWindowLongPtrW(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(window));
		window->m_hWnd = hWnd;
	}
	else
	{
		window = reinterpret_cast<Window*>(GetWindowLongPtrW(hWnd, GWLP_USERDATA));
	}

	if (window)
	{
		return window->HandleMessage(hWnd, message, wParam, lParam);
	}

	return DefWindowProcW(hWnd, message, wParam, lParam);
}

LRESULT Window::HandleMessage(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message)
	{
	case WM_ACTIVATE:
		if (LOWORD(wParam) == WA_INACTIVE)
		{
			// 필요 시 포커스 상실 처리
		}
		return 0;

	case WM_SIZE:
	{
		UpdateClientSize();

		if (wParam == SIZE_MINIMIZED)
		{
			m_minimized = true;
			m_maximized = false;
		}
		else if (wParam == SIZE_MAXIMIZED)
		{
			m_minimized = false;
			m_maximized = true;
			m_sizeChanged = true;
		}
		else if (wParam == SIZE_RESTORED)
		{
			if (m_minimized)
			{
				m_minimized = false;
				m_sizeChanged = true;
			}
			else if (m_maximized)
			{
				m_maximized = false;
				m_sizeChanged = true;
			}
			else if (!m_resizing)
			{
				m_sizeChanged = true;
			}
		}
		return 0;
	}

	case WM_ENTERSIZEMOVE:
		m_resizing = true;
		return 0;

	case WM_EXITSIZEMOVE:
		m_resizing = false;
		UpdateClientSize();
		m_sizeChanged = true;
		return 0;

	case WM_GETMINMAXINFO:
	{
		auto* minMaxInfo = reinterpret_cast<MINMAXINFO*>(lParam);
		minMaxInfo->ptMinTrackSize.x = 320;
		minMaxInfo->ptMinTrackSize.y = 240;
		return 0;
	}

	case WM_KEYDOWN:
	{
		if (m_onKeyDown)
		{
			m_onKeyDown(wParam);
		}
		return 0;
	}

	case WM_MOUSEMOVE:
	{
		if (m_onMouseMove)
		{
			m_onMouseMove(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
		}
		return 0;
	}

	case WM_LBUTTONDOWN:
	{
		if (m_onMouseDown)
		{
			m_onMouseDown(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
		}
		return 0;
	}

	case WM_DESTROY:
		PostQuitMessage(0);
		return 0;

	default:
		return DefWindowProcW(hWnd, message, wParam, lParam);
	}
}
