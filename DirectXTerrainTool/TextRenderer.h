#pragma once
#include"Common.h"
#include"GraphicsCore.h"

#include<d3dcompiler.h>
#include<string>
#include<vector>
#include <unordered_map>

#pragma comment(lib, "d3dcompiler.lib")

// DirectX 12 에는 문자를 그리는 기능이 없다.
// 그래서 초기화 때 GDI 로 글자들을 텍스처 한 장(아틀라스)에 미리 구워두고,
// 매 프레임 그 텍스처를 입힌 사각형들을 그려서 문자를 표현한다.
//
// 사용 순서: Initialize -> (매 프레임) Begin -> Draw... -> Record
class TextRenderer
{
public:
	// charset 에 들어 있는 문자만 아틀라스에 구워진다.
	// 여기 없는 글자는 화면에 아무것도 나오지 않으니 주의.
	bool Initialize(GraphicsCore& core, const wchar_t* fontName, int fontHeight,
		const std::wstring& charset);

	// 프레임 시작 시 누적된 정점을 비운다.
	void Begin();

	// 화면 좌표(픽셀) 기준으로 문자열을 쌓아둔다. 실제 그리기는 Record 에서 한 번에 일어난다.
	void Draw(const std::wstring& text, float x, float y,
		const DirectX::XMFLOAT4& color, float scale = 1.0f);

	// 쌓아둔 정점을 GPU 로 올리고 드로우 콜을 커맨드 리스트에 기록한다.
	void Record(ID3D12GraphicsCommandList* commandList, UINT frameIndex,
		UINT screenWidth, UINT screenHeight);

	float GetLineHeight() const { return m_lineHeight; }

	// 가운데 정렬이나 클릭 판정용으로 문자열의 픽셀 너비를 미리 구한다.
	float MeasureWidth(const std::wstring& text, float scale = 1.0f) const;

private:
	// 아틀라스 안에서 글자 하나가 차지하는 영역과 크기.
	struct Glyph
	{
		float u0, v0, u1, v1;   // 아틀라스 텍스처 좌표(0~1)
		float width, height;    // 픽셀 크기
		float advance;          // 이 글자를 그린 뒤 다음 글자까지 이동할 거리
	};

	struct Vertex
	{
		DirectX::XMFLOAT2 position;
		DirectX::XMFLOAT2 uv;
		DirectX::XMFLOAT4 color;
	};

	// 한 프레임에 그릴 수 있는 글자 수 상한. 정점 버퍼 크기를 정하는 기준이다.
	static constexpr UINT MaxCharsPerFrame = 4096;

	// 글자 하나가 사각형이므로 삼각형 두 개, 즉 정점 여섯 개다.
	static constexpr UINT VerticesPerChar = 6;

	bool BuildFontAtlas(ID3D12Device* device, ID3D12CommandQueue* queue,
		const wchar_t* fontName, int fontHeight, const std::wstring& charset);
	bool CreatePipeline(ID3D12Device* device, DXGI_FORMAT rtvFormat);
	bool CreateVertexBuffers(ID3D12Device* device);

	ComPtr<ID3D12RootSignature>  m_rootSignature;
	ComPtr<ID3D12PipelineState>  m_pipelineState;
	ComPtr<ID3D12DescriptorHeap> m_srvHeap;

	// 글자들이 구워진 텍스처. R8 한 채널만 쓰고 그 값을 알파로 사용한다.
	ComPtr<ID3D12Resource>       m_atlasTexture;

	// GPU 가 이전 프레임의 정점을 읽는 중일 수 있으므로 프레임마다 따로 둔다.
	// 업로드 힙에 만들어 계속 매핑해 두고 memcpy 로만 갱신한다.
	ComPtr<ID3D12Resource>   m_vertexBuffers[GraphicsCore::FrameCount];
	Vertex* m_mappedVertices[GraphicsCore::FrameCount] = {};
	D3D12_VERTEX_BUFFER_VIEW m_vertexBufferViews[GraphicsCore::FrameCount] = {};

	// 문자 -> 아틀라스 위치. Draw 할 때마다 여기서 찾는다.
	std::unordered_map<wchar_t, Glyph> m_glyphs;

	// 이번 프레임에 그릴 정점을 CPU 쪽에 모아두는 곳.
	std::vector<Vertex> m_vertices;

	// 폰트의 한 줄 높이. 줄바꿈 간격 계산에 쓴다.
	float m_lineHeight = 0.0f;

};

