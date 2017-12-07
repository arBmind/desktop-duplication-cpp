#include "base_renderer.h"

#include "renderer.h"

#include "meta/array.h"

#include "PlainPixelShader.h"
#include "VertexShader.h"

using error = renderer::error;

base_renderer::base_renderer(init_args &&args)
    : device_m(std::move(args.device))
    , deviceContext_m(std::move(args.deviceContext)) {
    createDiscreteSamplerState();
    createAlphaBlendState();
    createVertexShader();
    createPlainPixelShader();
}

base_renderer::~base_renderer() {
    // ensure device is clean, so we can create a new one!
    if (deviceContext_m) {
        deviceContext_m->ClearState();
        deviceContext_m->Flush();
    }
}

ComPtr<ID3D11ShaderResourceView>
base_renderer::createShaderTexture(gsl::not_null<ID3D11Texture2D *> texture) const {
    D3D11_TEXTURE2D_DESC description;
    texture->GetDesc(&description);

    D3D11_SHADER_RESOURCE_VIEW_DESC shader_resource_description;
    shader_resource_description.Format = description.Format;
    shader_resource_description.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    shader_resource_description.Texture2D.MostDetailedMip = description.MipLevels - 1;
    shader_resource_description.Texture2D.MipLevels = description.MipLevels;

    ComPtr<ID3D11ShaderResourceView> shader_resource;
    const auto result =
        device_m->CreateShaderResourceView(texture, &shader_resource_description, &shader_resource);
    if (IS_ERROR(result)) throw error{result, "Failed to create texture resource"};
    return shader_resource;
}

ComPtr<ID3D11SamplerState> base_renderer::createLinearSampler() {
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
    const auto result = device_m->CreateSamplerState(&descrption, &sampler);
    if (IS_ERROR(result)) throw error{result, "Failed to create linear sampler state"};
    return sampler;
}

void base_renderer::createDiscreteSamplerState() {
    D3D11_SAMPLER_DESC descrption;
    RtlZeroMemory(&descrption, sizeof(descrption));
    descrption.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
    descrption.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
    descrption.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
    descrption.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    descrption.ComparisonFunc = D3D11_COMPARISON_NEVER;
    descrption.MinLOD = 0;
    descrption.MaxLOD = D3D11_FLOAT32_MAX;
    const auto result = device_m->CreateSamplerState(&descrption, &discreteSamplerState_m);
    if (IS_ERROR(result)) throw error{result, "Failed to create discrete sampler state"};
}

void base_renderer::createAlphaBlendState() {
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
    const auto result = device_m->CreateBlendState(&description, &alphaBlendState_m);
    if (IS_ERROR(result)) throw error{result, "Failed to create blend state"};
}

void base_renderer::createVertexShader() {
    const auto size = ARRAYSIZE(g_VertexShader);
    auto shader = &g_VertexShader[0];
    auto result = device_m->CreateVertexShader(shader, size, nullptr, &vertexShader_m);
    if (IS_ERROR(result)) throw error{result, "Failed to create vertex shader"};

    static const auto layout = make_array(
        D3D11_INPUT_ELEMENT_DESC{
            "POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
        D3D11_INPUT_ELEMENT_DESC{
            "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 8, D3D11_INPUT_PER_VERTEX_DATA, 0});
    const auto layout_size = layout.size();
    result = device_m->CreateInputLayout(
        layout.data(),
        reinterpret_cast<const uint32_t &>(layout_size),
        shader,
        size,
        &inputLayout_m);
    if (IS_ERROR(result)) throw error{result, "Failed to create input layout"};
}

void base_renderer::createPlainPixelShader() {
    const auto size = ARRAYSIZE(g_PlainPixelShader);
    auto shader = &g_PlainPixelShader[0];
    ID3D11ClassLinkage *linkage = nullptr;
    const auto result = device_m->CreatePixelShader(shader, size, linkage, &plainPixelShader_m);
    if (IS_ERROR(result)) throw error{result, "Failed to create pixel shader"};
}
