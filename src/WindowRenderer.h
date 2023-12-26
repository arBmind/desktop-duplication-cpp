#pragma once
#include "BaseRenderer.h"
#include "win32/Geometry.h"
#include "win32/Handle.h"

#include <dxgi1_3.h>

#include <optional>

struct PointerBuffer;

using win32::Dimension;
using win32::Handle;
using win32::Point;
using win32::Vec2f;

// manages state how to render background & pointer to output window
struct WindowRenderer {
    using SetErrorFunc = void(void *, std::exception_ptr);
    struct Args {
        PointerBuffer const &pointerBuffer;
        float outputZoom{1.0f};
        Vec2f captureOffset{};

        SetErrorFunc *setErrorCallback{&WindowRenderer::noopSetErrorCallback};
        void *callbackPtr{};

        /// note: callbacks are invoked inside the RenderThread!
        template<class T>
        void setCallbacks(T *p) {
            callbackPtr = p;
            setErrorCallback = [](void *ptr, std::exception_ptr exception) {
                auto *cb = reinterpret_cast<T *>(ptr);
                cb->setError(exception);
            };
        }
    };
    struct InitArgs {
        BaseRenderer::InitArgs basic{};
        HWND windowHandle{}; // window to render to
        Dimension windowDimension{};
        ComPtr<ID3D11Texture2D> texture; // texture is rendered as quad
    };
    using Vertex = BaseRenderer::Vertex;

    WindowRenderer(Args const &);

    auto isInitialized() const -> bool { return m_dx.has_value(); }

    void init(InitArgs &&args);
    void reset() noexcept;

    auto frameLatencyWaitable() -> Handle;

    bool resize(Dimension size) noexcept;
    void zoomOutput(float zoom) noexcept;
    void updateOffset(Vec2f offset) noexcept;
    void updateHideFrame(bool) noexcept;

    void render();

private:
    void renderBlack();
    void renderFrame();
    void renderPointer();
    void swap();

    void resizeSwapBuffer();
    void setViewPort();
    void updatePointerShape(const PointerBuffer &pointer);
    void updatePointerVertices(const PointerBuffer &pointer);

    static void noopSetErrorCallback(void *, std::exception_ptr) {}

private:
    Args m_args;
    Dimension m_size{};

    bool m_pendingResizeBuffers = false;

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

        void clearRenderTarget(const Color &c) { deviceContext()->ClearRenderTargetView(renderTarget.Get(), c.data()); }
        void activateRenderTarget() { deviceContext()->OMSetRenderTargets(1, renderTarget.GetAddressOf(), nullptr); }

        void activateBackgroundTexture() {
            deviceContext()->PSSetShaderResources(0, 1, backgroundTextureShaderResource.GetAddressOf());
        }
        void activateBackgroundVertexBuffer() {
            const uint32_t stride = sizeof(Vertex);
            const uint32_t offset = 0;
            deviceContext()->IASetVertexBuffers(0, 1, backgroundVertexBuffer.GetAddressOf(), &stride, &offset);
        }

        void activateMaskedPixelShader() const { deviceContext()->PSSetShader(maskedPixelShader.Get(), nullptr, 0); }
        void activatePointerTexture(int index = 0) {
            deviceContext()->PSSetShaderResources(index, 1, pointerTextureShaderResource.GetAddressOf());
        }
        void activateLinearSampler(int index = 0) const {
            deviceContext()->PSSetSamplers(index, 1, linearSamplerState.GetAddressOf());
        }
        void activatePointerVertexBuffer() {
            const uint32_t stride = sizeof(Vertex);
            const uint32_t offset = 0;
            deviceContext()->IASetVertexBuffers(0, 1, pointerVertexBuffer.GetAddressOf(), &stride, &offset);
        }

        ComPtr<ID3D11Texture2D> backgroundTexture;
        ComPtr<ID3D11ShaderResourceView> backgroundTextureShaderResource;
        ComPtr<ID3D11Buffer> backgroundVertexBuffer;
        ComPtr<IDXGISwapChain2> swapChain;
        ComPtr<ID3D11RenderTargetView> renderTarget;
        ComPtr<ID3D11PixelShader> maskedPixelShader;
        ComPtr<ID3D11SamplerState> linearSamplerState;

        ComPtr<ID3D11Texture2D> pointerTexture;
        ComPtr<ID3D11ShaderResourceView> pointerTextureShaderResource;
        ComPtr<ID3D11Buffer> pointerVertexBuffer;
    };

    std::optional<Resources> m_dx;
};
