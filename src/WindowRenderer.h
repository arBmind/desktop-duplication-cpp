#pragma once
#include "stable.h"

#include "BaseRenderer.h"

#include <optional>

struct PointerBuffer;

// manages state how to render background & pointer to output window
struct WindowRenderer {
    struct InitArgs : BaseRenderer::InitArgs {
        HWND windowHandle{}; // window to render to
        ComPtr<ID3D11Texture2D> texture; // texture is rendered as quad
    };
    using Vertex = BaseRenderer::Vertex;
    struct Vec2f {
        float x, y;
    };

    void init(InitArgs &&args);
    void reset() noexcept;

    float zoom() const noexcept { return m_zoom; }
    auto offset() const noexcept -> POINT {
        return {static_cast<long>(m_offset.x), static_cast<long>(m_offset.y)};
    }

    bool resize(SIZE size) noexcept;
    void setZoom(float zoom) noexcept;
    void moveOffset(POINT delta) noexcept;
    void moveToBorder(int x, int y);
    void moveTo(Vec2f offset) noexcept;

    void render();
    void renderPointer(const PointerBuffer &pointer);
    void swap();

private:
    void resizeSwapBuffer();
    void setViewPort();
    void updatePointerShape(const PointerBuffer &pointer);
    void updatePointerVertices(const PointerBuffer &pointer);

private:
    float m_zoom = 1.f;
    Vec2f m_offset = {0, 0};
    SIZE m_size{};

    bool m_pendingResizeBuffers = false;
    HWND m_windowHandle{};

    uint64_t m_lastPointerShapeUpdate = 0;
    uint64_t m_lastPointerPositionUpdate = 0;

    struct Resources : BaseRenderer {
        Resources(WindowRenderer::InitArgs &&args);

    private:
        void createBackgroundTextureShaderResource();
        void createBackgroundVertexBuffer();
        void createSwapChain(HWND windowHandle);
        void createMaskedPixelShader();
        void createLinearSamplerState();
        void createPointerVertexBuffer();

    public:
        void createRenderTarget();

        void clearRenderTarget(const Color &c) {
            deviceContext()->ClearRenderTargetView(renderTarget.Get(), c.data());
        }
        void activateRenderTarget() {
            deviceContext()->OMSetRenderTargets(1, renderTarget.GetAddressOf(), nullptr);
        }

        void activateBackgroundTexture() {
            deviceContext()->PSSetShaderResources(
                0, 1, backgroundTextureShaderResource.GetAddressOf());
        }
        void activateBackgroundVertexBuffer() {
            const uint32_t stride = sizeof(Vertex);
            const uint32_t offset = 0;
            deviceContext()->IASetVertexBuffers(
                0, 1, backgroundVertexBuffer.GetAddressOf(), &stride, &offset);
        }

        void activateMaskedPixelShader() const {
            deviceContext()->PSSetShader(maskedPixelShader.Get(), nullptr, 0);
        }
        void activatePointerTexture(int index = 0) {
            deviceContext()->PSSetShaderResources(
                index, 1, pointerTextureShaderResource.GetAddressOf());
        }
        void activateLinearSampler(int index = 0) const {
            deviceContext()->PSSetSamplers(index, 1, linearSamplerState.GetAddressOf());
        }
        void activatePointerVertexBuffer() {
            const uint32_t stride = sizeof(Vertex);
            const uint32_t offset = 0;
            deviceContext()->IASetVertexBuffers(
                0, 1, pointerVertexBuffer.GetAddressOf(), &stride, &offset);
        }

        ComPtr<ID3D11Texture2D> backgroundTexture;
        ComPtr<ID3D11ShaderResourceView> backgroundTextureShaderResource;
        ComPtr<ID3D11Buffer> backgroundVertexBuffer;
        ComPtr<IDXGISwapChain1> swapChain;
        ComPtr<ID3D11RenderTargetView> renderTarget;
        ComPtr<ID3D11PixelShader> maskedPixelShader;
        ComPtr<ID3D11SamplerState> linearSamplerState;

        ComPtr<ID3D11Texture2D> pointerTexture;
        ComPtr<ID3D11ShaderResourceView> pointerTextureShaderResource;
        ComPtr<ID3D11Buffer> pointerVertexBuffer;
    };

    std::optional<Resources> m_dx;
};
