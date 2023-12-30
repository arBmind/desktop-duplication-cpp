#include "FrameUpdater.h"

#include "renderer.h"

#include "CapturedUpdate.h"
#include "FrameContext.h"

namespace {

using win32::Dimension;
using win32::Point;
using win32::Rect;

auto rotate(Rect rect, DXGI_MODE_ROTATION rotation, Dimension spaceDim) noexcept -> Rect {
    switch (rotation) {
    case DXGI_MODE_ROTATION_UNSPECIFIED:
    case DXGI_MODE_ROTATION_IDENTITY: return rect;
    case DXGI_MODE_ROTATION_ROTATE90:
        return {
            Point{spaceDim.height - rect.bottom(), rect.left()},
            Dimension{rect.height(), rect.width()},
        };
    case DXGI_MODE_ROTATION_ROTATE180:
        return {
            Point{spaceDim.width - rect.right(), spaceDim.height - rect.bottom()},
            rect.dimension,
        };
    case DXGI_MODE_ROTATION_ROTATE270:
        return {
            Point{rect.top(), spaceDim.width - rect.right()},
            Dimension{rect.height(), rect.width()},
        };
    default: return {};
    }
}

} // namespace

FrameUpdater::FrameUpdater(InitArgs &&args)
    : m_dx(std::move(args)) {}

void FrameUpdater::update(const FrameUpdate &data, const FrameContext &context) {
    performMoves(data, context);
    updateDirty(data, context);
}

void FrameUpdater::performMoves(const FrameUpdate &data, const FrameContext &context) {
    const auto moved = data.moved();
    if (moved.empty()) return;

    const auto desktopRect = Rect::fromRECT(context.output_desc.DesktopCoordinates);
    const auto &desktopDim = desktopRect.dimension;

    if (!m_dx.moveTmp) {
        auto target_description = D3D11_TEXTURE2D_DESC{};
        m_dx.target->GetDesc(&target_description);

        auto move_description = D3D11_TEXTURE2D_DESC{
            .Width = static_cast<UINT>(desktopDim.width),
            .Height = static_cast<UINT>(desktopDim.height),
            .MipLevels = 1,
            .ArraySize = target_description.ArraySize,
            .Format = target_description.Format,
            .SampleDesc = target_description.SampleDesc,
            .Usage = target_description.Usage,
            .BindFlags = D3D11_BIND_RENDER_TARGET,
            .CPUAccessFlags = target_description.CPUAccessFlags,
            .MiscFlags = 0,
        };
        const auto result = m_dx.device()->CreateTexture2D(&move_description, nullptr, &m_dx.moveTmp);
        if (IS_ERROR(result)) throw RenderFailure(result, "Failed to create move temporary texture");
    }

    const auto target_x = context.output_desc.DesktopCoordinates.left - context.offset.x;
    const auto target_y = context.output_desc.DesktopCoordinates.top - context.offset.y;
    const auto rotation = context.output_desc.Rotation;

    for (auto &move : moved) {
        auto moveDestinationRect = Rect::fromRECT(move.DestinationRect);
        auto moveSourcePoint = Point::fromPOINT(move.SourcePoint);

        const auto sourceRect = rotate(Rect{moveSourcePoint, moveDestinationRect.dimension}, rotation, desktopDim);
        const auto dest = rotate(moveDestinationRect, rotation, desktopDim);

        auto box = D3D11_BOX{
            .left = static_cast<UINT>(sourceRect.left() + target_x),
            .top = static_cast<UINT>(sourceRect.top() + target_y),
            .front = 0,
            .right = static_cast<UINT>(sourceRect.right() + target_x),
            .bottom = static_cast<UINT>(sourceRect.bottom() + target_y),
            .back = 1,
        };

        m_dx.deviceContext()->CopySubresourceRegion(
            m_dx.moveTmp.Get(), 0, sourceRect.left(), sourceRect.top(), 0, m_dx.target.Get(), 0, &box);

        box.left = sourceRect.left();
        box.top = sourceRect.top();
        box.right = sourceRect.right();
        box.bottom = sourceRect.bottom();

        m_dx.deviceContext()->CopySubresourceRegion(
            m_dx.target.Get(), 0, dest.left() + target_x, dest.top() + target_y, 0, m_dx.moveTmp.Get(), 0, &box);
    }
}

void FrameUpdater::updateDirty(const FrameUpdate &data, const FrameContext &context) {
    auto dirts = data.dirty();
    if (dirts.empty()) return;

    const auto desktop = data.image.Get();

    ComPtr<ID3D11ShaderResourceView> shader_resource = m_dx.createShaderTexture(desktop);
    m_dx.deviceContext()->PSSetShaderResources(0, 1, shader_resource.GetAddressOf());

    auto target_description = D3D11_TEXTURE2D_DESC{};
    m_dx.target->GetDesc(&target_description);

    auto desktop_description = D3D11_TEXTURE2D_DESC{};
    desktop->GetDesc(&desktop_description);

    const auto target_x = context.output_desc.DesktopCoordinates.left - context.offset.x;
    const auto target_y = context.output_desc.DesktopCoordinates.top - context.offset.y;
    const auto desktopRect = Rect::fromRECT(context.output_desc.DesktopCoordinates);
    const auto rotation = context.output_desc.Rotation;

    const auto center_x = static_cast<long>(target_description.Width) / 2;
    const auto center_y = static_cast<long>(target_description.Height) / 2;

    const auto make_vertex = [&](int x, int y) {
        return Vertex{
            (x + target_x - center_x) / static_cast<float>(center_x),
            -1 * (y + target_y - center_y) / static_cast<float>(center_y),
            x / static_cast<float>(desktop_description.Width),
            y / static_cast<float>(desktop_description.Height)};
    };

    auto vertexBufferSize = static_cast<uint32_t>(sizeof(quad_vertices) * dirts.size());
    if (m_dx.vertexBufferSize < vertexBufferSize) {
        auto buffer_description = D3D11_BUFFER_DESC{
            .ByteWidth = vertexBufferSize,
            .Usage = D3D11_USAGE_DYNAMIC,
            .BindFlags = D3D11_BIND_VERTEX_BUFFER,
            .CPUAccessFlags = D3D11_CPU_ACCESS_WRITE,
            .MiscFlags = {},
            .StructureByteStride = {},
        };
        const auto result = m_dx.device()->CreateBuffer(&buffer_description, nullptr, &m_dx.vertexBuffer);
        if (IS_ERROR(result)) throw RenderFailure(result, "Failed to create dirty vertex buffer");
        m_dx.vertexBufferSize = vertexBufferSize;
    }

    auto mapped = D3D11_MAPPED_SUBRESOURCE{};
    const auto subresource = 0;
    const auto mapType = D3D11_MAP_WRITE_DISCARD;
    const unsigned mapFlags = 0;
    m_dx.deviceContext()->Map(m_dx.vertexBuffer.Get(), subresource, mapType, mapFlags, &mapped);

    auto *vertex_ptr = static_cast<quad_vertices *>(mapped.pData);

    for (const auto &dirt : dirts) {
        auto dirtRect = Rect::fromRECT(dirt);
        const auto rotated = rotate(dirtRect, rotation, desktopRect.dimension);
        auto &vertices = *vertex_ptr;

        vertices[0] = make_vertex(rotated.left(), rotated.bottom());
        vertices[1] = make_vertex(rotated.left(), rotated.top());
        vertices[2] = make_vertex(rotated.right(), rotated.bottom());
        vertices[3] = vertices[2];
        vertices[4] = vertices[1];
        vertices[5] = make_vertex(rotated.right(), rotated.top());
        vertex_ptr++;
    }
    m_dx.deviceContext()->Unmap(m_dx.vertexBuffer.Get(), 0);

    const uint32_t stride = sizeof(Vertex);
    const uint32_t offset = 0;
    m_dx.deviceContext()->IASetVertexBuffers(0, 1, m_dx.vertexBuffer.GetAddressOf(), &stride, &offset);

    auto view_port = D3D11_VIEWPORT{
        .TopLeftX = 0.f,
        .TopLeftY = 0.f,
        .Width = static_cast<FLOAT>(target_description.Width),
        .Height = static_cast<FLOAT>(target_description.Height),
        .MinDepth = 0.f,
        .MaxDepth = 1.f,
    };
    m_dx.deviceContext()->RSSetViewports(1, &view_port);

    [[gsl::suppress("26472")]] // conversion required because of APIs
    m_dx.deviceContext()
        ->Draw(static_cast<uint32_t>(6 * dirts.size()), 0);

    // dx_m.activateNoRenderTarget();
    ID3D11ShaderResourceView *noResource = nullptr;
    m_dx.deviceContext()->PSSetShaderResources(0, 1, &noResource);
}

FrameUpdater::Resources::Resources(FrameUpdater::InitArgs &&args)
    : BaseRenderer(std::move(args)) {
    prepare(args.targetHandle);

    activateRenderTarget();
    activateTriangleList();

    activateVertexShader();
    activatePlainPixelShader();
    activateDiscreteSampler();
}

void FrameUpdater::Resources::prepare(HANDLE targetHandle) {
    target = renderer::getTextureFromHandle(device(), targetHandle);
    renderTarget = renderer::renderToTexture(device(), target);
}
