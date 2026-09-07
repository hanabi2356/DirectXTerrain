#include "TerrainPipeline.h"
#include "Camera.h"
#include "Terrain.h"

#include <cstdint>

using namespace DirectX;

namespace
{
	// 정점을 변환하고 램버트 조명만 입히는 최소 셰이더.
	// 솔리드 상태에서도 격자가 보이도록 UV 기반 체커 패턴을 얹는다.
	constexpr char ShaderSource[] = R"(
cbuffer FrameConstants : register(b0)
{
    float4x4 viewProjection;
    float4   lightDirection;
    float4   cameraPosition;
};

struct VSInput
{
    float3 position : POSITION;
    float3 normal   : NORMAL;
    float2 uv       : TEXCOORD;
};

struct PSInput
{
    float4 position : SV_POSITION;
    float3 normal   : NORMAL;
    float2 uv       : TEXCOORD;
};

PSInput VSMain(VSInput input)
{
    PSInput output;
    output.position = mul(float4(input.position, 1.0f), viewProjection);
    output.normal   = input.normal;
    output.uv       = input.uv;
    return output;
}

float4 PSMain(PSInput input) : SV_TARGET
{
    float3 normal = normalize(input.normal);
    float3 toLight = normalize(-lightDirection.xyz);
    float lambert = saturate(dot(normal, toLight));

    float2 cell = step(0.5f, frac(input.uv * 32.0f));
    float checker = lerp(0.82f, 1.0f, abs(cell.x - cell.y));

    float3 baseColor = float3(0.36f, 0.48f, 0.28f) * checker;
    float3 color = baseColor * (0.30f + 0.70f * lambert);
    return float4(color, 1.0f);
}
)";
}

bool TerrainPipeline::Initialize(GraphicsCore& core)
{
	ID3D12Device* device = core.GetDevice();

	if (!CreatePipelineStates(device, core.GetBackBufferFormat()))
	{
		return false;
	}

	return CreateConstantBuffers(device);
}

bool TerrainPipeline::CreatePipelineStates(ID3D12Device* device, DXGI_FORMAT rtvFormat)
{
	CD3DX12_ROOT_PARAMETER rootParameters[1];
	rootParameters[0].InitAsConstantBufferView(0, 0, D3D12_SHADER_VISIBILITY_ALL);

	CD3DX12_ROOT_SIGNATURE_DESC rootSignatureDesc;
	rootSignatureDesc.Init(_countof(rootParameters), rootParameters, 0, nullptr,
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

	ComPtr<ID3DBlob> signature;
	ComPtr<ID3DBlob> error;
	if (FAILED(D3D12SerializeRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1,
			&signature, &error)))
	{
		return false;
	}
	if (FAILED(device->CreateRootSignature(0, signature->GetBufferPointer(),
			signature->GetBufferSize(), IID_PPV_ARGS(&m_rootSignature))))
	{
		return false;
	}

	UINT compileFlags = 0;
#if defined(_DEBUG)
	compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif

	ComPtr<ID3DBlob> vertexShader;
	ComPtr<ID3DBlob> pixelShader;
	if (FAILED(D3DCompile(ShaderSource, sizeof(ShaderSource) - 1, "TerrainShader", nullptr, nullptr,
			"VSMain", "vs_5_0", compileFlags, 0, &vertexShader, &error)))
	{
		return false;
	}
	if (FAILED(D3DCompile(ShaderSource, sizeof(ShaderSource) - 1, "TerrainShader", nullptr, nullptr,
			"PSMain", "ps_5_0", compileFlags, 0, &pixelShader, &error)))
	{
		return false;
	}

	// TerrainVertex 의 배치와 반드시 일치해야 한다.
	const D3D12_INPUT_ELEMENT_DESC inputElements[] =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,  0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,    0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
	};

	D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
	psoDesc.InputLayout = { inputElements, _countof(inputElements) };
	psoDesc.pRootSignature = m_rootSignature.Get();
	psoDesc.VS = CD3DX12_SHADER_BYTECODE(vertexShader.Get());
	psoDesc.PS = CD3DX12_SHADER_BYTECODE(pixelShader.Get());
	psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
	psoDesc.DSVFormat = DXGI_FORMAT_D32_FLOAT;
	psoDesc.SampleMask = UINT_MAX;
	psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	psoDesc.NumRenderTargets = 1;
	psoDesc.RTVFormats[0] = rtvFormat;
	psoDesc.SampleDesc.Count = 1;

	if (FAILED(device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&m_solidPipelineState))))
	{
		return false;
	}

	// 와이어프레임은 뒷면까지 보여야 구조가 읽히므로 컬링을 끈다.
	psoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_WIREFRAME;
	psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;

	return SUCCEEDED(device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&m_wireframePipelineState)));
}

bool TerrainPipeline::CreateConstantBuffers(ID3D12Device* device)
{
	static_assert(sizeof(FrameConstants) <= ConstantBufferStride, "프레임 상수가 정렬 크기를 넘었다.");

	const CD3DX12_HEAP_PROPERTIES uploadHeap(D3D12_HEAP_TYPE_UPLOAD);
	const CD3DX12_RESOURCE_DESC bufferDesc = CD3DX12_RESOURCE_DESC::Buffer(ConstantBufferStride);

	for (UINT i = 0; i < GraphicsCore::FrameCount; ++i)
	{
		if (FAILED(device->CreateCommittedResource(&uploadHeap, D3D12_HEAP_FLAG_NONE, &bufferDesc,
				D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&m_constantBuffers[i]))))
		{
			return false;
		}

		const CD3DX12_RANGE readRange(0, 0);
		if (FAILED(m_constantBuffers[i]->Map(0, &readRange,
				reinterpret_cast<void**>(&m_mappedConstants[i]))))
		{
			return false;
		}
	}

	return true;
}

void TerrainPipeline::Bind(ID3D12GraphicsCommandList* commandList, UINT frameIndex,
	const Camera& camera, bool wireframe)
{
	FrameConstants constants = {};

	// HLSL 은 기본이 열 우선이므로 행 우선인 XMMATRIX 를 전치해서 넘긴다.
	XMStoreFloat4x4(&constants.viewProjection, XMMatrixTranspose(camera.GetViewProjection()));
	constants.lightDirection = { -0.45f, -0.80f, 0.40f, 0.0f };

	const XMFLOAT3& position = camera.GetPosition();
	constants.cameraPosition = { position.x, position.y, position.z, 1.0f };

	memcpy(m_mappedConstants[frameIndex], &constants, sizeof(constants));

	commandList->SetGraphicsRootSignature(m_rootSignature.Get());
	commandList->SetPipelineState(wireframe ? m_wireframePipelineState.Get() : m_solidPipelineState.Get());
	commandList->SetGraphicsRootConstantBufferView(0, m_constantBuffers[frameIndex]->GetGPUVirtualAddress());
}
