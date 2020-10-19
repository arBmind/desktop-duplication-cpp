#include "FrameUpdater.h"

#include "renderer.h"

#include "CapturedUpdate.h"
#include "FrameContext.h"
#include <gsl.h>

namespace {

auto rectMoveTo(const RECT &rect, const POINT &p) noexcept -> RECT {
    const auto size = rectSize(rect);
    return RECT{p.x, p.y, p.x + size.cx, p.y + size.cy};
}

auto rotate(const RECT &rect, DXGI_MODE_ROTATION rotation, const SIZE &size) noexcept -> RECT {
    switch (rotation) {
    case DXGI_MODE_ROTATION_UNSPECIFIED:
    case DXGI_MODE_ROTATION_IDENTITY: return rect;
    case DXGI_MODE_ROTATION_ROTATE90:
        return {size.cy - rect.bottom, rect.left, size.cy - rect.top, rect.right};
    case DXGI_MODE_ROTATION_ROTATE180:
        return {
            size.cx - rect.right, size.cy - rect.bottom, size.cx - rect.left, size.cy - rect.top};
    case DXGI_MODE_ROTATION_ROTATE270:
        return {rect.top, size.cx - rect.right, rect.bottom, size.cx - rect.left};
    default: return {0, 0, 0, 0};
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

    const auto desktop_size = rectSize(context.output_desc.DesktopCoordinates);

    if (!m_dx.moveTmp) {
        D3D11_TEXTURE2D_DESC target_description;
        m_dx.target->GetDesc(&target_description);

        D3D11_TEXTURE2D_DESC move_description;
        move_description = target_description;
        move_description.Width = desktop_size.cx;
        move_description.Height = desktop_size.cy;
        move_description.MipLevels = 1;
        move_description.BindFlags = D3D11_BIND_RENDER_TARGET;
        move_description.MiscFlags = 0;
        const auto result =
            m_dx.device()->CreateTexture2D(&move_description, nullptr, &m_dx.moveTmp);
        if (IS_ERROR(result))
            throw RenderFailure(result, "Failed to create move temporary texture");
    }

    const auto target_x = context.output_desc.DesktopCoordinates.left - context.offset.x;
    const auto target_y = context.output_desc.DesktopCoordinates.top - context.offset.y;
    const auto rotation = context.output_desc.Rotation;

    for (auto &move : moved) {
        const auto source =
            rotate(rectMoveTo(move.DestinationRect, move.SourcePoint), rotation, desktop_size);
        const auto dest = rotate(move.DestinationRect, rotation, desktop_size);

        D3D11_BOX box;
        box.left = source.left + target_x;
        box.top = source.top + target_y;
        box.front = 0;
        box.right = source.right + target_x;
        box.bottom = source.bottom + target_y;
        box.back = 1;

        m_dx.deviceContext()->CopySubresourceRegion(
            m_dx.moveTmp.Get(), 0, source.left, source.top, 0, m_dx.target.Get(), 0, &box);

        box.left = source.left;
        box.top = source.top;
        box.right = source.right;
        box.bottom = source.bottom;

        m_dx.deviceContext()->CopySubresourceRegion(
            m_dx.target.Get(),
            0,
            dest.left + target_x,
            dest.top + target_y,
            0,
            m_dx.moveTmp.Get(),
            0,
            &box);
    }
}

void FrameUpdater::updateDirty(const FrameUpdate &data, const FrameContext &context) {
    auto dirts = data.dirty();
    if (dirts.empty()) return;

    const auto desktop = gsl::not_null<ID3D11Texture2D *>(data.image.Get());

    ComPtr<ID3D11ShaderResourceView> shader_resource = m_dx.createShaderTexture(desktop);
    m_dx.deviceContext()->PSSetShaderResources(0, 1, shader_resource.GetAddressOf());

    D3D11_TEXTURE2D_DESC target_description;
    m_dx.target->GetDesc(&target_description);

    D3D11_TEXTURE2D_DESC desktop_description;
    desktop->GetDesc(&desktop_description);

    const auto target_x = context.output_desc.DesktopCoordinates.left - context.offset.x;
    const auto target_y = context.output_desc.DesktopCoordinates.top - context.offset.y;
    const auto desktop_size = rectSize(context.output_desc.DesktopCoordinates);
    const auto rotation = context.output_desc.Rotation;

    const auto center_x = gsl::narrow_cast<long>(target_description.Width) / 2;
    const auto center_y = gsl::narrow_cast<long>(target_description.Height) / 2;

    const auto make_vertex = [&](int x, int y) {
        return Vertex{
            (x + target_x - center_x) / static_cast<float>(center_x),
            -1 * (y + target_y - center_y) / static_cast<float>(center_y),
            x / static_cast<float>(desktop_description.Width),
            y / static_cast<float>(desktop_description.Height)};
    };

    m_dirtyQuads.reserve(dirts.size());
    m_dirtyQuads.clear();
    for (const auto &dirt : dirts) {
        const auto rotated = rotate(dirt, rotation, desktop_size);
        quad_vertices vertices;

        vertices[0] = make_vertex(rotated.left, rotated.bottom);
        vertices[1] = make_vertex(rotated.left, rotated.top);
        vertices[2] = make_vertex(rotated.right, rotated.bottom);
        vertices[3] = vertices[2];
        vertices[4] = vertices[1];
        vertices[5] = make_vertex(rotated.right, rotated.top);

        m_dirtyQuads.push_back(vertices);
    }

    D3D11_BUFFER_DESC buffer_description;
    RtlZeroMemory(&buffer_description, sizeof(buffer_description));
    buffer_description.Usage = D3D11_USAGE_DEFAULT;
    [[gsl::suppress("26472")]] // conversion required because of APIs
    buffer_description.ByteWidth = static_cast<uint32_t>(sizeof(quad_vertices) * dirts.size());
    buffer_description.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    buffer_description.CPUAccessFlags = 0;

    D3D11_SUBRESOURCE_DATA init_data;
    RtlZeroMemory(&init_data, sizeof(init_data));
    init_data.pSysMem = m_dirtyQuads.data();

    ComPtr<ID3D11Buffer> vertex_buffer;
    const auto result =
        m_dx.device()->CreateBuffer(&buffer_description, &init_data, &vertex_buffer);
    if (IS_ERROR(result)) throw RenderFailure(result, "Failed to create dirty vertex buffer");

    const uint32_t stride = sizeof(Vertex);
    const uint32_t offset = 0;
    m_dx.deviceContext()->IASetVertexBuffers(0, 1, vertex_buffer.GetAddressOf(), &stride, &offset);

    D3D11_VIEWPORT view_port;
    view_port.Width = static_cast<FLOAT>(target_description.Width);
    view_port.Height = static_cast<FLOAT>(target_description.Height);
    view_port.MinDepth = 0.0f;
    view_port.MaxDepth = 1.0f;
    view_port.TopLeftX = 0.0f;
    view_port.TopLeftY = 0.0f;
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
