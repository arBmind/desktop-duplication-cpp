#pragma once
#include "stable.h"

#include "meta/comptr.h"

#include <array>
#include <cinttypes>
#include <gsl.h>

// shared between frame updater & window renderer
struct BaseRenderer {
    struct InitArgs {
        ComPtr<ID3D11Device> device;
        ComPtr<ID3D11DeviceContext> deviceContext;
    };
    struct Vertex {
        float x, y, u, v;
    };
    using Color = std::array<float, 4>;

    BaseRenderer(InitArgs &&args);
    ~BaseRenderer();

    BaseRenderer() = default;
    BaseRenderer(const BaseRenderer &) = default;
    BaseRenderer(BaseRenderer &&) = default;
    BaseRenderer &operator=(const BaseRenderer &) = default;
    BaseRenderer &operator=(BaseRenderer &&) = default;

    auto createShaderTexture(gsl::not_null<ID3D11Texture2D *> texture) const
        -> ComPtr<ID3D11ShaderResourceView>;
    auto createLinearSampler() -> ComPtr<ID3D11SamplerState>;

    auto device() const noexcept -> ID3D11Device * { return device_m.Get(); }
    auto deviceContext() const noexcept -> ID3D11DeviceContext * { return deviceContext_m.Get(); }

    void activateNoRenderTarget() const {
        deviceContext_m->OMSetRenderTargets(0, nullptr, nullptr);
    }

    void activateDiscreteSampler(int index = 0) const {
        deviceContext_m->PSSetSamplers(index, 1, discreteSamplerState_m.GetAddressOf());
    }

    void activateAlphaBlendState() const {
        const Color blendFactor{{0.f, 0.f, 0.f, 0.f}};
        const uint32_t sampleMask = 0xffffffff;
        deviceContext_m->OMSetBlendState(alphaBlendState_m.Get(), blendFactor.data(), sampleMask);
    }
    void activateNoBlendState() const {
        const Color blendFactor{{0.f, 0.f, 0.f, 0.f}};
        const uint32_t sampleMask = 0xffffffff;
        deviceContext_m->OMSetBlendState(nullptr, blendFactor.data(), sampleMask);
    }

    void activateVertexShader() const {
        deviceContext_m->VSSetShader(vertexShader_m.Get(), nullptr, 0);
        deviceContext_m->IASetInputLayout(inputLayout_m.Get());
    }
    void activatePlainPixelShader() const {
        deviceContext_m->PSSetShader(plainPixelShader_m.Get(), nullptr, 0);
    }

    void activateTriangleList() const {
        deviceContext_m->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    }

private:
    void createDiscreteSamplerState();
    void createAlphaBlendState();
    void createVertexShader();
    void createPlainPixelShader();

private:
    ComPtr<ID3D11Device> device_m;
    ComPtr<ID3D11DeviceContext> deviceContext_m;

    ComPtr<ID3D11SamplerState> discreteSamplerState_m;
    ComPtr<ID3D11BlendState> alphaBlendState_m;

    ComPtr<ID3D11VertexShader> vertexShader_m;
    ComPtr<ID3D11InputLayout> inputLayout_m;
    ComPtr<ID3D11PixelShader> plainPixelShader_m;
};
