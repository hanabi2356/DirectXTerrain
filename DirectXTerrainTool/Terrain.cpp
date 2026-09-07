#include "Terrain.h"
#include "GpuBuffer.h"

#include <algorithm>
#include <cmath>
#include <numeric>
#include <random>

using namespace DirectX;

namespace
{
	// 고전적인 펄린 노이즈. 2번 샘플에서 그대로 재사용한다.
	class PerlinNoise
	{
	public:
		explicit PerlinNoise(unsigned int seed)
		{
			// 0~255 를 채운 뒤 시드로 섞는다. 이 순서가 지형의 생김새를 결정한다.
			std::iota(m_permutation, m_permutation + 256, 0);

			std::mt19937 rng(seed);
			std::shuffle(m_permutation, m_permutation + 256, rng);

			// 뒤쪽 256 개에 같은 내용을 복사해 둔다.
			// 조회할 때 인덱스가 최대 511 까지 올라가는데, 이러면 범위 검사를 생략할 수 있다.
			for (int i = 0; i < 256; ++i)
			{
				m_permutation[256 + i] = m_permutation[i];
			}
		}

		// 좌표가 속한 격자 칸의 네 모서리에서 그래디언트 내적을 구한 뒤 부드럽게 섞는다.
		// 정수 좌표에서는 항상 0 이 나오므로, 호출부에서 주파수에 정수를 넣으면 지형이 평평해진다.
		float Sample(float x, float y) const
		{
			// 격자 칸의 좌표(정수부)와 칸 안에서의 위치(소수부)로 나눈다.
			const int xi = static_cast<int>(std::floor(x)) & 255;
			const int yi = static_cast<int>(std::floor(y)) & 255;
			const float xf = x - std::floor(x);
			const float yf = y - std::floor(y);

			// 그대로 선형 보간하면 칸 경계가 눈에 보인다. Fade 로 완만하게 만든다.
			const float u = Fade(xf);
			const float v = Fade(yf);

			// 네 모서리의 그래디언트를 순열 테이블로 결정한다. 같은 모서리는 항상 같은 값이 나와
			// 이웃한 칸끼리 경계가 매끄럽게 이어진다.
			const int aa = m_permutation[m_permutation[xi] + yi];
			const int ab = m_permutation[m_permutation[xi] + yi + 1];
			const int ba = m_permutation[m_permutation[xi + 1] + yi];
			const int bb = m_permutation[m_permutation[xi + 1] + yi + 1];

			const float lower = Lerp(Grad(aa, xf, yf), Grad(ba, xf - 1.0f, yf), u);
			const float upper = Lerp(Grad(ab, xf, yf - 1.0f), Grad(bb, xf - 1.0f, yf - 1.0f), u);
			return Lerp(lower, upper, v);
		}

	private:
		// 개선판 펄린 노이즈의 5차 보간 함수 6t^5 - 15t^4 + 10t^3.
		// 격자 경계에서 1차뿐 아니라 2차 도함수까지 0 이라 격자선이 드러나지 않는다.
		static float Fade(float t) { return t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f); }

		static float Lerp(float a, float b, float t) { return a + t * (b - a); }

		// 해시 하위 2비트로 (1,1) (-1,1) (1,-1) (-1,-1) 중 하나를 골라 내적한 결과와 같다.
		// 곱셈 없이 부호만 바꾸면 되므로 이렇게 펼쳐 쓴다.
		static float Grad(int hash, float x, float y)
		{
			switch (hash & 3)
			{
			case 0:  return  x + y;
			case 1:  return -x + y;
			case 2:  return  x - y;
			default: return -x - y;
			}
		}

		int m_permutation[512] = {};
	};
}

HeightField::HeightField(UINT width, UINT depth)
	: m_width(width)
	, m_depth(depth)
	, m_heights(static_cast<size_t>(width) * depth, 0.0f)
{
}

HeightField HeightField::MakeFlat(UINT width, UINT depth)
{
	return HeightField(width, depth);
}

HeightField HeightField::MakePerlin(UINT width, UINT depth,
	float frequency, float amplitude, int octaves, unsigned int seed)
{
	HeightField field(width, depth);
	const PerlinNoise noise(seed);

	for (UINT z = 0; z < depth; ++z)
	{
		for (UINT x = 0; x < width; ++x)
		{
			// 옥타브를 겹쳐 쌓는다(fBm).
			// 주파수를 두 배로 올리고 진폭을 절반으로 줄이면서 더하면,
			// 큰 산맥 위에 작은 굴곡이 얹히는 자연스러운 지형이 된다.
			float value = 0.0f;
			float currentFrequency = frequency;
			float currentAmplitude = amplitude;

			for (int octave = 0; octave < octaves; ++octave)
			{
				value += noise.Sample(x * currentFrequency, z * currentFrequency) * currentAmplitude;
				currentFrequency *= 2.0f;
				currentAmplitude *= 0.5f;
			}

			field.m_heights[static_cast<size_t>(z) * width + x] = value;
		}
	}

	return field;
}

float HeightField::At(UINT x, UINT z) const
{
	if (x >= m_width || z >= m_depth)
	{
		return 0.0f;
	}
	return m_heights[static_cast<size_t>(z) * m_width + x];
}

bool TerrainMesh::Build(GraphicsCore& core, const HeightField& field, float cellSize)
{
	const UINT width = field.GetWidth();
	const UINT depth = field.GetDepth();
	if (width < 2 || depth < 2)
	{
		return false;
	}

	// 지형의 중심이 원점에 오도록 절반만큼 빼준다.
	const float halfWidth = (width - 1) * cellSize * 0.5f;
	const float halfDepth = (depth - 1) * cellSize * 0.5f;

	std::vector<TerrainVertex> vertices(static_cast<size_t>(width) * depth);
	for (UINT z = 0; z < depth; ++z)
	{
		for (UINT x = 0; x < width; ++x)
		{
			TerrainVertex& vertex = vertices[static_cast<size_t>(z) * width + x];

			vertex.position =
			{
				x * cellSize - halfWidth,
				field.At(x, z),
				z * cellSize - halfDepth
			};

			vertex.uv =
			{
				static_cast<float>(x) / (width - 1),
				static_cast<float>(z) / (depth - 1)
			};

			// 중앙 차분으로 법선을 구한다. 가장자리는 자기 자신을 이웃으로 써서 기울기를 0 으로 만든다.
			const float left = field.At(x > 0 ? x - 1 : x, z);
			const float right = field.At(x + 1 < width ? x + 1 : x, z);
			const float back = field.At(x, z > 0 ? z - 1 : z);
			const float front = field.At(x, z + 1 < depth ? z + 1 : z);

			XMStoreFloat3(&vertex.normal, XMVector3Normalize(
				XMVectorSet(left - right, 2.0f * cellSize, back - front, 0.0f)));
		}
	}

	std::vector<UINT> indices;
	indices.reserve(static_cast<size_t>(width - 1) * (depth - 1) * 6);

	for (UINT z = 0; z + 1 < depth; ++z)
	{
		for (UINT x = 0; x + 1 < width; ++x)
		{
			const UINT nearLeft = z * width + x;
			const UINT nearRight = z * width + x + 1;
			const UINT farLeft = (z + 1) * width + x;
			const UINT farRight = (z + 1) * width + x + 1;

			// 위에서 내려다볼 때 시계 방향이 되도록 감는다(기본 래스터라이저는 CW 가 앞면).
			indices.push_back(nearLeft);
			indices.push_back(farLeft);
			indices.push_back(farRight);

			indices.push_back(nearLeft);
			indices.push_back(farRight);
			indices.push_back(nearRight);
		}
	}

	const UINT64 vertexBytes = vertices.size() * sizeof(TerrainVertex);
	const UINT64 indexBytes = indices.size() * sizeof(UINT);

	if (!CreateDefaultBuffer(core, vertices.data(), vertexBytes,
			D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER, m_vertexBuffer))
	{
		return false;
	}

	if (!CreateDefaultBuffer(core, indices.data(), indexBytes,
			D3D12_RESOURCE_STATE_INDEX_BUFFER, m_indexBuffer))
	{
		return false;
	}

	m_vertexBufferView.BufferLocation = m_vertexBuffer->GetGPUVirtualAddress();
	m_vertexBufferView.StrideInBytes = sizeof(TerrainVertex);
	m_vertexBufferView.SizeInBytes = static_cast<UINT>(vertexBytes);

	m_indexBufferView.BufferLocation = m_indexBuffer->GetGPUVirtualAddress();
	m_indexBufferView.Format = DXGI_FORMAT_R32_UINT;
	m_indexBufferView.SizeInBytes = static_cast<UINT>(indexBytes);

	m_vertexCount = static_cast<UINT>(vertices.size());
	m_indexCount = static_cast<UINT>(indices.size());
	return true;
}

void TerrainMesh::Draw(ID3D12GraphicsCommandList* commandList) const
{
	if (m_indexCount == 0)
	{
		return;
	}

	commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	commandList->IASetVertexBuffers(0, 1, &m_vertexBufferView);
	commandList->IASetIndexBuffer(&m_indexBufferView);
	commandList->DrawIndexedInstanced(m_indexCount, 1, 0, 0, 0);
}
