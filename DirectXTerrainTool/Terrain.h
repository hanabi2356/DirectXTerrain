#pragma once
#include "Common.h"

#include <vector>

class GraphicsCore;

// 격자 높이 값의 공급원.
// 평면, 펄린 노이즈, 높이맵 이미지가 모두 같은 형태로 들어오므로
// 아래 TerrainMesh 는 높이가 어디서 왔는지 알 필요가 없다.
class HeightField
{
public:
	static HeightField MakeFlat(UINT width, UINT depth);
	static HeightField MakePerlin(UINT width, UINT depth,
		float frequency, float amplitude, int octaves, unsigned int seed = 1337u);

	UINT GetWidth() const { return m_width; }
	UINT GetDepth() const { return m_depth; }
	float At(UINT x, UINT z) const;

private:
	HeightField(UINT width, UINT depth);

	UINT m_width = 0;
	UINT m_depth = 0;
	std::vector<float> m_heights;
};

struct TerrainVertex
{
	DirectX::XMFLOAT3 position;
	DirectX::XMFLOAT3 normal;
	DirectX::XMFLOAT2 uv;
};

// HeightField 를 삼각형 격자 메시로 굽고 GPU 버퍼로 올린다.
class TerrainMesh
{
public:
	bool Build(GraphicsCore& core, const HeightField& field, float cellSize);
	void Draw(ID3D12GraphicsCommandList* commandList) const;

	UINT GetVertexCount() const { return m_vertexCount; }
	UINT GetIndexCount() const { return m_indexCount; }

private:
	ComPtr<ID3D12Resource> m_vertexBuffer;
	ComPtr<ID3D12Resource> m_indexBuffer;
	D3D12_VERTEX_BUFFER_VIEW m_vertexBufferView = {};
	D3D12_INDEX_BUFFER_VIEW m_indexBufferView = {};
	UINT m_vertexCount = 0;
	UINT m_indexCount = 0;
};
