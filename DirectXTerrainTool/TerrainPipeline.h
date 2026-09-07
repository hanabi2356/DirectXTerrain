#pragma once
#include "Common.h"
#include "GraphicsCore.h"

#include <d3dcompiler.h>

#pragma comment(lib, "d3dcompiler.lib")

class Camera;

// 지형 샘플들이 공유하는 기본 파이프라인.
// 루트 시그니처, 솔리드/와이어프레임 PSO, 프레임 상수 버퍼를 한 묶음으로 들고 있다.
class TerrainPipeline
{
public:
	bool Initialize(GraphicsCore& core);

	// 색상 그라데이션의 기준이 되는 높이. 지형의 대략적인 진폭을 넣으면 된다.
	void SetHeightScale(float heightScale) { m_heightScale = heightScale; }

	// 상수 버퍼를 갱신하고 루트 시그니처와 PSO 를 바인딩한다.
	void Bind(ID3D12GraphicsCommandList* commandList, UINT frameIndex,
		const Camera& camera, bool wireframe);

private:
	struct FrameConstants
	{
		DirectX::XMFLOAT4X4 viewProjection;
		DirectX::XMFLOAT4 lightDirection;
		DirectX::XMFLOAT4 cameraPosition;
		DirectX::XMFLOAT4 terrainParams;   // x = 높이 정규화 기준
	};

	// 상수 버퍼는 256 바이트 정렬이 필요하다.
	static constexpr UINT ConstantBufferStride = 256;

	bool CreatePipelineStates(ID3D12Device* device, DXGI_FORMAT rtvFormat);
	bool CreateConstantBuffers(ID3D12Device* device);

	ComPtr<ID3D12RootSignature> m_rootSignature;
	ComPtr<ID3D12PipelineState> m_solidPipelineState;
	ComPtr<ID3D12PipelineState> m_wireframePipelineState;

	ComPtr<ID3D12Resource> m_constantBuffers[GraphicsCore::FrameCount];
	uint8_t* m_mappedConstants[GraphicsCore::FrameCount] = {};

	float m_heightScale = 1.0f;
};
