#include "frame_updater.h"

#include "renderer.h"

#include "captured_update.h"
#include "frame_context.h"

namespace {
RECT rectMoveTo(const RECT &rect, const POINT &p) {
    auto size = rectSize(rect);
    return RECT{p.x, p.y, p.x + size.cx, p.y + size.cy};
}

RECT rotate(const RECT &rect, DXGI_MODE_ROTATION rotation, const SIZE &size) {
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

frame_updater::frame_updater(init_args &&args)
    : dx_m(std::move(args)) {}

void frame_updater::update(const frame_update &data, const frame_context &context) {
    performMoves(data, context);
    updateDirty(data, context);
}

void frame_updater::performMoves(const frame_update &data, const frame_context &context) {
    const auto moved = data.moved();
    if (moved.empty()) return;

    const auto desktop_size = rectSize(context.output_desc.DesktopCoordinates);

    if (!dx_m.moveTmp_m) {
        D3D11_TEXTURE2D_DESC target_description;
        dx_m.target_m->GetDesc(&target_description);

        D3D11_TEXTURE2D_DESC move_description;
        move_description = target_description;
        move_description.Width = desktop_size.cx;
        move_description.Height = desktop_size.cy;
        move_description.MipLevels = 1;
        move_description.BindFlags = D3D11_BIND_RENDER_TARGET;
        move_description.MiscFlags = 0;
        const auto result =
            dx_m.device()->CreateTexture2D(&move_description, nullptr, &dx_m.moveTmp_m);
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

        dx_m.deviceContext()->CopySubresourceRegion(
            dx_m.moveTmp_m.Get(), 0, source.left, source.top, 0, dx_m.target_m.Get(), 0, &box);

        box.left = source.left;
        box.top = source.top;
        box.right = source.right;
        box.bottom = source.bottom;

        dx_m.deviceContext()->CopySubresourceRegion(
            dx_m.target_m.Get(),
            0,
            dest.left + target_x,
            dest.top + target_y,
            0,
            dx_m.moveTmp_m.Get(),
            0,
            &box);
    }
}

void frame_updater::updateDirty(const frame_update &data, const frame_context &context) {
    auto dirts = data.dirty();
    if (dirts.empty()) return;

    auto *desktop = data.image.Get();

    ComPtr<ID3D11ShaderResourceView> shader_resource = dx_m.createShaderTexture(desktop);
    dx_m.deviceContext()->PSSetShaderResources(0, 1, shader_resource.GetAddressOf());

    D3D11_TEXTURE2D_DESC target_description;
    dx_m.target_m->GetDesc(&target_description);

    D3D11_TEXTURE2D_DESC desktop_description;
    desktop->GetDesc(&desktop_description);

    const auto target_x = context.output_desc.DesktopCoordinates.left - context.offset.x;
    const auto target_y = context.output_desc.DesktopCoordinates.top - context.offset.y;
    const auto desktop_size = rectSize(context.output_desc.DesktopCoordinates);
    const auto rotation = context.output_desc.Rotation;

    const auto center_x = static_cast<long>(target_description.Width) / 2;
    const auto center_y = static_cast<long>(target_description.Height) / 2;

    auto make_vertex = [&](int x, int y) {
        return vertex{(x + target_x - center_x) / static_cast<float>(center_x),
                      -1 * (y + target_y - center_y) / static_cast<float>(center_y),
                      x / static_cast<float>(desktop_description.Width),
                      y / static_cast<float>(desktop_description.Height)};
    };

    dirtyQuads_m.reserve(dirts.size());
    dirtyQuads_m.clear();
    for (const auto &dirt : dirts) {
        const auto rotated = rotate(dirt, rotation, desktop_size);
        quad_vertices vertices;

        vertices[0] = make_vertex(rotated.left, rotated.bottom);
        vertices[1] = make_vertex(rotated.left, rotated.top);
        vertices[2] = make_vertex(rotated.right, rotated.bottom);
        vertices[3] = vertices[2];
        vertices[4] = vertices[1];
        vertices[5] = make_vertex(rotated.right, rotated.top);

        dirtyQuads_m.push_back(vertices);
    }

    D3D11_BUFFER_DESC buffer_description;
    RtlZeroMemory(&buffer_description, sizeof(buffer_description));
    buffer_description.Usage = D3D11_USAGE_DEFAULT;
    buffer_description.ByteWidth = static_cast<uint32_t>(sizeof(quad_vertices) * dirts.size());
    buffer_description.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    buffer_description.CPUAccessFlags = 0;

    D3D11_SUBRESOURCE_DATA init_data;
    RtlZeroMemory(&init_data, sizeof(init_data));
    init_data.pSysMem = dirtyQuads_m.data();

    ComPtr<ID3D11Buffer> vertex_buffer;
    auto result = dx_m.device()->CreateBuffer(&buffer_description, &init_data, &vertex_buffer);
    if (IS_ERROR(result)) throw RenderFailure(result, "Failed to create dirty vertex buffer");

    uint32_t stride = sizeof(vertex);
    uint32_t offset = 0;
    dx_m.deviceContext()->IASetVertexBuffers(0, 1, vertex_buffer.GetAddressOf(), &stride, &offset);

    D3D11_VIEWPORT view_port;
    view_port.Width = static_cast<FLOAT>(target_description.Width);
    view_port.Height = static_cast<FLOAT>(target_description.Height);
    view_port.MinDepth = 0.0f;
    view_port.MaxDepth = 1.0f;
    view_port.TopLeftX = 0.0f;
    view_port.TopLeftY = 0.0f;
    dx_m.deviceContext()->RSSetViewports(1, &view_port);

    dx_m.deviceContext()->Draw(static_cast<uint32_t>(6 * dirts.size()), 0);

    // dx_m.activateNoRenderTarget();
    ID3D11ShaderResourceView *noResource = nullptr;
    dx_m.deviceContext()->PSSetShaderResources(0, 1, &noResource);
}

frame_updater::resources::resources(frame_updater::init_args &&args)
    : base_renderer(std::move(args)) {
    prepare(args.targetHandle);

    activateRenderTarget();
    activateTriangleList();

    activateVertexShader();
    activatePlainPixelShader();
    activateDiscreteSampler();
}

void frame_updater::resources::prepare(HANDLE targetHandle) {
    target_m = renderer::getTextureFromHandle(device(), targetHandle);
    renderTarget_m = renderer::renderToTexture(device(), target_m);
}
