#include "WindowRenderer.h"
#include "PointerUpdater.h"

#include "renderer.h"

#include "MaskedPixelShader.h"

#include <array>
#include <memory>

using Error = renderer::Error;
using win32::Rect;

void WindowRenderer::init(InitArgs &&args) {
    m_dx.emplace(std::move(args));
    m_windowHandle = args.windowHandle;

    RECT rect;
    ::GetClientRect(m_windowHandle, &rect);
    m_size = Rect::fromRECT(rect).dimension;
}

void WindowRenderer::reset() noexcept { m_dx.reset(); }

auto WindowRenderer::frameLatencyWaitable() -> Handle {
    return Handle{m_dx->swapChain->GetFrameLatencyWaitableObject()};
}

bool WindowRenderer::resize(Dimension size) noexcept {
    if (size == m_size) return false;
    m_size = size;
    m_pendingResizeBuffers = true;
    return true;
}

void WindowRenderer::setZoom(float zoom) noexcept {
    if (m_zoom == zoom) return;
    m_zoom = zoom;
}

// void WindowRenderer::moveToBorder(int x, int y) {
//     auto next = m_offset;

//     const auto &dx = *m_dx;
//     D3D11_TEXTURE2D_DESC texture_description;
//     dx.backgroundTexture->GetDesc(&texture_description);

//     if (0 > x)
//         next.x = 0;
//     else if (0 < x)
//         next.x = -static_cast<float>(texture_description.Width) + (m_size.width / m_zoom);

//     if (0 > y)
//         next.y = 0;
//     else if (0 < y)
//         next.y = -static_cast<float>(texture_description.Height) + (m_size.height / m_zoom);

//     setOffset(next);
// }

void WindowRenderer::setOffset(Vec2f offset) noexcept {
    if (offset.x == m_offset.x && offset.y == m_offset.y) return;
    m_offset = offset;
}

void WindowRenderer::render() {
    auto &dx = *m_dx;
    if (m_pendingResizeBuffers) {
        m_pendingResizeBuffers = false;
        dx.renderTarget.Reset();
        resizeSwapBuffer();
        dx.createRenderTarget();
    }

    auto black = BaseRenderer::Color{0, 0, 0, 0};
    dx.clearRenderTarget(black);

    // dx.deviceContext()->GenerateMips(dx.backgroundTextureShaderResource.Get());

    setViewPort();
    dx.activateRenderTarget();

    dx.activateLinearSampler();
    dx.activateVertexShader();
    dx.activateDiscreteSampler();
    dx.activatePlainPixelShader();
    dx.activateBackgroundTexture();
    dx.activateNoBlendState();
    dx.activateTriangleList();
    dx.activateBackgroundVertexBuffer();
    dx.deviceContext()->Draw(6, 0);
}

void WindowRenderer::renderPointer(const PointerBuffer &pointer) {
    if (pointer.position_timestamp == 0) return;
    updatePointerShape(pointer);
    if (!pointer.visible) return;
    updatePointerVertices(pointer);

    auto &dx = *m_dx;
    dx.activatePointerVertexBuffer();

    if (pointer.shape_info.Type == DXGI_OUTDUPL_POINTER_SHAPE_TYPE_COLOR) {
        dx.activateAlphaBlendState();
        dx.activatePlainPixelShader();
        dx.activatePointerTexture();
    }
    else {
        const auto index = 1;
        // dx.activateNoBlendState(); -- already set
        dx.activatePointerTexture(index);
        dx.activateDiscreteSampler(index);
        dx.activateMaskedPixelShader();
    }

    dx.deviceContext()->Draw(6, 0);
}

void WindowRenderer::swap() {
    const auto &dx = *m_dx;
    dx.activateNoRenderTarget();

    const auto result = dx.swapChain->Present(1, 0);
    if (IS_ERROR(result)) throw Error{result, "Failed to swap buffers"};
}

void WindowRenderer::resizeSwapBuffer() {
    const auto &dx = *m_dx;
    DXGI_SWAP_CHAIN_DESC description;
    dx.swapChain->GetDesc(&description);
    const auto result = dx.swapChain->ResizeBuffers(
        description.BufferCount,
        m_size.width,
        m_size.height,
        description.BufferDesc.Format,
        description.Flags);
    if (IS_ERROR(result)) throw Error{result, "Failed to resize swap buffers"};
}

void WindowRenderer::setViewPort() {
    const auto &dx = *m_dx;
    D3D11_TEXTURE2D_DESC texture_description;
    dx.backgroundTexture->GetDesc(&texture_description);

    D3D11_VIEWPORT view_port;
    view_port.Width = static_cast<float>(texture_description.Width) * m_zoom;
    view_port.Height = static_cast<float>(texture_description.Height) * m_zoom;
    view_port.MinDepth = 0.0f;
    view_port.MaxDepth = 1.0f;
    view_port.TopLeftX = static_cast<float>(m_offset.x) * m_zoom;
    view_port.TopLeftY = static_cast<float>(m_offset.y) * m_zoom;

    dx.deviceContext()->RSSetViewports(1, &view_port);
}

void WindowRenderer::updatePointerShape(const PointerBuffer &pointer) {
    if (pointer.shape_timestamp == m_lastPointerShapeUpdate) return;
    m_lastPointerShapeUpdate = pointer.shape_timestamp;

    D3D11_TEXTURE2D_DESC texture_description;
    texture_description.Width = pointer.shape_info.Width;
    texture_description.Height = pointer.shape_info.Height;
    texture_description.MipLevels = 1;
    texture_description.ArraySize = 1;
    texture_description.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    texture_description.SampleDesc.Count = 1;
    texture_description.SampleDesc.Quality = 0;
    texture_description.Usage = D3D11_USAGE_DEFAULT;
    texture_description.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    texture_description.CPUAccessFlags = 0;
    texture_description.MiscFlags = 0;

    D3D11_SUBRESOURCE_DATA resource_data;
    resource_data.pSysMem = pointer.shape_data.data();
    resource_data.SysMemPitch = pointer.shape_info.Pitch;
    resource_data.SysMemSlicePitch = 0;

    // convert monochrome to masked colors
    using Color = std::array<uint8_t, 4>;
    std::vector<Color> tmpData;
    if (pointer.shape_info.Type == DXGI_OUTDUPL_POINTER_SHAPE_TYPE_MONOCHROME) {
        const auto width = texture_description.Width;
        const auto height = texture_description.Height >> 1;
        const auto pitch = pointer.shape_info.Pitch;
        const auto xor_offset = static_cast<size_t>(height) * pitch;
        tmpData.resize(static_cast<size_t>(width) * height);
        for (auto row = size_t{}; row < height; ++row) {
            for (auto col = 0u; col < width; ++col) {
                const auto mask = 0x80 >> (col & 7);
                const auto addr = (col >> 3) + (row * pitch);
                const auto and_mask = pointer.shape_data[addr] & mask;
                const auto xor_mask = pointer.shape_data[addr + xor_offset] & mask;
                const auto pixel = and_mask ? (xor_mask ? Color{{0xFF, 0xFF, 0xFF, 0xFF}}
                                                        : Color{{0x00, 0x00, 0x00, 0xFF}})
                                            : (xor_mask ? Color{{0xFF, 0xFF, 0xFF, 0x00}}
                                                        : Color{{0x00, 0x00, 0x00, 0x00}});
                tmpData[row * width + col] = pixel;
            }
        }

        texture_description.Height = height;
        resource_data.pSysMem = tmpData.data();
        resource_data.SysMemPitch = width * sizeof(Color);
    }
    //{
    //	auto width = texture_description.Width;
    //	auto height = texture_description.Height;
    //	auto pitch = resource_data.SysMemPitch;
    //	const uint8_t* data = reinterpret_cast<const uint8_t*>(resource_data.pSysMem);
    //	for (auto row = 0u; row < height; ++row) {
    //		for (auto col = 0u; col < width; ++col) {
    //			auto addr = (col * 4) + (row * pitch);
    //			auto Color = reinterpret_cast<const uint32_t&>(data[addr]);
    //			char ch[2] = {'u', '\0'};
    //			switch (Color) {
    //			case 0x00000000: ch[0] = 'b'; break;
    //			case 0x00FFFFFF: ch[0] = 'w'; break;
    //			case 0xFF000000: ch[0] = ' '; break;
    //			case 0xFFFFFFFF: ch[0] = 'x'; break;
    //			default:
    //				if (Color & 0xFF000000) ch[0] = 'n';
    //			}
    //			OutputDebugStringA(ch);
    //		}
    //		OutputDebugStringA("\n");
    //	}
    //}

    auto &dx = *m_dx;
    auto result =
        dx.device()->CreateTexture2D(&texture_description, &resource_data, &dx.pointerTexture);

    D3D11_SHADER_RESOURCE_VIEW_DESC shader_resource_description;
    shader_resource_description.Format = texture_description.Format;
    shader_resource_description.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    shader_resource_description.Texture2D.MostDetailedMip = texture_description.MipLevels - 1;
    shader_resource_description.Texture2D.MipLevels = texture_description.MipLevels;

    result = dx.device()->CreateShaderResourceView(
        dx.pointerTexture.Get(), &shader_resource_description, &dx.pointerTextureShaderResource);
    if (IS_ERROR(result)) throw Error{result, "Failed to create pointer shader resource"};
}

void WindowRenderer::updatePointerVertices(const PointerBuffer &pointer) {
    if (pointer.position_timestamp == m_lastPointerPositionUpdate) return;
    m_lastPointerPositionUpdate = pointer.position_timestamp;

    const auto &dx = *m_dx;

    Vertex vertices[] = {
        Vertex{-1.0f, -1.0f, 0.0f, 1.0f},
        Vertex{-1.0f, 1.0f, 0.0f, 0.0f},
        Vertex{1.0f, -1.0f, 1.0f, 1.0f},
        Vertex{1.0f, -1.0f, 1.0f, 1.0f},
        Vertex{-1.0f, 1.0f, 0.0f, 0.0f},
        Vertex{1.0f, 1.0f, 1.0f, 0.0f}};

    D3D11_TEXTURE2D_DESC texture_description;
    dx.backgroundTexture->GetDesc(&texture_description);

    const auto texture_size = SIZE{
        static_cast<int>(texture_description.Width), static_cast<int>(texture_description.Height)};
    const auto center = POINT{texture_size.cx / 2, texture_size.cy / 2};

    const auto position = pointer.position;

    const auto mouse_to_desktop = [&](Vertex &v, int x, int y) {
        v.x = (position.x + x - center.x) / static_cast<float>(center.x);
        v.y = -1 * (position.y + y - center.y) / static_cast<float>(center.y);
    };

    const auto isMonochrome = pointer.shape_info.Type == DXGI_OUTDUPL_POINTER_SHAPE_TYPE_MONOCHROME;
    const auto size = SIZE{
        static_cast<int>(pointer.shape_info.Width),
        static_cast<int>(isMonochrome ? pointer.shape_info.Height / 2 : pointer.shape_info.Height)};
    mouse_to_desktop(vertices[0], 0, size.cy);
    mouse_to_desktop(vertices[1], 0, 0);
    mouse_to_desktop(vertices[2], size.cx, size.cy);
    mouse_to_desktop(vertices[3], size.cx, size.cy);
    mouse_to_desktop(vertices[4], 0, 0);
    mouse_to_desktop(vertices[5], size.cx, 0);

    dx.deviceContext()->UpdateSubresource(
        dx.pointerVertexBuffer.Get(), 0, nullptr, &vertices[0], 0, 0);
}

WindowRenderer::Resources::Resources(WindowRenderer::InitArgs &&args)
    : BaseRenderer(std::move(args))
    , backgroundTexture(std::move(args.texture)) {

    createBackgroundTextureShaderResource();
    createBackgroundVertexBuffer();
    createSwapChain(args.windowHandle);
    createRenderTarget();
    createMaskedPixelShader();
    createLinearSamplerState();
    createPointerVertexBuffer();
}

void WindowRenderer::Resources::createBackgroundTextureShaderResource() {
    const auto result = device()->CreateShaderResourceView(
        backgroundTexture.Get(), nullptr, &backgroundTextureShaderResource);
    if (IS_ERROR(result)) throw Error{result, "Failed to create background shader resource"};
}

void WindowRenderer::Resources::createBackgroundVertexBuffer() {
    static const auto vertices = std::array{
        Vertex{-1.0f, -1.0f, 0.0f, 1.0f},
        Vertex{-1.0f, 1.0f, 0.0f, 0.0f},
        Vertex{1.0f, -1.0f, 1.0f, 1.0f},
        Vertex{1.0f, -1.0f, 1.0f, 1.0f},
        Vertex{-1.0f, 1.0f, 0.0f, 0.0f},
        Vertex{1.0f, 1.0f, 1.0f, 0.0f},
    };
    D3D11_BUFFER_DESC buffer_description;
    RtlZeroMemory(&buffer_description, sizeof(buffer_description));
    buffer_description.Usage = D3D11_USAGE_DEFAULT;
    buffer_description.ByteWidth = sizeof(vertices);
    buffer_description.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    buffer_description.CPUAccessFlags = 0;

    D3D11_SUBRESOURCE_DATA init_data;
    RtlZeroMemory(&init_data, sizeof(init_data));
    init_data.pSysMem = &vertices[0];

    const auto result =
        device()->CreateBuffer(&buffer_description, &init_data, &backgroundVertexBuffer);
    if (IS_ERROR(result)) throw Error{result, "Failed to create vertex buffer"};
}

void WindowRenderer::Resources::createSwapChain(HWND windowHandle) {
    auto factory = renderer::getFactory(device());
    swapChain = renderer::createSwapChain(factory, device(), windowHandle);
}

void WindowRenderer::Resources::createRenderTarget() {
    ComPtr<ID3D11Texture2D> back_buffer;
    const auto buffer = 0;
    auto result = swapChain->GetBuffer(buffer, __uuidof(ID3D11Texture2D), &back_buffer);
    if (IS_ERROR(result)) throw Error{result, "Failed to get backbuffer"};

    const D3D11_RENDER_TARGET_VIEW_DESC *render_target_description = nullptr;
    result = device()->CreateRenderTargetView(
        back_buffer.Get(), render_target_description, &renderTarget);
    if (IS_ERROR(result)) throw Error{result, "Failed to create render target for backbuffer"};
}

void WindowRenderer::Resources::createMaskedPixelShader() {
    const auto size = ARRAYSIZE(g_MaskedPixelShader);
    auto shader = &g_MaskedPixelShader[0];
    ID3D11ClassLinkage *linkage = nullptr;
    const auto result = device()->CreatePixelShader(shader, size, linkage, &maskedPixelShader);
    if (IS_ERROR(result)) throw Error{result, "Failed to create pixel shader"};
}

void WindowRenderer::Resources::createLinearSamplerState() {
    linearSamplerState = createLinearSampler();
}

void WindowRenderer::Resources::createPointerVertexBuffer() {
    D3D11_BUFFER_DESC buffer_description;
    RtlZeroMemory(&buffer_description, sizeof(buffer_description));
    buffer_description.Usage = D3D11_USAGE_DEFAULT;
    buffer_description.ByteWidth = 6 * sizeof(Vertex);
    buffer_description.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    buffer_description.CPUAccessFlags = 0;

    const auto result = device()->CreateBuffer(&buffer_description, nullptr, &pointerVertexBuffer);
    if (IS_ERROR(result)) throw Error{result, "Failed to create vertex buffer"};
}
