#pragma once
#include "Common.h"
#include "GraphicsCore.h"

// 초기화 시점에만 쓰는 일회성 업로드 헬퍼.
// 자체 커맨드 리스트와 펜스를 만들어 복사가 끝날 때까지 기다리므로,
// 호출이 반환된 뒤에는 중간 업로드 버퍼를 바로 버려도 안전하다.
inline bool CreateDefaultBuffer(
	GraphicsCore& core,
	const void* data,
	UINT64 byteSize,
	D3D12_RESOURCE_STATES finalState,
	ComPtr<ID3D12Resource>& outBuffer)
{
	if (!data || byteSize == 0)
	{
		return false;
	}

	ID3D12Device* device = core.GetDevice();
	const CD3DX12_RESOURCE_DESC bufferDesc = CD3DX12_RESOURCE_DESC::Buffer(byteSize);

	const CD3DX12_HEAP_PROPERTIES defaultHeap(D3D12_HEAP_TYPE_DEFAULT);
	if (FAILED(device->CreateCommittedResource(&defaultHeap, D3D12_HEAP_FLAG_NONE, &bufferDesc,
			D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&outBuffer))))
	{
		return false;
	}

	const CD3DX12_HEAP_PROPERTIES uploadHeap(D3D12_HEAP_TYPE_UPLOAD);
	ComPtr<ID3D12Resource> uploadBuffer;
	if (FAILED(device->CreateCommittedResource(&uploadHeap, D3D12_HEAP_FLAG_NONE, &bufferDesc,
			D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&uploadBuffer))))
	{
		return false;
	}

	void* mapped = nullptr;
	const CD3DX12_RANGE readRange(0, 0);
	if (FAILED(uploadBuffer->Map(0, &readRange, &mapped)))
	{
		return false;
	}
	memcpy(mapped, data, static_cast<size_t>(byteSize));
	uploadBuffer->Unmap(0, nullptr);

	ComPtr<ID3D12CommandAllocator> allocator;
	ComPtr<ID3D12GraphicsCommandList> commandList;
	if (FAILED(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&allocator))) ||
		FAILED(device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, allocator.Get(), nullptr,
			IID_PPV_ARGS(&commandList))))
	{
		return false;
	}

	commandList->CopyBufferRegion(outBuffer.Get(), 0, uploadBuffer.Get(), 0, byteSize);

	const CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
		outBuffer.Get(), D3D12_RESOURCE_STATE_COPY_DEST, finalState);
	commandList->ResourceBarrier(1, &barrier);
	commandList->Close();

	ID3D12CommandList* lists[] = { commandList.Get() };
	core.GetCommandQueue()->ExecuteCommandLists(_countof(lists), lists);

	ComPtr<ID3D12Fence> fence;
	if (FAILED(device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence))))
	{
		return false;
	}

	HANDLE fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
	if (!fenceEvent)
	{
		return false;
	}

	core.GetCommandQueue()->Signal(fence.Get(), 1);
	fence->SetEventOnCompletion(1, fenceEvent);
	WaitForSingleObject(fenceEvent, INFINITE);
	CloseHandle(fenceEvent);

	return true;
}
