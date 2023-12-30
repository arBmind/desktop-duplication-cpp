#pragma once
#include "meta/comptr.h"

#include <d3d11.h>

#include <array>

// shared between frame updater & window renderer
struct BaseRenderer {
    struct InitArgs {
        ComPtr<ID3D11Device> device{};
        ComPtr<ID3D11DeviceContext> deviceContext{};
    };
    struct Vertex {
        float x{};
        float y{};
        float u{};
        float v{};
    };
    using Color = std::array<float, 4>;

    BaseRenderer(InitArgs &&args);
    ~BaseRenderer();

    BaseRenderer() = default;
    BaseRenderer(const BaseRenderer &) = default;
    BaseRenderer(BaseRenderer &&) = default;
    BaseRenderer &operator=(const BaseRenderer &) = default;
    BaseRenderer &operator=(BaseRenderer &&) = default;

    auto createShaderTexture(ID3D11Texture2D *texture) const -> ComPtr<ID3D11ShaderResourceView>;
    auto createLinearSampler() -> ComPtr<ID3D11SamplerState>;

    auto device() const noexcept -> ID3D11Device * { return m_device.Get(); }
    auto deviceContext() const noexcept -> ID3D11DeviceContext * { return m_deviceContext.Get(); }

    void activateNoRenderTarget() const { m_deviceContext->OMSetRenderTargets(0, nullptr, nullptr); }

    void activateDiscreteSampler(int index = 0) const {
        m_deviceContext->PSSetSamplers(index, 1, m_discreteSamplerState.GetAddressOf());
    }

    void activateAlphaBlendState() const {
        auto const blendFactor = Color{0.f, 0.f, 0.f, 0.f};
        auto const sampleMask = uint32_t{0xffffffffu};
        m_deviceContext->OMSetBlendState(m_alphaBlendState.Get(), blendFactor.data(), sampleMask);
    }
    void activateNoBlendState() const {
        auto const blendFactor = Color{0.f, 0.f, 0.f, 0.f};
        auto const sampleMask = uint32_t{0xffffffffu};
        m_deviceContext->OMSetBlendState(nullptr, blendFactor.data(), sampleMask);
    }

    void activateVertexShader() const {
        m_deviceContext->VSSetShader(m_vertexShader.Get(), nullptr, 0);
        m_deviceContext->IASetInputLayout(m_inputLayout.Get());
    }
    void activatePlainPixelShader() const { m_deviceContext->PSSetShader(m_plainPixelShader.Get(), nullptr, 0); }

    void activateTriangleList() const {
        m_deviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    }

private:
    void createDiscreteSamplerState();
    void createAlphaBlendState();
    void createVertexShader();
    void createPlainPixelShader();

private:
    ComPtr<ID3D11Device> m_device{};
    ComPtr<ID3D11DeviceContext> m_deviceContext{};

    ComPtr<ID3D11SamplerState> m_discreteSamplerState{};
    ComPtr<ID3D11BlendState> m_alphaBlendState{};

    ComPtr<ID3D11VertexShader> m_vertexShader{};
    ComPtr<ID3D11InputLayout> m_inputLayout{};
    ComPtr<ID3D11PixelShader> m_plainPixelShader{};
};
