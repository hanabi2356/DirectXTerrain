#pragma once
#include "Common.h"

// DirectX 12 의 기반 객체를 한곳에서 소유한다.
// 디바이스, 커맨드 큐/리스트, 스왑체인, 렌더 타겟과 깊이 버퍼, 그리고 CPU-GPU 동기화까지.
// 지형이나 UI 같은 "무엇을 그릴지"는 전혀 모르고, "그릴 수 있는 환경"만 책임진다.
class GraphicsCore
{
public:
	// 백버퍼 개수. 2 면 더블 버퍼링이다.
	// CPU 가 다음 프레임을 준비하는 동안 GPU 가 이전 프레임을 그릴 수 있게 해준다.
	static constexpr UINT FrameCount = 2;

	GraphicsCore() = default;
	~GraphicsCore();

	// D3D12 객체는 복사되면 안 되므로 복사를 막는다.
	GraphicsCore(const GraphicsCore&) = delete;
	GraphicsCore& operator=(const GraphicsCore&) = delete;

	bool Initialize(HWND hWnd, UINT width, UINT height);

	// 창 크기가 바뀌면 백버퍼와 깊이 버퍼를 새로 만들어야 한다.
	void Resize(UINT width, UINT height);

	// BeginFrame - (그리기 명령 기록) - EndFrame 순서로 한 프레임을 구성한다.
	void BeginFrame();
	void EndFrame();

	// GPU 가 현재까지 제출된 모든 작업을 끝낼 때까지 CPU 를 멈춘다.
	// 리소스를 해제하기 직전이나 종료 시점에 부른다.
	void WaitForGpu();

	ID3D12Device* GetDevice() const { return m_device.Get(); }
	ID3D12GraphicsCommandList* GetCommandList() const { return m_commandList.Get(); }
	ID3D12CommandQueue* GetCommandQueue() const { return m_commandQueue.Get(); }
	ID3D12Resource* GetBackBuffer(UINT index) const { return m_renderTargets[index].Get(); }

	// PSO 를 만들 때 렌더 타겟 포맷이 스왑체인과 일치해야 해서 밖으로 노출한다.
	DXGI_FORMAT GetBackBufferFormat() const { return DXGI_FORMAT_R8G8B8A8_UNORM; }

	D3D12_CPU_DESCRIPTOR_HANDLE GetCurrentRtv() const;
	D3D12_CPU_DESCRIPTOR_HANDLE GetDsv() const;

	UINT GetWidth() const { return m_width; }
	UINT GetHeight() const { return m_height; }

	// 지금 그리고 있는 백버퍼의 번호. 프레임마다 번갈아 바뀐다.
	// 프레임별로 따로 둔 자원(상수 버퍼 등)을 고를 때 쓴다.
	UINT GetFrameIndex() const { return m_frameIndex; }

	const D3D12_VIEWPORT& GetViewport() const { return m_viewport; }
	const D3D12_RECT& GetScissorRect() const { return m_scissorRect; }

private:
	// Initialize 가 아래 순서대로 호출한다. 앞의 것이 뒤의 것의 전제가 된다.
	void CreateDevice();
	void CreateCommandObjects();
	void CreateSwapChain(HWND hWnd);
	void CreateRtvAndDsvHeaps();
	void CreateFrameResources();
	void CreateDepthStencil();

	// 다음 프레임으로 넘어가면서, 그 백버퍼를 쓰던 이전 작업이 끝났는지만 확인한다.
	void MoveToNextFrame();

	HWND m_hWnd = nullptr;
	UINT m_width = 0;
	UINT m_height = 0;
	UINT m_frameIndex = 0;

	// RTV 서술자 하나의 바이트 크기. GPU 마다 달라서 실행 중에 물어봐야 한다.
	UINT m_rtvDescriptorSize = 0;

	// 다음에 사용할 펜스 값. 신호를 보낼 때마다 증가한다.
	UINT64 m_fenceValue = 0;

	// 각 백버퍼에 마지막으로 붙인 펜스 값. 그 프레임을 재사용해도 되는지 판단하는 기준이다.
	UINT64 m_frameFenceValues[FrameCount] = {};

	// 하드웨어 GPU 대신 소프트웨어 렌더러(WARP)를 강제할 때 쓴다. 디버깅용.
	bool m_useWarpDevice = false;

	// 가변 주사율(티어링) 지원 여부. 지원되면 Present 에서 수직 동기화를 건너뛸 수 있다.
	bool m_tearingSupported = false;

	bool m_initialized = false;

	D3D12_VIEWPORT m_viewport = {};
	D3D12_RECT m_scissorRect = {};

	ComPtr<IDXGIFactory6> m_factory;
	ComPtr<ID3D12Device> m_device;

	// 커맨드 큐는 GPU 에 작업을 제출하는 창구다.
	ComPtr<ID3D12CommandQueue> m_commandQueue;

	// 할당자는 GPU 가 다 읽기 전에 재사용하면 안 되므로 프레임 수만큼 둔다.
	ComPtr<ID3D12CommandAllocator> m_commandAllocators[FrameCount];

	// 커맨드 리스트는 매 프레임 Reset 으로 되감아 재사용하므로 하나면 된다.
	ComPtr<ID3D12GraphicsCommandList> m_commandList;

	ComPtr<IDXGISwapChain3> m_swapChain;

	// 서술자 힙은 뷰(RTV/DSV)를 담아두는 GPU 메모리 배열이다.
	ComPtr<ID3D12DescriptorHeap> m_rtvHeap;
	ComPtr<ID3D12DescriptorHeap> m_dsvHeap;

	// 스왑체인이 소유한 백버퍼들. 직접 만들지 않고 GetBuffer 로 받아온다.
	ComPtr<ID3D12Resource> m_renderTargets[FrameCount];

	// 깊이 버퍼는 하나만 있으면 된다. 프레임 안에서만 쓰이고 다음 프레임에 다시 지워지기 때문이다.
	ComPtr<ID3D12Resource> m_depthStencil;

	// 펜스는 GPU 가 어디까지 진행했는지 CPU 가 확인하는 수단이다.
	ComPtr<ID3D12Fence> m_fence;
	HANDLE m_fenceEvent = nullptr;

#if defined(_DEBUG)
	ComPtr<ID3D12Debug> m_debugController;
#endif
};
