#pragma once
#include "Common.h"

class GraphicsCore
{
public:
	static constexpr UINT FrameCount = 2;

	GraphicsCore() = default;
	~GraphicsCore();

	GraphicsCore(const GraphicsCore&) = delete;
	GraphicsCore& operator=(const GraphicsCore&) = delete;

	bool Initialize(HWND hWnd, UINT width, UINT height);
	void Resize(UINT width, UINT height);
	void BeginFrame();
	void EndFrame();
	void WaitForGpu();

	ID3D12Device* GetDevice() const { return m_device.Get(); }
	ID3D12GraphicsCommandList* GetCommandList() const { return m_commandList.Get(); }
	ID3D12CommandQueue* GetCommandQueue() const { return m_commandQueue.Get(); }
	ID3D12Resource* GetBackBuffer(UINT index) const { return m_renderTargets[index].Get(); }
	DXGI_FORMAT GetBackBufferFormat() const { return DXGI_FORMAT_R8G8B8A8_UNORM; }
	D3D12_CPU_DESCRIPTOR_HANDLE GetCurrentRtv() const;
	D3D12_CPU_DESCRIPTOR_HANDLE GetDsv() const;

	UINT GetWidth() const { return m_width; }
	UINT GetHeight() const { return m_height; }
	UINT GetFrameIndex() const { return m_frameIndex; }
	const D3D12_VIEWPORT& GetViewport() const { return m_viewport; }
	const D3D12_RECT& GetScissorRect() const { return m_scissorRect; }

private:
	void CreateDevice();
	void CreateCommandObjects();
	void CreateSwapChain(HWND hWnd);
	void CreateRtvAndDsvHeaps();
	void CreateFrameResources();
	void CreateDepthStencil();
	void MoveToNextFrame();

	HWND m_hWnd = nullptr;
	UINT m_width = 0;
	UINT m_height = 0;
	UINT m_frameIndex = 0;
	UINT m_rtvDescriptorSize = 0;
	UINT64 m_fenceValue = 0;
	UINT64 m_frameFenceValues[FrameCount] = {};

	bool m_useWarpDevice = false;
	bool m_tearingSupported = false;
	bool m_initialized = false;

	D3D12_VIEWPORT m_viewport = {};
	D3D12_RECT m_scissorRect = {};

	ComPtr<IDXGIFactory6> m_factory;
	ComPtr<ID3D12Device> m_device;
	ComPtr<ID3D12CommandQueue> m_commandQueue;
	ComPtr<ID3D12CommandAllocator> m_commandAllocators[FrameCount];
	ComPtr<ID3D12GraphicsCommandList> m_commandList;
	ComPtr<IDXGISwapChain3> m_swapChain;
	ComPtr<ID3D12DescriptorHeap> m_rtvHeap;
	ComPtr<ID3D12DescriptorHeap> m_dsvHeap;
	ComPtr<ID3D12Resource> m_renderTargets[FrameCount];
	ComPtr<ID3D12Resource> m_depthStencil;
	ComPtr<ID3D12Fence> m_fence;
	HANDLE m_fenceEvent = nullptr;

#if defined(_DEBUG)
	ComPtr<ID3D12Debug> m_debugController;
#endif
};
