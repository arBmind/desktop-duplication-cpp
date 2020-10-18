#pragma once
#include "stable.h"

#include "BaseRenderer.h"

#include <array>
#include <meta/comptr.h>
#include <vector>

struct FrameUpdate;
struct FrameContext;

struct RenderFailure {
    RenderFailure(HRESULT res, const char *text) noexcept {}
};

struct FrameUpdater {
    struct InitArgs : BaseRenderer::InitArgs {
        HANDLE targetHandle{}; // shared texture handle that should be updated
    };
    using Vertex = BaseRenderer::Vertex;
    using quad_vertices = std::array<Vertex, 6>;

    FrameUpdater(InitArgs &&args);

    void update(const FrameUpdate &data, const FrameContext &context);

private:
    void performMoves(const FrameUpdate &data, const FrameContext &context);
    void updateDirty(const FrameUpdate &data, const FrameContext &context);

private:
    struct Resources : BaseRenderer {
        Resources(FrameUpdater::InitArgs &&args);

        void prepare(HANDLE targetHandle);

        void activateRenderTarget() {
            deviceContext()->OMSetRenderTargets(1, renderTarget_m.GetAddressOf(), nullptr);
        }

        ComPtr<ID3D11Texture2D> target_m;
        ComPtr<ID3D11RenderTargetView> renderTarget_m;

        ComPtr<ID3D11Texture2D> moveTmp_m;
    };
    Resources dx_m;

    std::vector<quad_vertices> dirtyQuads_m;
};
