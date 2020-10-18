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

    float zoom() const noexcept { return zoom_m; }
    auto offset() const noexcept -> POINT {
        return {static_cast<long>(offset_m.x), static_cast<long>(offset_m.y)};
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
    float zoom_m = 1.f;
    Vec2f offset_m = {0, 0};
    SIZE size_m{};

    bool pendingResizeBuffers_m = false;
    HWND windowHandle_m{};

    uint64_t lastPointerShapeUpdate_m = 0;
    uint64_t lastPointerPositionUpdate_m = 0;

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
            deviceContext()->ClearRenderTargetView(renderTarget_m.Get(), c.data());
        }
        void activateRenderTarget() {
            deviceContext()->OMSetRenderTargets(1, renderTarget_m.GetAddressOf(), nullptr);
        }

        void activateBackgroundTexture() {
            deviceContext()->PSSetShaderResources(
                0, 1, backgroundTextureShaderResource_m.GetAddressOf());
        }
        void activateBackgroundVertexBuffer() {
            const uint32_t stride = sizeof(Vertex);
            const uint32_t offset = 0;
            deviceContext()->IASetVertexBuffers(
                0, 1, backgroundVertexBuffer_m.GetAddressOf(), &stride, &offset);
        }

        void activateMaskedPixelShader() const {
            deviceContext()->PSSetShader(maskedPixelShader_m.Get(), nullptr, 0);
        }
        void activatePointerTexture(int index = 0) {
            deviceContext()->PSSetShaderResources(
                index, 1, pointerTextureShaderResource_m.GetAddressOf());
        }
        void activateLinearSampler(int index = 0) const {
            deviceContext()->PSSetSamplers(index, 1, linearSamplerState_m.GetAddressOf());
        }
        void activatePointerVertexBuffer() {
            const uint32_t stride = sizeof(Vertex);
            const uint32_t offset = 0;
            deviceContext()->IASetVertexBuffers(
                0, 1, pointerVertexBuffer_m.GetAddressOf(), &stride, &offset);
        }

        ComPtr<ID3D11Texture2D> backgroundTexture_m;
        ComPtr<ID3D11ShaderResourceView> backgroundTextureShaderResource_m;
        ComPtr<ID3D11Buffer> backgroundVertexBuffer_m;
        ComPtr<IDXGISwapChain1> swapChain_m;
        ComPtr<ID3D11RenderTargetView> renderTarget_m;
        ComPtr<ID3D11PixelShader> maskedPixelShader_m;
        ComPtr<ID3D11SamplerState> linearSamplerState_m;

        ComPtr<ID3D11Texture2D> pointerTexture_m;
        ComPtr<ID3D11ShaderResourceView> pointerTextureShaderResource_m;
        ComPtr<ID3D11Buffer> pointerVertexBuffer_m;
    };

    std::optional<Resources> dx_m;
};
