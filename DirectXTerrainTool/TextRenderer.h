#pragma once
#include"Common.h"
#include"GraphicsCore.h"

#include<d3dcompiler.h>
#include<string>
#include<vector>
#include <unordered_map>

#pragma comment(lib, "d3dcompiler.lib")

class TextRenderer
{
public:
    bool Initialize(GraphicsCore& core, const wchar_t* fontName, int fontHeight,
        const std::wstring& charset);
    void Begin();   // 프레임 시작 시 누적 정점 초기화
    void Draw(const std::wstring& text, float x, float y,
        const DirectX::XMFLOAT4& color, float scale = 1.0f);
    void Record(ID3D12GraphicsCommandList* commandList, UINT frameIndex,
        UINT screenWidth, UINT screenHeight);
    float GetLineHeight() const { return m_lineHeight; }
    float MeasureWidth(const std::wstring& text, float scale = 1.0f) const;

private:
    struct Glyph
    {
        float u0, v0, u1, v1;
        float width, height;
        float advance;
    };
    struct Vertex
    {
        DirectX::XMFLOAT2 position;
        DirectX::XMFLOAT2 uv;
        DirectX::XMFLOAT4 color;
    };
    static constexpr UINT MaxCharsPerFrame = 4096;
    static constexpr UINT VerticesPerChar = 6;
    bool BuildFontAtlas(ID3D12Device* device, ID3D12CommandQueue* queue,
        const wchar_t* fontName, int fontHeight, const std::wstring& charset);
    bool CreatePipeline(ID3D12Device* device, DXGI_FORMAT rtvFormat);
    bool CreateVertexBuffers(ID3D12Device* device);
    ComPtr<ID3D12RootSignature>  m_rootSignature;
    ComPtr<ID3D12PipelineState>  m_pipelineState;
    ComPtr<ID3D12DescriptorHeap> m_srvHeap;
    ComPtr<ID3D12Resource>       m_atlasTexture;
    ComPtr<ID3D12Resource>   m_vertexBuffers[GraphicsCore::FrameCount];
    Vertex* m_mappedVertices[GraphicsCore::FrameCount] = {};
    D3D12_VERTEX_BUFFER_VIEW m_vertexBufferViews[GraphicsCore::FrameCount] = {};
    std::unordered_map<wchar_t, Glyph> m_glyphs;
    std::vector<Vertex> m_vertices;
    float m_lineHeight = 0.0f;

};

