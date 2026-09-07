#include "TextRenderer.h"
#include<algorithm>
#include<cstdint>
#include<set>

using namespace DirectX;

namespace
{
    // 픽셀 좌표를 그대로 받아 NDC 로 바꾸는 단순 스프라이트 셰이더.
    // 아틀라스는 R8_UNORM 이므로 r 채널을 알파로 사용한다.
    constexpr char ShaderSource[] = R"(
cbuffer ScreenConstants : register(b0)
{
    float2 invScreenSize;
};
Texture2D    g_atlas   : register(t0);
SamplerState g_sampler : register(s0);
struct VSInput
{
    float2 position : POSITION;
    float2 uv       : TEXCOORD;
    float4 color    : COLOR;
};
struct PSInput
{
    float4 position : SV_POSITION;
    float2 uv       : TEXCOORD;
    float4 color    : COLOR;
};
PSInput VSMain(VSInput input)
{
    float2 ndc = input.position * invScreenSize * 2.0f - 1.0f;
    PSInput output;
    output.position = float4(ndc.x, -ndc.y, 0.0f, 1.0f);
    output.uv       = input.uv;
    output.color    = input.color;
    return output;
}
float4 PSMain(PSInput input) : SV_TARGET
{
    float coverage = g_atlas.Sample(g_sampler, input.uv).r;
    return float4(input.color.rgb, input.color.a * coverage);
}
)";
}

bool TextRenderer::Initialize(GraphicsCore& core, const wchar_t* fontName, int fontHeight,
    const std::wstring& charset)
{
    ID3D12Device* device = core.GetDevice();
    if (!BuildFontAtlas(device, core.GetCommandQueue(), fontName, fontHeight, charset)) return false;
    if (!CreatePipeline(device, core.GetBackBufferFormat()))                            return false;
    if (!CreateVertexBuffers(device))                                                   return false;
    m_vertices.reserve(static_cast<size_t>(MaxCharsPerFrame) * VerticesPerChar);
    return true;
}
bool TextRenderer::BuildFontAtlas(ID3D12Device* device, ID3D12CommandQueue* queue,
    const wchar_t* fontName, int fontHeight,
    const std::wstring& charset)
{
    HDC hdc = CreateCompatibleDC(nullptr);
    if (!hdc)
    {
        return false;
    }
    HFONT font = CreateFontW(
        -fontHeight, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS,
        ANTIALIASED_QUALITY, DEFAULT_PITCH | FF_DONTCARE, fontName);
    HGDIOBJ previousFont = SelectObject(hdc, font);
    TEXTMETRICW metrics = {};
    GetTextMetricsW(hdc, &metrics);
    m_lineHeight = static_cast<float>(metrics.tmHeight);
    // 중복 제거된 문자 목록
    const std::set<wchar_t> uniqueChars(charset.begin(), charset.end());
    struct Placement { wchar_t ch; SIZE size; UINT x; UINT y; };
    std::vector<Placement> placements;
    placements.reserve(uniqueChars.size());
    constexpr UINT Padding = 1;
    constexpr UINT AtlasWidth = 1024;
    UINT penX = Padding;
    UINT penY = Padding;
    UINT rowHeight = 0;
    for (wchar_t ch : uniqueChars)
    {
        SIZE size = {};
        GetTextExtentPoint32W(hdc, &ch, 1, &size);
        if (penX + static_cast<UINT>(size.cx) + Padding > AtlasWidth)
        {
            penX = Padding;
            penY += rowHeight + Padding;
            rowHeight = 0;
        }
        placements.push_back({ ch, size, penX, penY });
        penX += static_cast<UINT>(size.cx) + Padding;
        rowHeight = (std::max)(rowHeight, static_cast<UINT>(size.cy));
    }
    const UINT atlasHeight = penY + rowHeight + Padding;
    // 32bpp top-down DIB 에 검은 배경 + 흰 글자로 래스터화
    BITMAPINFO bitmapInfo = {};
    bitmapInfo.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bitmapInfo.bmiHeader.biWidth = static_cast<LONG>(AtlasWidth);
    bitmapInfo.bmiHeader.biHeight = -static_cast<LONG>(atlasHeight);
    bitmapInfo.bmiHeader.biPlanes = 1;
    bitmapInfo.bmiHeader.biBitCount = 32;
    bitmapInfo.bmiHeader.biCompression = BI_RGB;
    void* bits = nullptr;
    HBITMAP bitmap = CreateDIBSection(hdc, &bitmapInfo, DIB_RGB_COLORS, &bits, nullptr, 0);
    HGDIOBJ previousBitmap = SelectObject(hdc, bitmap);
    RECT fullRect = { 0, 0, static_cast<LONG>(AtlasWidth), static_cast<LONG>(atlasHeight) };
    FillRect(hdc, &fullRect, static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH)));
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, RGB(255, 255, 255));
    for (const Placement& placement : placements)
    {
        TextOutW(hdc, static_cast<int>(placement.x), static_cast<int>(placement.y), &placement.ch, 1);
    }
    GdiFlush();
    // BGRA → R8 (안티에일리어싱 커버리지만 남긴다)
    std::vector<uint8_t> pixels(static_cast<size_t>(AtlasWidth) * atlasHeight);
    const auto* source = static_cast<const uint32_t*>(bits);
    for (size_t i = 0; i < pixels.size(); ++i)
    {
        pixels[i] = static_cast<uint8_t>(source[i] & 0xFF);
    }
    const float invAtlasWidth = 1.0f / static_cast<float>(AtlasWidth);
    const float invAtlasHeight = 1.0f / static_cast<float>(atlasHeight);
    for (const Placement& placement : placements)
    {
        Glyph glyph = {};
        glyph.u0 = placement.x * invAtlasWidth;
        glyph.v0 = placement.y * invAtlasHeight;
        glyph.u1 = (placement.x + placement.size.cx) * invAtlasWidth;
        glyph.v1 = (placement.y + placement.size.cy) * invAtlasHeight;
        glyph.width = static_cast<float>(placement.size.cx);
        glyph.height = static_cast<float>(placement.size.cy);
        glyph.advance = static_cast<float>(placement.size.cx);
        m_glyphs[placement.ch] = glyph;
    }
    SelectObject(hdc, previousBitmap);
    SelectObject(hdc, previousFont);
    DeleteObject(bitmap);
    DeleteObject(font);
    DeleteDC(hdc);
    // ---- D3D12 텍스처 생성 및 업로드 ----
    const CD3DX12_HEAP_PROPERTIES defaultHeap(D3D12_HEAP_TYPE_DEFAULT);
    const CD3DX12_RESOURCE_DESC textureDesc =
        CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_R8_UNORM, AtlasWidth, atlasHeight, 1, 1);
    if (FAILED(device->CreateCommittedResource(&defaultHeap, D3D12_HEAP_FLAG_NONE, &textureDesc,
        D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&m_atlasTexture))))
    {
        return false;
    }
    const UINT64 uploadSize = GetRequiredIntermediateSize(m_atlasTexture.Get(), 0, 1);
    const CD3DX12_HEAP_PROPERTIES uploadHeap(D3D12_HEAP_TYPE_UPLOAD);
    const CD3DX12_RESOURCE_DESC uploadDesc = CD3DX12_RESOURCE_DESC::Buffer(uploadSize);
    ComPtr<ID3D12Resource> uploadBuffer;
    if (FAILED(device->CreateCommittedResource(&uploadHeap, D3D12_HEAP_FLAG_NONE, &uploadDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&uploadBuffer))))
    {
        return false;
    }
    D3D12_SUBRESOURCE_DATA subresource = {};
    subresource.pData = pixels.data();
    subresource.RowPitch = static_cast<LONG_PTR>(AtlasWidth);
    subresource.SlicePitch = static_cast<LONG_PTR>(pixels.size());
    // 초기화 전용 일회성 커맨드 리스트
    ComPtr<ID3D12CommandAllocator> allocator;
    ComPtr<ID3D12GraphicsCommandList> commandList;
    device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&allocator));
    device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, allocator.Get(), nullptr,
        IID_PPV_ARGS(&commandList));
    UpdateSubresources<1>(commandList.Get(), m_atlasTexture.Get(), uploadBuffer.Get(), 0, 0, 1, &subresource);
    const CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
        m_atlasTexture.Get(),
        D3D12_RESOURCE_STATE_COPY_DEST,
        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    commandList->ResourceBarrier(1, &barrier);
    commandList->Close();
    ID3D12CommandList* lists[] = { commandList.Get() };
    queue->ExecuteCommandLists(_countof(lists), lists);
    // 업로드 버퍼가 파괴되기 전에 완료를 보장
    ComPtr<ID3D12Fence> fence;
    device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence));
    HANDLE fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    queue->Signal(fence.Get(), 1);
    fence->SetEventOnCompletion(1, fenceEvent);
    WaitForSingleObject(fenceEvent, INFINITE);
    CloseHandle(fenceEvent);
    // SRV 힙
    D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
    heapDesc.NumDescriptors = 1;
    heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    if (FAILED(device->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&m_srvHeap))))
    {
        return false;
    }
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format = DXGI_FORMAT_R8_UNORM;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.Texture2D.MipLevels = 1;
    device->CreateShaderResourceView(m_atlasTexture.Get(), &srvDesc,
        m_srvHeap->GetCPUDescriptorHandleForHeapStart());
    return true;
}
bool TextRenderer::CreatePipeline(ID3D12Device* device, DXGI_FORMAT rtvFormat)
{
    CD3DX12_DESCRIPTOR_RANGE srvRange;
    srvRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);
    CD3DX12_ROOT_PARAMETER rootParameters[2];
    rootParameters[0].InitAsDescriptorTable(1, &srvRange, D3D12_SHADER_VISIBILITY_PIXEL);
    rootParameters[1].InitAsConstants(2, 0, 0, D3D12_SHADER_VISIBILITY_VERTEX);
    const CD3DX12_STATIC_SAMPLER_DESC staticSampler(
        0, D3D12_FILTER_MIN_MAG_MIP_LINEAR,
        D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
        D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
        D3D12_TEXTURE_ADDRESS_MODE_CLAMP);
    CD3DX12_ROOT_SIGNATURE_DESC rootSignatureDesc;
    rootSignatureDesc.Init(_countof(rootParameters), rootParameters, 1, &staticSampler,
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
    if (FAILED(D3DCompile(ShaderSource, sizeof(ShaderSource) - 1, "TextShader", nullptr, nullptr,
        "VSMain", "vs_5_0", compileFlags, 0, &vertexShader, &error)))
    {
        return false;
    }
    if (FAILED(D3DCompile(ShaderSource, sizeof(ShaderSource) - 1, "TextShader", nullptr, nullptr,
        "PSMain", "ps_5_0", compileFlags, 0, &pixelShader, &error)))
    {
        return false;
    }
    const D3D12_INPUT_ELEMENT_DESC inputElements[] =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32_FLOAT,       0,  0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,       0,  8, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "COLOR",    0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 16, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
    };
    D3D12_RENDER_TARGET_BLEND_DESC blend = {};
    blend.BlendEnable = TRUE;
    blend.SrcBlend = D3D12_BLEND_SRC_ALPHA;
    blend.DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
    blend.BlendOp = D3D12_BLEND_OP_ADD;
    blend.SrcBlendAlpha = D3D12_BLEND_ONE;
    blend.DestBlendAlpha = D3D12_BLEND_INV_SRC_ALPHA;
    blend.BlendOpAlpha = D3D12_BLEND_OP_ADD;
    blend.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
    psoDesc.InputLayout = { inputElements, _countof(inputElements) };
    psoDesc.pRootSignature = m_rootSignature.Get();
    psoDesc.VS = CD3DX12_SHADER_BYTECODE(vertexShader.Get());
    psoDesc.PS = CD3DX12_SHADER_BYTECODE(pixelShader.Get());
    psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
    psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
    psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
    psoDesc.BlendState.RenderTarget[0] = blend;
    psoDesc.DepthStencilState.DepthEnable = FALSE;   // UI 는 깊이 테스트 없음
    psoDesc.DepthStencilState.StencilEnable = FALSE;
    psoDesc.DSVFormat = DXGI_FORMAT_D32_FLOAT;       // 바인딩된 DSV 와 포맷만 맞춘다
    psoDesc.SampleMask = UINT_MAX;
    psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    psoDesc.NumRenderTargets = 1;
    psoDesc.RTVFormats[0] = rtvFormat;
    psoDesc.SampleDesc.Count = 1;
    return SUCCEEDED(device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&m_pipelineState)));
}
bool TextRenderer::CreateVertexBuffers(ID3D12Device* device)
{
    const UINT bufferSize = MaxCharsPerFrame * VerticesPerChar * sizeof(Vertex);
    const CD3DX12_HEAP_PROPERTIES uploadHeap(D3D12_HEAP_TYPE_UPLOAD);
    const CD3DX12_RESOURCE_DESC bufferDesc = CD3DX12_RESOURCE_DESC::Buffer(bufferSize);
    for (UINT i = 0; i < GraphicsCore::FrameCount; ++i)
    {
        if (FAILED(device->CreateCommittedResource(&uploadHeap, D3D12_HEAP_FLAG_NONE, &bufferDesc,
            D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&m_vertexBuffers[i]))))
        {
            return false;
        }
        const CD3DX12_RANGE readRange(0, 0);   // CPU 는 읽지 않는다
        if (FAILED(m_vertexBuffers[i]->Map(0, &readRange, reinterpret_cast<void**>(&m_mappedVertices[i]))))
        {
            return false;
        }
        m_vertexBufferViews[i].BufferLocation = m_vertexBuffers[i]->GetGPUVirtualAddress();
        m_vertexBufferViews[i].StrideInBytes = sizeof(Vertex);
        m_vertexBufferViews[i].SizeInBytes = 0;
    }
    return true;
}
void TextRenderer::Begin()
{
    m_vertices.clear();
}
void TextRenderer::Draw(const std::wstring& text, float x, float y,
    const XMFLOAT4& color, float scale)
{
    float penX = x;
    float penY = y;
    for (wchar_t ch : text)
    {
        if (ch == L'\n')
        {
            penX = x;
            penY += m_lineHeight * scale;
            continue;
        }
        const auto found = m_glyphs.find(ch);
        if (found == m_glyphs.end())
        {
            continue;   // 아틀라스에 없는 문자는 건너뛴다
        }
        const Glyph& glyph = found->second;
        const float left = penX;
        const float top = penY;
        const float right = penX + glyph.width * scale;
        const float bottom = penY + glyph.height * scale;
        m_vertices.push_back({ { left,  top    }, { glyph.u0, glyph.v0 }, color });
        m_vertices.push_back({ { right, top    }, { glyph.u1, glyph.v0 }, color });
        m_vertices.push_back({ { left,  bottom }, { glyph.u0, glyph.v1 }, color });
        m_vertices.push_back({ { right, top    }, { glyph.u1, glyph.v0 }, color });
        m_vertices.push_back({ { right, bottom }, { glyph.u1, glyph.v1 }, color });
        m_vertices.push_back({ { left,  bottom }, { glyph.u0, glyph.v1 }, color });
        penX += glyph.advance * scale;
    }
}
float TextRenderer::MeasureWidth(const std::wstring& text, float scale) const
{
    float width = 0.0f;
    for (wchar_t ch : text)
    {
        const auto found = m_glyphs.find(ch);
        if (found != m_glyphs.end())
        {
            width += found->second.advance * scale;
        }
    }
    return width;
}
void TextRenderer::Record(ID3D12GraphicsCommandList* commandList, UINT frameIndex,
    UINT screenWidth, UINT screenHeight)
{
    if (m_vertices.empty())
    {
        return;
    }
    const size_t vertexCount =
        (std::min)(m_vertices.size(), static_cast<size_t>(MaxCharsPerFrame) * VerticesPerChar);
    const UINT byteSize = static_cast<UINT>(vertexCount * sizeof(Vertex));
    memcpy(m_mappedVertices[frameIndex], m_vertices.data(), byteSize);
    m_vertexBufferViews[frameIndex].SizeInBytes = byteSize;
    ID3D12DescriptorHeap* heaps[] = { m_srvHeap.Get() };
    commandList->SetDescriptorHeaps(_countof(heaps), heaps);
    commandList->SetGraphicsRootSignature(m_rootSignature.Get());
    commandList->SetPipelineState(m_pipelineState.Get());
    commandList->SetGraphicsRootDescriptorTable(0, m_srvHeap->GetGPUDescriptorHandleForHeapStart());

    const float invScreenSize[2] =
    {
        1.0f / static_cast<float>(screenWidth),
        1.0f / static_cast<float>(screenHeight)
    };

    commandList->SetGraphicsRoot32BitConstants(1, 2, invScreenSize, 0);
    commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    commandList->IASetVertexBuffers(0, 1, &m_vertexBufferViews[frameIndex]);
    commandList->DrawInstanced(static_cast<UINT>(vertexCount), 1, 0, 0);
}