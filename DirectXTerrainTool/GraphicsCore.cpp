#include "GraphicsCore.h"
#include <stdexcept>

namespace
{
	inline void ThrowIfFailed(HRESULT hr)
	{
		if (FAILED(hr))
		{
			throw std::runtime_error("DirectX 12 API call failed.");
		}
	}

	void GetHardwareAdapter(IDXGIFactory6* factory, IDXGIAdapter1** adapter, bool requestHighPerformance)
	{
		*adapter = nullptr;
		ComPtr<IDXGIAdapter1> candidate;

		const DXGI_GPU_PREFERENCE preference = requestHighPerformance
			? DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE
			: DXGI_GPU_PREFERENCE_UNSPECIFIED;

		for (UINT adapterIndex = 0;
			SUCCEEDED(factory->EnumAdapterByGpuPreference(adapterIndex, preference, IID_PPV_ARGS(&candidate)));
			++adapterIndex)
		{
			DXGI_ADAPTER_DESC1 desc = {};
			candidate->GetDesc1(&desc);

			if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
			{
				continue;
			}

			if (SUCCEEDED(D3D12CreateDevice(candidate.Get(), D3D_FEATURE_LEVEL_11_0, _uuidof(ID3D12Device), nullptr)))
			{
				*adapter = candidate.Detach();
				return;
			}
		}
	}
}

GraphicsCore::~GraphicsCore()
{
	if (m_initialized)
	{
		WaitForGpu();
	}

	if (m_fenceEvent)
	{
		CloseHandle(m_fenceEvent);
		m_fenceEvent = nullptr;
	}
}

bool GraphicsCore::Initialize(HWND hWnd, UINT width, UINT height)
{
	if (!hWnd || width == 0 || height == 0)
	{
		return false;
	}

	m_hWnd = hWnd;
	m_width = width;
	m_height = height;
	m_viewport = { 0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height), 0.0f, 1.0f };
	m_scissorRect = { 0, 0, static_cast<LONG>(width), static_cast<LONG>(height) };

	try
	{
		CreateDevice();
		CreateCommandObjects();
		CreateSwapChain(hWnd);
		CreateRtvAndDsvHeaps();
		CreateFrameResources();
		CreateDepthStencil();

		ThrowIfFailed(m_device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_fence)));
		m_fenceValue = 1;
		m_fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
		if (!m_fenceEvent)
		{
			ThrowIfFailed(HRESULT_FROM_WIN32(GetLastError()));
		}

		m_frameIndex = m_swapChain->GetCurrentBackBufferIndex();
		m_initialized = true;
		return true;
	}
	catch (...)
	{
		return false;
	}
}

void GraphicsCore::Resize(UINT width, UINT height)
{
	if (!m_initialized || width == 0 || height == 0 || (width == m_width && height == m_height))
	{
		return;
	}

	WaitForGpu();

	for (UINT i = 0; i < FrameCount; ++i)
	{
		m_renderTargets[i].Reset();
		m_frameFenceValues[i] = m_frameFenceValues[m_frameIndex];
	}
	m_depthStencil.Reset();

	DXGI_SWAP_CHAIN_DESC1 desc = {};
	ThrowIfFailed(m_swapChain->GetDesc1(&desc));
	ThrowIfFailed(m_swapChain->ResizeBuffers(FrameCount, width, height, desc.Format, desc.Flags));

	m_width = width;
	m_height = height;
	m_viewport = { 0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height), 0.0f, 1.0f };
	m_scissorRect = { 0, 0, static_cast<LONG>(width), static_cast<LONG>(height) };
	m_frameIndex = m_swapChain->GetCurrentBackBufferIndex();

	CreateFrameResources();
	CreateDepthStencil();
}

void GraphicsCore::BeginFrame()
{
	ThrowIfFailed(m_commandAllocators[m_frameIndex]->Reset());
	ThrowIfFailed(m_commandList->Reset(m_commandAllocators[m_frameIndex].Get(), nullptr));

	const CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
		m_renderTargets[m_frameIndex].Get(),
		D3D12_RESOURCE_STATE_PRESENT,
		D3D12_RESOURCE_STATE_RENDER_TARGET);
	m_commandList->ResourceBarrier(1, &barrier);

	const D3D12_CPU_DESCRIPTOR_HANDLE rtv = GetCurrentRtv();
	const D3D12_CPU_DESCRIPTOR_HANDLE dsv = GetDsv();
	m_commandList->OMSetRenderTargets(1, &rtv, FALSE, &dsv);
	m_commandList->RSSetViewports(1, &m_viewport);
	m_commandList->RSSetScissorRects(1, &m_scissorRect);

	constexpr float clearColor[] = { 0.06f, 0.20f, 0.45f, 1.0f };
	m_commandList->ClearRenderTargetView(rtv, clearColor, 0, nullptr);
	m_commandList->ClearDepthStencilView(dsv, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
}

void GraphicsCore::EndFrame()
{
	const CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
		m_renderTargets[m_frameIndex].Get(),
		D3D12_RESOURCE_STATE_RENDER_TARGET,
		D3D12_RESOURCE_STATE_PRESENT);
	m_commandList->ResourceBarrier(1, &barrier);

	ThrowIfFailed(m_commandList->Close());

	ID3D12CommandList* commandLists[] = { m_commandList.Get() };
	m_commandQueue->ExecuteCommandLists(_countof(commandLists), commandLists);

	const UINT presentFlags = m_tearingSupported ? DXGI_PRESENT_ALLOW_TEARING : 0;
	ThrowIfFailed(m_swapChain->Present(0, presentFlags));

	MoveToNextFrame();
}

void GraphicsCore::WaitForGpu()
{
	ThrowIfFailed(m_commandQueue->Signal(m_fence.Get(), m_fenceValue));

	ThrowIfFailed(m_fence->SetEventOnCompletion(m_fenceValue, m_fenceEvent));
	WaitForSingleObject(m_fenceEvent, INFINITE);

	++m_fenceValue;
}

D3D12_CPU_DESCRIPTOR_HANDLE GraphicsCore::GetCurrentRtv() const
{
	return CD3DX12_CPU_DESCRIPTOR_HANDLE(
		m_rtvHeap->GetCPUDescriptorHandleForHeapStart(),
		static_cast<INT>(m_frameIndex),
		m_rtvDescriptorSize);
}

D3D12_CPU_DESCRIPTOR_HANDLE GraphicsCore::GetDsv() const
{
	return m_dsvHeap->GetCPUDescriptorHandleForHeapStart();
}

void GraphicsCore::CreateDevice()
{
	UINT dxgiFactoryFlags = 0;

#if defined(_DEBUG)
	// Graphics Tools 선택적 기능이 없는 PC 에서는 디버그 계층을 얻을 수 없다.
	// 그 상태로 DXGI_CREATE_FACTORY_DEBUG 를 넘기면 팩토리 생성 자체가 실패한다.
	if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&m_debugController))))
	{
		m_debugController->EnableDebugLayer();
		dxgiFactoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
	}
#endif

	ThrowIfFailed(CreateDXGIFactory2(dxgiFactoryFlags, IID_PPV_ARGS(&m_factory)));

	BOOL allowTearing = FALSE;
	if (SUCCEEDED(m_factory->CheckFeatureSupport(DXGI_FEATURE_PRESENT_ALLOW_TEARING, &allowTearing, sizeof(allowTearing))))
	{
		m_tearingSupported = allowTearing == TRUE;
	}

	ComPtr<IDXGIAdapter1> adapter;
	if (m_useWarpDevice)
	{
		ThrowIfFailed(m_factory->EnumWarpAdapter(IID_PPV_ARGS(&adapter)));
	}
	else
	{
		GetHardwareAdapter(m_factory.Get(), &adapter, true);
		if (!adapter)
		{
			ThrowIfFailed(m_factory->EnumWarpAdapter(IID_PPV_ARGS(&adapter)));
		}
	}

	ThrowIfFailed(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&m_device)));
}

void GraphicsCore::CreateCommandObjects()
{
	D3D12_COMMAND_QUEUE_DESC queueDesc = {};
	queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
	queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
	ThrowIfFailed(m_device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&m_commandQueue)));

	for (UINT i = 0; i < FrameCount; ++i)
	{
		ThrowIfFailed(m_device->CreateCommandAllocator(
			D3D12_COMMAND_LIST_TYPE_DIRECT,
			IID_PPV_ARGS(&m_commandAllocators[i])));
	}

	ThrowIfFailed(m_device->CreateCommandList(
		0,
		D3D12_COMMAND_LIST_TYPE_DIRECT,
		m_commandAllocators[0].Get(),
		nullptr,
		IID_PPV_ARGS(&m_commandList)));
	ThrowIfFailed(m_commandList->Close());
}

void GraphicsCore::CreateSwapChain(HWND hWnd)
{
	DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
	swapChainDesc.BufferCount = FrameCount;
	swapChainDesc.Width = m_width;
	swapChainDesc.Height = m_height;
	swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	swapChainDesc.SampleDesc.Count = 1;
	swapChainDesc.Flags = m_tearingSupported ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0;

	ComPtr<IDXGISwapChain1> swapChain;
	ThrowIfFailed(m_factory->CreateSwapChainForHwnd(
		m_commandQueue.Get(),
		hWnd,
		&swapChainDesc,
		nullptr,
		nullptr,
		&swapChain));

	ThrowIfFailed(m_factory->MakeWindowAssociation(hWnd, DXGI_MWA_NO_ALT_ENTER));
	ThrowIfFailed(swapChain.As(&m_swapChain));
}

void GraphicsCore::CreateRtvAndDsvHeaps()
{
	D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
	rtvHeapDesc.NumDescriptors = FrameCount;
	rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
	rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	ThrowIfFailed(m_device->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&m_rtvHeap)));
	m_rtvDescriptorSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

	D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc = {};
	dsvHeapDesc.NumDescriptors = 1;
	dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
	dsvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	ThrowIfFailed(m_device->CreateDescriptorHeap(&dsvHeapDesc, IID_PPV_ARGS(&m_dsvHeap)));
}

void GraphicsCore::CreateFrameResources()
{
	CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(m_rtvHeap->GetCPUDescriptorHandleForHeapStart());

	for (UINT i = 0; i < FrameCount; ++i)
	{
		ThrowIfFailed(m_swapChain->GetBuffer(i, IID_PPV_ARGS(&m_renderTargets[i])));
		m_device->CreateRenderTargetView(m_renderTargets[i].Get(), nullptr, rtvHandle);
		rtvHandle.Offset(1, m_rtvDescriptorSize);
	}
}

void GraphicsCore::CreateDepthStencil()
{
	D3D12_CLEAR_VALUE clearValue = {};
	clearValue.Format = DXGI_FORMAT_D32_FLOAT;
	clearValue.DepthStencil.Depth = 1.0f;
	clearValue.DepthStencil.Stencil = 0;

	const CD3DX12_HEAP_PROPERTIES heapProps(D3D12_HEAP_TYPE_DEFAULT);
	const CD3DX12_RESOURCE_DESC depthDesc = CD3DX12_RESOURCE_DESC::Tex2D(
		DXGI_FORMAT_D32_FLOAT,
		m_width,
		m_height,
		1,
		0,
		1,
		0,
		D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL);

	ThrowIfFailed(m_device->CreateCommittedResource(
		&heapProps,
		D3D12_HEAP_FLAG_NONE,
		&depthDesc,
		D3D12_RESOURCE_STATE_DEPTH_WRITE,
		&clearValue,
		IID_PPV_ARGS(&m_depthStencil)));

	D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
	dsvDesc.Format = DXGI_FORMAT_D32_FLOAT;
	dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
	dsvDesc.Flags = D3D12_DSV_FLAG_NONE;
	m_device->CreateDepthStencilView(m_depthStencil.Get(), &dsvDesc, m_dsvHeap->GetCPUDescriptorHandleForHeapStart());
}

void GraphicsCore::MoveToNextFrame()
{
	const UINT64 currentFenceValue = m_fenceValue;
	ThrowIfFailed(m_commandQueue->Signal(m_fence.Get(), currentFenceValue));
	m_frameFenceValues[m_frameIndex] = currentFenceValue;
	++m_fenceValue;

	m_frameIndex = m_swapChain->GetCurrentBackBufferIndex();

	if (m_fence->GetCompletedValue() < m_frameFenceValues[m_frameIndex])
	{
		ThrowIfFailed(m_fence->SetEventOnCompletion(m_frameFenceValues[m_frameIndex], m_fenceEvent));
		WaitForSingleObject(m_fenceEvent, INFINITE);
	}
}
