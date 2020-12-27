#include "BaseRenderer.h"

#include "renderer.h"

#include "PlainPixelShader.h"
#include "VertexShader.h"

using Error = renderer::Error;

BaseRenderer::BaseRenderer(InitArgs &&args)
    : m_device(std::move(args.device))
    , m_deviceContext(std::move(args.deviceContext)) {
    createDiscreteSamplerState();
    createAlphaBlendState();
    createVertexShader();
    createPlainPixelShader();
}

BaseRenderer::~BaseRenderer() {
    // ensure device is clean, so we can create a new one!
    if (m_deviceContext) {
        m_deviceContext->ClearState();
        m_deviceContext->Flush();
    }
}

auto BaseRenderer::createShaderTexture(ID3D11Texture2D *texture) const
    -> ComPtr<ID3D11ShaderResourceView> {

    D3D11_TEXTURE2D_DESC description;
    texture->GetDesc(&description);

    D3D11_SHADER_RESOURCE_VIEW_DESC shader_resource_description;
    shader_resource_description.Format = description.Format;
    shader_resource_description.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    shader_resource_description.Texture2D.MostDetailedMip = description.MipLevels - 1;
    shader_resource_description.Texture2D.MipLevels = description.MipLevels;

    ComPtr<ID3D11ShaderResourceView> shader_resource;
    const auto result =
        m_device->CreateShaderResourceView(texture, &shader_resource_description, &shader_resource);
    if (IS_ERROR(result)) throw Error{result, "Failed to create shader texture resource"};
    return shader_resource;
}

auto BaseRenderer::createLinearSampler() -> ComPtr<ID3D11SamplerState> {
    D3D11_SAMPLER_DESC descrption;
    RtlZeroMemory(&descrption, sizeof(descrption));
    descrption.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    descrption.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
    descrption.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
    descrption.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    descrption.ComparisonFunc = D3D11_COMPARISON_NEVER;
    descrption.MinLOD = 0;
    descrption.MaxLOD = D3D11_FLOAT32_MAX;
    ComPtr<ID3D11SamplerState> sampler;
    const auto result = m_device->CreateSamplerState(&descrption, &sampler);
    if (IS_ERROR(result)) throw Error{result, "Failed to create linear sampler state"};
    return sampler;
}

void BaseRenderer::createDiscreteSamplerState() {
    D3D11_SAMPLER_DESC descrption;
    RtlZeroMemory(&descrption, sizeof(descrption));
    descrption.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
    descrption.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
    descrption.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
    descrption.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    descrption.ComparisonFunc = D3D11_COMPARISON_NEVER;
    descrption.MinLOD = 0;
    descrption.MaxLOD = D3D11_FLOAT32_MAX;
    const auto result = m_device->CreateSamplerState(&descrption, &m_discreteSamplerState);
    if (IS_ERROR(result)) throw Error{result, "Failed to create discrete sampler state"};
}

void BaseRenderer::createAlphaBlendState() {
    D3D11_BLEND_DESC description;
    description.AlphaToCoverageEnable = FALSE;
    description.IndependentBlendEnable = FALSE;
    description.RenderTarget[0].BlendEnable = TRUE;
    description.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
    description.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
    description.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
    description.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
    description.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
    description.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
    description.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
    const auto result = m_device->CreateBlendState(&description, &m_alphaBlendState);
    if (IS_ERROR(result)) throw Error{result, "Failed to create blend state"};
}

void BaseRenderer::createVertexShader() {
    const auto size = ARRAYSIZE(g_VertexShader);
    auto shader = &g_VertexShader[0];
    auto result = m_device->CreateVertexShader(shader, size, nullptr, &m_vertexShader);
    if (IS_ERROR(result)) throw Error{result, "Failed to create vertex shader"};

    static const auto layout = std::array{
        D3D11_INPUT_ELEMENT_DESC{
            "POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
        D3D11_INPUT_ELEMENT_DESC{
            "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 8, D3D11_INPUT_PER_VERTEX_DATA, 0}};
    const auto layout_size = static_cast<uint32_t>(layout.size());
    result = m_device->CreateInputLayout(layout.data(), layout_size, shader, size, &m_inputLayout);
    if (IS_ERROR(result)) throw Error{result, "Failed to create input layout"};
}

void BaseRenderer::createPlainPixelShader() {
    const auto size = ARRAYSIZE(g_PlainPixelShader);
    auto shader = &g_PlainPixelShader[0];
    ID3D11ClassLinkage *linkage = nullptr;
    const auto result = m_device->CreatePixelShader(shader, size, linkage, &m_plainPixelShader);
    if (IS_ERROR(result)) throw Error{result, "Failed to create pixel shader"};
}
