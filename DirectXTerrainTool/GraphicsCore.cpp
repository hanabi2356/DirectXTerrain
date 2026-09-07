#include "GraphicsCore.h"
#include <stdexcept>

namespace
{
	// D3D12 함수는 대부분 HRESULT 를 돌려준다. 매번 if 로 검사하면 초기화 코드가 읽기 어려워지므로
	// 예외로 바꿔 던지고, Initialize 한곳에서 catch 해 false 로 변환한다.
	inline void ThrowIfFailed(HRESULT hr)
	{
		if (FAILED(hr))
		{
			throw std::runtime_error("DirectX 12 API call failed.");
		}
	}

	// 쓸 만한 GPU 를 하나 고른다.
	// 노트북처럼 내장 GPU 와 외장 GPU 가 함께 있는 환경에서 느린 쪽이 잡히는 것을 막기 위해
	// 고성능 우선으로 열거하고, 실제로 디바이스를 만들 수 있는지까지 확인한다.
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

			// 소프트웨어 어댑터는 여기서 제외한다. 실패했을 때 마지막 수단으로만 쓴다.
			if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
			{
				continue;
			}

			// 마지막 인자를 nullptr 로 주면 실제로 만들지 않고 가능 여부만 확인한다.
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
	// GPU 가 아직 리소스를 쓰고 있는데 해제하면 크래시가 난다.
	// ComPtr 들이 풀리기 전에 반드시 GPU 를 기다려야 한다.
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
		// 순서가 중요하다. 디바이스가 있어야 커맨드 큐를 만들 수 있고,
		// 커맨드 큐가 있어야 스왑체인을 만들 수 있고, 스왑체인이 있어야 백버퍼를 얻는다.
		CreateDevice();
		CreateCommandObjects();
		CreateSwapChain(hWnd);
		CreateRtvAndDsvHeaps();
		CreateFrameResources();
		CreateDepthStencil();

		// 펜스는 0 에서 시작하고, 앞으로 보낼 신호값은 1 부터 쓴다.
		ThrowIfFailed(m_device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_fence)));
		m_fenceValue = 1;

		// 펜스가 목표값에 도달하면 이 이벤트가 켜지고, CPU 는 그때까지 잠든다.
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

	// 백버퍼를 놓기 전에 GPU 가 그것들을 다 쓰고 났는지 확인한다.
	WaitForGpu();

	// ResizeBuffers 는 백버퍼에 대한 참조가 하나도 남아 있지 않아야 성공한다.
	for (UINT i = 0; i < FrameCount; ++i)
	{
		m_renderTargets[i].Reset();
		m_frameFenceValues[i] = m_frameFenceValues[m_frameIndex];
	}
	m_depthStencil.Reset();

	// 포맷과 플래그는 처음 만들 때와 같아야 하므로 기존 설정을 그대로 읽어서 넘긴다.
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
	// 이 프레임의 할당자는 MoveToNextFrame 에서 이미 사용 완료를 확인했으므로 되감아도 안전하다.
	ThrowIfFailed(m_commandAllocators[m_frameIndex]->Reset());
	ThrowIfFailed(m_commandList->Reset(m_commandAllocators[m_frameIndex].Get(), nullptr));

	// 백버퍼는 화면에 표시되는 상태(PRESENT)로 있다. 그리려면 렌더 타겟 상태로 바꿔야 한다.
	// 이런 상태 전환을 D3D12 에서는 리소스 배리어로 명시한다.
	const CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
		m_renderTargets[m_frameIndex].Get(),
		D3D12_RESOURCE_STATE_PRESENT,
		D3D12_RESOURCE_STATE_RENDER_TARGET);
	m_commandList->ResourceBarrier(1, &barrier);

	// 어디에 그릴지(렌더 타겟, 깊이 버퍼)와 화면의 어느 영역에 그릴지를 지정한다.
	const D3D12_CPU_DESCRIPTOR_HANDLE rtv = GetCurrentRtv();
	const D3D12_CPU_DESCRIPTOR_HANDLE dsv = GetDsv();
	m_commandList->OMSetRenderTargets(1, &rtv, FALSE, &dsv);
	m_commandList->RSSetViewports(1, &m_viewport);
	m_commandList->RSSetScissorRects(1, &m_scissorRect);

	// 이전 프레임의 잔상을 지운다. 깊이는 1.0(가장 먼 값)으로 초기화한다.
	constexpr float clearColor[] = { 0.06f, 0.20f, 0.45f, 1.0f };
	m_commandList->ClearRenderTargetView(rtv, clearColor, 0, nullptr);
	m_commandList->ClearDepthStencilView(dsv, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
}

void GraphicsCore::EndFrame()
{
	// 그리기가 끝났으니 다시 표시 가능한 상태로 되돌린다.
	const CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
		m_renderTargets[m_frameIndex].Get(),
		D3D12_RESOURCE_STATE_RENDER_TARGET,
		D3D12_RESOURCE_STATE_PRESENT);
	m_commandList->ResourceBarrier(1, &barrier);

	// 여기까지는 명령을 "기록"만 한 것이다. Close 로 기록을 끝내고 큐에 제출해야 GPU 가 실행한다.
	ThrowIfFailed(m_commandList->Close());

	ID3D12CommandList* commandLists[] = { m_commandList.Get() };
	m_commandQueue->ExecuteCommandLists(_countof(commandLists), commandLists);

	// 첫 인자가 동기화 간격이다. 0 이면 수직 동기화를 기다리지 않는다.
	const UINT presentFlags = m_tearingSupported ? DXGI_PRESENT_ALLOW_TEARING : 0;
	ThrowIfFailed(m_swapChain->Present(0, presentFlags));

	MoveToNextFrame();
}

// 제출된 모든 작업이 끝날 때까지 완전히 멈춘다.
// MoveToNextFrame 과 달리 GPU 를 놀리게 되므로 매 프레임 쓰면 안 되고,
// 종료나 리소스 해제처럼 확실한 정지가 필요할 때만 쓴다.
void GraphicsCore::WaitForGpu()
{
	// 큐의 맨 뒤에 "여기까지 오면 이 값을 써라"는 표시를 남긴다.
	ThrowIfFailed(m_commandQueue->Signal(m_fence.Get(), m_fenceValue));

	// 그 값이 될 때까지 CPU 를 재운다.
	ThrowIfFailed(m_fence->SetEventOnCompletion(m_fenceValue, m_fenceEvent));
	WaitForSingleObject(m_fenceEvent, INFINITE);

	++m_fenceValue;
}

// RTV 힙에는 백버퍼 수만큼의 뷰가 나란히 들어 있다.
// 서술자 크기가 GPU 마다 달라서, 시작 주소에 (번호 x 크기)를 더해 원하는 위치를 찾는다.
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

		// 쓸 만한 GPU 가 없으면 소프트웨어 렌더러로라도 동작하게 한다.
		if (!adapter)
		{
			ThrowIfFailed(m_factory->EnumWarpAdapter(IID_PPV_ARGS(&adapter)));
		}
	}

	// 기능 수준 11_0 은 D3D12 를 지원하는 GPU 라면 사실상 모두 만족한다.
	ThrowIfFailed(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&m_device)));
}

void GraphicsCore::CreateCommandObjects()
{
	// DIRECT 타입은 그리기, 계산, 복사를 모두 처리할 수 있는 범용 큐다.
	D3D12_COMMAND_QUEUE_DESC queueDesc = {};
	queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
	queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
	ThrowIfFailed(m_device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&m_commandQueue)));

	// 할당자는 기록된 명령이 실제로 담기는 메모리다.
	// GPU 가 아직 그 명령을 실행 중이면 되감을 수 없으므로 프레임마다 하나씩 둔다.
	for (UINT i = 0; i < FrameCount; ++i)
	{
		ThrowIfFailed(m_device->CreateCommandAllocator(
			D3D12_COMMAND_LIST_TYPE_DIRECT,
			IID_PPV_ARGS(&m_commandAllocators[i])));
	}

	// 반면 커맨드 리스트는 Reset 으로 할당자만 바꿔 끼우면 되므로 하나로 충분하다.
	ThrowIfFailed(m_device->CreateCommandList(
		0,
		D3D12_COMMAND_LIST_TYPE_DIRECT,
		m_commandAllocators[0].Get(),
		nullptr,
		IID_PPV_ARGS(&m_commandList)));

	// 생성 직후에는 기록 중 상태다. BeginFrame 이 Reset 으로 시작하므로 일단 닫아둔다.
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

	// 플립 모델은 백버퍼를 복사하지 않고 화면과 맞바꾼다. D3D12 에서는 이 방식만 쓸 수 있다.
	swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;

	// 멀티샘플링은 쓰지 않는다. 플립 모델 스왑체인은 애초에 1 만 허용한다.
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

	// Alt+Enter 로 DXGI 가 멋대로 전체 화면 전환을 하지 않게 막는다.
	ThrowIfFailed(m_factory->MakeWindowAssociation(hWnd, DXGI_MWA_NO_ALT_ENTER));

	// GetCurrentBackBufferIndex 를 쓰려면 IDXGISwapChain3 이 필요하다.
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

// 백버퍼 자체는 스왑체인이 만들어 소유한다.
// 우리는 그것을 받아와서 "렌더 타겟으로 쓰겠다"는 뷰(RTV)만 힙에 만들어 둔다.
void GraphicsCore::CreateFrameResources()
{
	CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(m_rtvHeap->GetCPUDescriptorHandleForHeapStart());

	for (UINT i = 0; i < FrameCount; ++i)
	{
		ThrowIfFailed(m_swapChain->GetBuffer(i, IID_PPV_ARGS(&m_renderTargets[i])));
		m_device->CreateRenderTargetView(m_renderTargets[i].Get(), nullptr, rtvHandle);

		// 다음 서술자 자리로 이동한다.
		rtvHandle.Offset(1, m_rtvDescriptorSize);
	}
}

void GraphicsCore::CreateDepthStencil()
{
	// 깊이 버퍼는 지우는 값을 미리 알려주면 GPU 가 더 빠르게 처리할 수 있다.
	// 여기 적은 값과 ClearDepthStencilView 에 넘기는 값이 다르면 경고가 뜬다.
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

// 프레임을 넘기면서 필요한 만큼만 기다린다.
// 방금 제출한 작업이 끝나기를 기다리는 게 아니라, "다음에 쓸 백버퍼"를 사용하던
// 이전 작업이 끝났는지만 확인한다. 덕분에 CPU 가 한 프레임 앞서 나갈 수 있다.
void GraphicsCore::MoveToNextFrame()
{
	// 방금 제출한 작업 뒤에 표시를 남기고, 그 값을 현재 프레임의 기록으로 저장한다.
	const UINT64 currentFenceValue = m_fenceValue;
	ThrowIfFailed(m_commandQueue->Signal(m_fence.Get(), currentFenceValue));
	m_frameFenceValues[m_frameIndex] = currentFenceValue;
	++m_fenceValue;

	m_frameIndex = m_swapChain->GetCurrentBackBufferIndex();

	// 다음 백버퍼를 쓰던 작업이 아직 안 끝났을 때만 기다린다.
	if (m_fence->GetCompletedValue() < m_frameFenceValues[m_frameIndex])
	{
		ThrowIfFailed(m_fence->SetEventOnCompletion(m_frameFenceValues[m_frameIndex], m_fenceEvent));
		WaitForSingleObject(m_fenceEvent, INFINITE);
	}
}
