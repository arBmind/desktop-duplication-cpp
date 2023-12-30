#include "WindowRenderer.h"
#include "PointerUpdater.h"

#include "renderer.h"

#include "MaskedPixelShader.h"

#include <array>

using Error = renderer::Error;
using win32::Rect;

WindowRenderer::WindowRenderer(const Args &config)
    : m_args{config} {}

void WindowRenderer::init(InitArgs &&args) {
    m_dx.emplace(std::move(args));
    m_size = args.windowDimension;
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

void WindowRenderer::zoomOutput(float zoom) noexcept { m_args.outputZoom = zoom; }

void WindowRenderer::updateOffset(Vec2f offset) noexcept { m_args.captureOffset = offset; }

void WindowRenderer::render() {
    try {
        renderBlack();
        renderFrame();
        renderPointer();
        swap();
    }
    catch (...) {
        m_args.setErrorCallback(m_args.callbackPtr, std::current_exception());
    }
}

void WindowRenderer::renderBlack() {
    auto &dx = *m_dx;
    if (m_pendingResizeBuffers) {
        m_pendingResizeBuffers = false;
        dx.renderTarget.Reset();
        resizeSwapBuffer();
        dx.createRenderTarget();
    }

    auto black = BaseRenderer::Color{0, 0, 0, 0};
    dx.clearRenderTarget(black);
}

void WindowRenderer::renderFrame() {
    auto &dx = *m_dx;
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

void WindowRenderer::renderPointer() {
    auto &pointer = m_args.pointerBuffer;
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
        auto const index = 1;
        // dx.activateNoBlendState(); -- already set
        dx.activatePointerTexture(index);
        dx.activateDiscreteSampler(index);
        dx.activateMaskedPixelShader();
    }

    dx.deviceContext()->Draw(6, 0);
}

void WindowRenderer::swap() {
    auto const &dx = *m_dx;
    dx.activateNoRenderTarget();

    auto const result = dx.swapChain->Present(1, 0);
    if (IS_ERROR(result)) throw Error{result, "Failed to swap buffers"};
}

void WindowRenderer::resizeSwapBuffer() {
    auto const &dx = *m_dx;
    auto description = DXGI_SWAP_CHAIN_DESC{};
    dx.swapChain->GetDesc(&description);
    auto const result = dx.swapChain->ResizeBuffers(
        description.BufferCount, m_size.width, m_size.height, description.BufferDesc.Format, description.Flags);
    if (IS_ERROR(result)) throw Error{result, "Failed to resize swap buffers"};
}

void WindowRenderer::setViewPort() {
    auto const &dx = *m_dx;
    auto texture_description = D3D11_TEXTURE2D_DESC{};
    dx.backgroundTexture->GetDesc(&texture_description);

    auto zoom = m_args.outputZoom;
    auto offset = m_args.captureOffset;
    auto view_port = D3D11_VIEWPORT{
        .TopLeftX = static_cast<float>(offset.x) * zoom,
        .TopLeftY = static_cast<float>(offset.y) * zoom,
        .Width = static_cast<float>(texture_description.Width) * zoom,
        .Height = static_cast<float>(texture_description.Height) * zoom,
        .MinDepth = 0.0f,
        .MaxDepth = 1.0f,
    };
    dx.deviceContext()->RSSetViewports(1, &view_port);
}

void WindowRenderer::updatePointerShape(const PointerBuffer &pointer) {
    if (pointer.shape_timestamp == m_lastPointerShapeUpdate) return;
    m_lastPointerShapeUpdate = pointer.shape_timestamp;

    auto texture_description = D3D11_TEXTURE2D_DESC{
        .Width = pointer.shape_info.Width,
        .Height = pointer.shape_info.Height,
        .MipLevels = 1,
        .ArraySize = 1,
        .Format = DXGI_FORMAT_B8G8R8A8_UNORM,
        .SampleDesc =
            DXGI_SAMPLE_DESC{
                .Count = 1,
                .Quality = 0,
            },
        .Usage = D3D11_USAGE_DEFAULT,
        .BindFlags = D3D11_BIND_SHADER_RESOURCE,
        .CPUAccessFlags = 0,
        .MiscFlags = 0,
    };
    auto resource_data = D3D11_SUBRESOURCE_DATA{
        .pSysMem = pointer.shape_data.data(),
        .SysMemPitch = pointer.shape_info.Pitch,
        .SysMemSlicePitch = 0,
    };

    // convert monochrome to masked colors
    using Color = std::array<uint8_t, 4>;
    auto tmpData = std::vector<Color>{};
    if (pointer.shape_info.Type == DXGI_OUTDUPL_POINTER_SHAPE_TYPE_MONOCHROME) {
        auto const width = texture_description.Width;
        auto const height = texture_description.Height >> 1;
        auto const pitch = pointer.shape_info.Pitch;
        auto const xor_offset = static_cast<size_t>(height) * pitch;
        tmpData.resize(static_cast<size_t>(width) * height);
        for (auto row = size_t{}; row < height; ++row) {
            for (auto col = 0u; col < width; ++col) {
                auto const mask = 0x80 >> (col & 7);
                auto const addr = (col >> 3) + (row * pitch);
                auto const and_mask = pointer.shape_data[addr] & mask;
                auto const xor_mask = pointer.shape_data[addr + xor_offset] & mask;
                auto const pixel = and_mask
                                       ? (xor_mask ? Color{{0xFF, 0xFF, 0xFF, 0xFF}} : Color{{0x00, 0x00, 0x00, 0xFF}})
                                       : (xor_mask ? Color{{0xFF, 0xFF, 0xFF, 0x00}} : Color{{0x00, 0x00, 0x00, 0x00}});
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
    //	const uint8_t* data = std::bit_cast<const uint8_t*>(resource_data.pSysMem);
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
    auto result = dx.device()->CreateTexture2D(&texture_description, &resource_data, &dx.pointerTexture);
    if (IS_ERROR(result)) throw Error{result, "Failed to create texture 2d"};

    auto shader_resource_description = D3D11_SHADER_RESOURCE_VIEW_DESC{
        .Format = texture_description.Format,
        .ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D,
        .Texture2D =
            D3D11_TEX2D_SRV{
                .MostDetailedMip = texture_description.MipLevels - 1,
                .MipLevels = texture_description.MipLevels,
            },
    };
    result = dx.device()->CreateShaderResourceView(
        dx.pointerTexture.Get(), &shader_resource_description, &dx.pointerTextureShaderResource);
    if (IS_ERROR(result)) throw Error{result, "Failed to create pointer shader resource"};
}

void WindowRenderer::updatePointerVertices(const PointerBuffer &pointer) {
    if (pointer.position_timestamp == m_lastPointerPositionUpdate) return;
    m_lastPointerPositionUpdate = pointer.position_timestamp;

    auto const &dx = *m_dx;

    auto vertices = std::array{
        Vertex{-1.0f, -1.0f, 0.0f, 1.0f},
        Vertex{-1.0f, 1.0f, 0.0f, 0.0f},
        Vertex{1.0f, -1.0f, 1.0f, 1.0f},
        Vertex{1.0f, -1.0f, 1.0f, 1.0f},
        Vertex{-1.0f, 1.0f, 0.0f, 0.0f},
        Vertex{1.0f, 1.0f, 1.0f, 0.0f},
    };
    auto texture_description = D3D11_TEXTURE2D_DESC{};
    dx.backgroundTexture->GetDesc(&texture_description);

    auto const texture_size =
        SIZE{static_cast<int>(texture_description.Width), static_cast<int>(texture_description.Height)};
    auto const center = POINT{texture_size.cx / 2, texture_size.cy / 2};

    auto const position = pointer.position;

    auto const mouse_to_desktop = [&](Vertex &v, int x, int y) {
        v.x = (position.x + x - center.x) / static_cast<float>(center.x);
        v.y = -1 * (position.y + y - center.y) / static_cast<float>(center.y);
    };

    auto const isMonochrome = pointer.shape_info.Type == DXGI_OUTDUPL_POINTER_SHAPE_TYPE_MONOCHROME;
    auto const size = SIZE{
        static_cast<int>(pointer.shape_info.Width),
        static_cast<int>(isMonochrome ? pointer.shape_info.Height / 2 : pointer.shape_info.Height),
    };
    mouse_to_desktop(vertices[0], 0, size.cy);
    mouse_to_desktop(vertices[1], 0, 0);
    mouse_to_desktop(vertices[2], size.cx, size.cy);
    mouse_to_desktop(vertices[3], size.cx, size.cy);
    mouse_to_desktop(vertices[4], 0, 0);
    mouse_to_desktop(vertices[5], size.cx, 0);

    dx.deviceContext()->UpdateSubresource(dx.pointerVertexBuffer.Get(), 0, nullptr, &vertices[0], 0, 0);
}

WindowRenderer::Resources::Resources(WindowRenderer::InitArgs &&args)
    : BaseRenderer(std::move(args.basic))
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
    auto const result =
        device()->CreateShaderResourceView(backgroundTexture.Get(), nullptr, &backgroundTextureShaderResource);
    if (IS_ERROR(result)) throw Error{result, "Failed to create background shader resource"};
}

void WindowRenderer::Resources::createBackgroundVertexBuffer() {
    static auto const vertices = std::array{
        Vertex{-1.0f, -1.0f, 0.0f, 1.0f},
        Vertex{-1.0f, 1.0f, 0.0f, 0.0f},
        Vertex{1.0f, -1.0f, 1.0f, 1.0f},
        Vertex{1.0f, -1.0f, 1.0f, 1.0f},
        Vertex{-1.0f, 1.0f, 0.0f, 0.0f},
        Vertex{1.0f, 1.0f, 1.0f, 0.0f},
    };
    auto const buffer_description = D3D11_BUFFER_DESC{
        .ByteWidth = sizeof(vertices),
        .Usage = D3D11_USAGE_DEFAULT,
        .BindFlags = D3D11_BIND_VERTEX_BUFFER,
        .CPUAccessFlags = 0,
        .MiscFlags = {},
        .StructureByteStride = {},
    };
    auto const init_data = D3D11_SUBRESOURCE_DATA{
        .pSysMem = vertices.data(),
        .SysMemPitch = {},
        .SysMemSlicePitch = {},
    };
    auto const result = device()->CreateBuffer(&buffer_description, &init_data, &backgroundVertexBuffer);
    if (IS_ERROR(result)) throw Error{result, "Failed to create vertex buffer"};
}

void WindowRenderer::Resources::createSwapChain(HWND windowHandle) {
    auto factory = renderer::getFactory(device());
    swapChain = renderer::createSwapChain(factory, device(), windowHandle);
}

void WindowRenderer::Resources::createRenderTarget() {
    auto back_buffer = ComPtr<ID3D11Texture2D>{};
    auto const buffer = 0;
    auto result = swapChain->GetBuffer(buffer, __uuidof(ID3D11Texture2D), &back_buffer);
    if (IS_ERROR(result)) throw Error{result, "Failed to get backbuffer"};

    static constexpr const D3D11_RENDER_TARGET_VIEW_DESC *render_target_description = nullptr;
    result = device()->CreateRenderTargetView(back_buffer.Get(), nullptr, &renderTarget);
    if (IS_ERROR(result)) throw Error{result, "Failed to create render target for backbuffer"};
}

void WindowRenderer::Resources::createMaskedPixelShader() {
    auto const size = ARRAYSIZE(g_MaskedPixelShader);
    auto shader = &g_MaskedPixelShader[0];
    ID3D11ClassLinkage *linkage = nullptr;
    auto const result = device()->CreatePixelShader(shader, size, linkage, &maskedPixelShader);
    if (IS_ERROR(result)) throw Error{result, "Failed to create pixel shader"};
}

void WindowRenderer::Resources::createLinearSamplerState() { linearSamplerState = createLinearSampler(); }

void WindowRenderer::Resources::createPointerVertexBuffer() {
    auto buffer_description = D3D11_BUFFER_DESC{
        .ByteWidth = 6 * sizeof(Vertex),
        .Usage = D3D11_USAGE_DEFAULT,
        .BindFlags = D3D11_BIND_VERTEX_BUFFER,
        .CPUAccessFlags = 0,
        .MiscFlags = {},
        .StructureByteStride = {},
    };
    auto const result = device()->CreateBuffer(&buffer_description, nullptr, &pointerVertexBuffer);
    if (IS_ERROR(result)) throw Error{result, "Failed to create vertex buffer"};
}
