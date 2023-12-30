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

auto BaseRenderer::createShaderTexture(ID3D11Texture2D *texture) const -> ComPtr<ID3D11ShaderResourceView> {

    auto description = D3D11_TEXTURE2D_DESC{};
    texture->GetDesc(&description);

    auto const shader_resource_description = D3D11_SHADER_RESOURCE_VIEW_DESC{
        .Format = description.Format,
        .ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D,
        .Texture2D =
            D3D11_TEX2D_SRV{
                .MostDetailedMip = description.MipLevels - 1,
                .MipLevels = description.MipLevels,
            },
    };
    auto shader_resource = ComPtr<ID3D11ShaderResourceView>{};
    auto const result = m_device->CreateShaderResourceView(texture, &shader_resource_description, &shader_resource);
    if (IS_ERROR(result)) throw Error{result, "Failed to create shader texture resource"};
    return shader_resource;
}

auto BaseRenderer::createLinearSampler() -> ComPtr<ID3D11SamplerState> {
    auto const description = D3D11_SAMPLER_DESC{
        .Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR,
        .AddressU = D3D11_TEXTURE_ADDRESS_CLAMP,
        .AddressV = D3D11_TEXTURE_ADDRESS_CLAMP,
        .AddressW = D3D11_TEXTURE_ADDRESS_CLAMP,
        .MipLODBias = {},
        .MaxAnisotropy = {},
        .ComparisonFunc = D3D11_COMPARISON_NEVER,
        .BorderColor = {},
        .MinLOD = 0,
        .MaxLOD = D3D11_FLOAT32_MAX,
    };
    ComPtr<ID3D11SamplerState> sampler;
    auto const result = m_device->CreateSamplerState(&description, &sampler);
    if (IS_ERROR(result)) throw Error{result, "Failed to create linear sampler state"};
    return sampler;
}

void BaseRenderer::createDiscreteSamplerState() {
    auto const description = D3D11_SAMPLER_DESC{
        .Filter = D3D11_FILTER_MIN_MAG_MIP_POINT,
        .AddressU = D3D11_TEXTURE_ADDRESS_CLAMP,
        .AddressV = D3D11_TEXTURE_ADDRESS_CLAMP,
        .AddressW = D3D11_TEXTURE_ADDRESS_CLAMP,
        .MipLODBias = {},
        .MaxAnisotropy = {},
        .ComparisonFunc = D3D11_COMPARISON_NEVER,
        .BorderColor = {},
        .MinLOD = 0,
        .MaxLOD = D3D11_FLOAT32_MAX,
    };
    auto const result = m_device->CreateSamplerState(&description, &m_discreteSamplerState);
    if (IS_ERROR(result)) throw Error{result, "Failed to create discrete sampler state"};
}

void BaseRenderer::createAlphaBlendState() {
    auto description = D3D11_BLEND_DESC{
        .AlphaToCoverageEnable = FALSE,
        .IndependentBlendEnable = FALSE,
        .RenderTarget =
            {
                D3D11_RENDER_TARGET_BLEND_DESC{
                    .BlendEnable = TRUE,
                    .SrcBlend = D3D11_BLEND_SRC_ALPHA,
                    .DestBlend = D3D11_BLEND_INV_SRC_ALPHA,
                    .BlendOp = D3D11_BLEND_OP_ADD,
                    .SrcBlendAlpha = D3D11_BLEND_ONE,
                    .DestBlendAlpha = D3D11_BLEND_ZERO,
                    .BlendOpAlpha = D3D11_BLEND_OP_ADD,
                    .RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL,
                },
            },
    };
    auto const result = m_device->CreateBlendState(&description, &m_alphaBlendState);
    if (IS_ERROR(result)) throw Error{result, "Failed to create blend state"};
}

void BaseRenderer::createVertexShader() {
    auto const size = ARRAYSIZE(g_VertexShader);
    auto shader = &g_VertexShader[0];
    static constexpr ID3D11ClassLinkage *noLinkage = nullptr;
    auto result = m_device->CreateVertexShader(shader, size, noLinkage, &m_vertexShader);
    if (IS_ERROR(result)) throw Error{result, "Failed to create vertex shader"};

    static auto const layout = std::array{
        D3D11_INPUT_ELEMENT_DESC{"POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
        D3D11_INPUT_ELEMENT_DESC{"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 8, D3D11_INPUT_PER_VERTEX_DATA, 0}};
    auto const layout_size = static_cast<uint32_t>(layout.size());
    result = m_device->CreateInputLayout(layout.data(), layout_size, shader, size, &m_inputLayout);
    if (IS_ERROR(result)) throw Error{result, "Failed to create input layout"};
}

void BaseRenderer::createPlainPixelShader() {
    auto const size = ARRAYSIZE(g_PlainPixelShader);
    auto shader = &g_PlainPixelShader[0];
    static constexpr ID3D11ClassLinkage *noLinkage = nullptr;
    auto const result = m_device->CreatePixelShader(shader, size, noLinkage, &m_plainPixelShader);
    if (IS_ERROR(result)) throw Error{result, "Failed to create pixel shader"};
}
