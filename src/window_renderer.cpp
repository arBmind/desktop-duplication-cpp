#include "window_renderer.h"
#include "pointer_updater.h"

#include "renderer.h"

#include "../MaskedPixelShader.h"

#include <memory>
#include <array>

using error = renderer::error;

void window_renderer::init(init_args&& args) {
	dx_m.emplace(std::move(args));
	windowHandle_m = args.windowHandle;

	RECT rect;
	GetClientRect(windowHandle_m, &rect);
	size_m = rectSize(rect);
}

void window_renderer::reset() {
	dx_m.reset();
}

bool window_renderer::resize(SIZE size) {
	if (size.cx == size_m.cx && size.cy == size_m.cy) return false;
	size_m = size;
	pendingResizeBuffers_m = true;
	return true;
}

void window_renderer::setZoom(float zoom) {
	if (zoom_m == zoom) return;
	zoom_m = zoom;
}

void window_renderer::moveOffset(POINT delta) {
	if (delta.x == 0 && delta.y == 0) return;
	auto offset = vec2f{
		offset_m.x + (float)delta.x / zoom_m,
		offset_m.y + (float)delta.y / zoom_m
	};
	offset_m = offset;
}

void window_renderer::moveToBorder(int x, int y) {
	auto next = offset_m;

	auto& dx = *dx_m;
	D3D11_TEXTURE2D_DESC texture_description;
	dx.backgroundTexture_m->GetDesc(&texture_description);

	if (0 > x) next.x = 0;
	else if (0 < x) next.x = -float(texture_description.Width) + (size_m.cx / zoom_m);

	if (0 > y) next.y = 0;
	else if (0 < y) next.y = -float(texture_description.Height) + (size_m.cy / zoom_m);

	moveTo(next);
}

void window_renderer::moveTo(vec2f offset) { 
	if (offset.x == offset_m.x && offset.y == offset_m.y) return;
	offset_m = offset;
}

void window_renderer::render() {
	auto& dx = *dx_m;
	if (pendingResizeBuffers_m) {
		pendingResizeBuffers_m = false;
		dx.renderTarget_m.Reset();
		resizeSwapBuffer();
		dx.createRenderTarget();
	}

	dx.clearRenderTarget({ 0,0,0,0 });

	dx.deviceContext()->GenerateMips(dx.backgroundTextureShaderResource_m.Get());

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

void window_renderer::renderPointer(const pointer_buffer& pointer) {
	if (pointer.position_timestamp == 0) return;
	if (!pointer.visible) return;

	auto& dx = *dx_m;
	if (pointer.shape_timestamp != lastPointerShapeUpdate_m) {
		lastPointerShapeUpdate_m = pointer.shape_timestamp;

		updatePointerShape(pointer);
	}

	activatePointerVertices(pointer);

	if (pointer.shape_info.Type == DXGI_OUTDUPL_POINTER_SHAPE_TYPE_COLOR) {
		dx.activateAlphaBlendState();
		dx.activatePlainPixelShader();
		dx.activatePointerTexture();
	}
	else {
		auto index = 1;
		// dx.activateNoBlendState(); -- already set
		dx.activatePointerTexture(index);
		dx.activateDiscreteSampler(index);
		dx.activateMaskedPixelShader();
	}

	dx.deviceContext()->Draw(6, 0);
}

void window_renderer::swap() {
	auto& dx = *dx_m;
	dx.activateNoRenderTarget();

	auto result = dx.swapChain_m->Present(1, 0);
	if (IS_ERROR(result)) throw error{ result, "Failed to swap buffers" };
}


void window_renderer::resizeSwapBuffer() {
	auto& dx = *dx_m;
	DXGI_SWAP_CHAIN_DESC description;
	dx.swapChain_m->GetDesc(&description);
	auto result = dx.swapChain_m->ResizeBuffers(
		description.BufferCount,
		size_m.cx, size_m.cy,
		description.BufferDesc.Format, description.Flags);
	if (IS_ERROR(result)) throw error{ result, "Failed to resize swap buffers" };
}

void window_renderer::setViewPort() {
	auto& dx = *dx_m;
	D3D11_TEXTURE2D_DESC texture_description;
	dx.backgroundTexture_m->GetDesc(&texture_description);

	D3D11_VIEWPORT view_port;
	view_port.Width = float(texture_description.Width) * zoom_m;
	view_port.Height = float(texture_description.Height) * zoom_m;
	view_port.MinDepth = 0.0f;
	view_port.MaxDepth = 1.0f;
	view_port.TopLeftX = float(offset_m.x) * zoom_m;
	view_port.TopLeftY = float(offset_m.y) * zoom_m;

	dx.deviceContext()->RSSetViewports(1, &view_port);
}

void window_renderer::updatePointerShape(const pointer_buffer & pointer) {
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
	using color = std::array<uint8_t, 4>;
	std::vector<color> tmpData;
	if (pointer.shape_info.Type == DXGI_OUTDUPL_POINTER_SHAPE_TYPE_MONOCHROME) {
		auto width = texture_description.Width;
		auto height = texture_description.Height >> 1;
		auto pitch = pointer.shape_info.Pitch;
		tmpData.resize(width * height);
		for (auto row = 0u; row < height; ++row) {
			for (auto col = 0u; col < width; ++col) {
				auto mask = 0x80 >> (col & 7);
				auto addr = (col >> 3) + (row * pitch);
				auto and_mask = pointer.shape_data[addr] & mask;
				auto xor_mask = pointer.shape_data[addr + height*pitch] & mask;
				auto pixel = and_mask ?
					(xor_mask ? color{ { 0xFF, 0xFF, 0xFF, 0xFF } } : color{ { 0x00, 0x00, 0x00, 0xFF } }) :
					(xor_mask ? color{ { 0xFF, 0xFF, 0xFF, 0x00 } } : color{ { 0xFF, 0xFF, 0xFF, 0x00 } });
				tmpData[row*width + col] = pixel;
			}
		}

		texture_description.Height = height;
		resource_data.pSysMem = tmpData.data();
		resource_data.SysMemPitch = width * sizeof(color);
	}

	auto& dx = *dx_m;
	auto result = dx.device()->CreateTexture2D(
		&texture_description,
		&resource_data,
		&dx.pointerTexture_m);

	D3D11_SHADER_RESOURCE_VIEW_DESC shader_resource_description;
	shader_resource_description.Format = texture_description.Format;
	shader_resource_description.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
	shader_resource_description.Texture2D.MostDetailedMip = texture_description.MipLevels - 1;
	shader_resource_description.Texture2D.MipLevels = texture_description.MipLevels;

	result = dx.device()->CreateShaderResourceView(
		dx.pointerTexture_m.Get(),
		&shader_resource_description,
		&dx.pointerTextureShaderResource_m);
}

void window_renderer::activatePointerVertices(const pointer_buffer & pointer) {
	auto& dx = *dx_m;

	vertex vertices[] = {
		vertex{ -1.0f, -1.0f, 0.0f, 1.0f },
		vertex{ -1.0f, 1.0f, 0.0f, 0.0f },
		vertex{ 1.0f, -1.0f, 1.0f, 1.0f },
		vertex{ 1.0f, -1.0f, 1.0f, 1.0f },
		vertex{ -1.0f, 1.0f, 0.0f, 0.0f },
		vertex{ 1.0f,  1.0f, 1.0f, 0.0f }
	};

	D3D11_TEXTURE2D_DESC texture_description;
	dx.backgroundTexture_m->GetDesc(&texture_description);

	auto texture_size = SIZE{ (int)texture_description.Width, (int)texture_description.Height };
	auto center = POINT{ texture_size.cx / 2, texture_size.cy / 2 };

	auto position = pointer.position;

	auto mouse_to_desktop = [&](vertex& v, int x, int y) {
		v.x = (position.x + x - center.x) / float(center.x);
		v.y = -1 * (position.y + y - center.y) / float(center.y);
	};

	auto isMonochrome = pointer.shape_info.Type == DXGI_OUTDUPL_POINTER_SHAPE_TYPE_MONOCHROME;
	const auto size = SIZE{
		(int)pointer.shape_info.Width,
		(int)(isMonochrome ? pointer.shape_info.Height / 2 : pointer.shape_info.Height)
	};
	mouse_to_desktop(vertices[0], 0, size.cy);
	mouse_to_desktop(vertices[1], 0, 0);
	mouse_to_desktop(vertices[2], size.cx, size.cy);
	mouse_to_desktop(vertices[3], size.cx, size.cy);
	mouse_to_desktop(vertices[4], 0, 0);
	mouse_to_desktop(vertices[5], size.cx, 0);

	D3D11_BUFFER_DESC buffer_description;
	ZeroMemory(&buffer_description, sizeof(D3D11_BUFFER_DESC));
	buffer_description.Usage = D3D11_USAGE_DEFAULT;
	buffer_description.ByteWidth = sizeof(vertices);
	buffer_description.BindFlags = D3D11_BIND_VERTEX_BUFFER;

	D3D11_SUBRESOURCE_DATA resource_data;
	ZeroMemory(&resource_data, sizeof(D3D11_SUBRESOURCE_DATA));
	resource_data.pSysMem = vertices;

	ComPtr<ID3D11Buffer> vertex_buffer;
	auto result = dx.device()->CreateBuffer(&buffer_description, &resource_data, &vertex_buffer);

	uint32_t stride = sizeof(vertex), offset = 0;
	dx.deviceContext()->IASetVertexBuffers(0, 1, vertex_buffer.GetAddressOf(), &stride, &offset);
}

window_renderer::resources::resources(window_renderer::init_args&& args)
	: base_renderer(std::move(args))
	, backgroundTexture_m(std::move(args.texture)) {

	createBackgroundTextureShaderResource();
	createBackgroundVertexBuffer();
	createSwapChain(args.windowHandle);
	createRenderTarget();
	createMaskedPixelShader();
	createLinearSamplerState();
}

void window_renderer::resources::createBackgroundTextureShaderResource() {
	D3D11_TEXTURE2D_DESC texture_description;
	backgroundTexture_m->GetDesc(&texture_description);

	D3D11_SHADER_RESOURCE_VIEW_DESC shader_description;
	shader_description.Format = texture_description.Format;
	shader_description.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
	shader_description.Texture2D.MostDetailedMip = texture_description.MipLevels - 1;
	shader_description.Texture2D.MipLevels = texture_description.MipLevels;

	auto result = device()->CreateShaderResourceView(backgroundTexture_m.Get(), nullptr, &backgroundTextureShaderResource_m);
	if (IS_ERROR(result)) throw error{ result, "Failed to create shader resource" };
}

void window_renderer::resources::createBackgroundVertexBuffer() {
	static const vertex vertices[] = {
		vertex{ -1.0f, -1.0f, 0.0f, 1.0f },
		vertex{ -1.0f, 1.0f, 0.0f, 0.0f },
		vertex{ 1.0f, -1.0f, 1.0f, 1.0f },
		vertex{ 1.0f, -1.0f, 1.0f, 1.0f },
		vertex{ -1.0f, 1.0f, 0.0f, 0.0f },
		vertex{ 1.0f,  1.0f, 1.0f, 0.0f }
	};

	D3D11_BUFFER_DESC buffer_description;
	RtlZeroMemory(&buffer_description, sizeof(buffer_description));
	buffer_description.Usage = D3D11_USAGE_DEFAULT;
	buffer_description.ByteWidth = sizeof(vertices);
	buffer_description.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	buffer_description.CPUAccessFlags = 0;

	D3D11_SUBRESOURCE_DATA init_data;
	RtlZeroMemory(&init_data, sizeof(init_data));
	init_data.pSysMem = vertices;

	auto result = device()->CreateBuffer(&buffer_description, &init_data, &backgroundVertexBuffer_m);
	if (IS_ERROR(result)) throw error{ result, "Failed to create vertex buffer" };
}

void window_renderer::resources::createSwapChain(HWND windowHandle) {
	auto factory = renderer::getFactory(device());
	swapChain_m = renderer::createSwapChain(factory, device(), windowHandle);
}

void window_renderer::resources::createRenderTarget() {
	ComPtr<ID3D11Texture2D> back_buffer;
	auto buffer = 0;
	auto result = swapChain_m->GetBuffer(buffer, __uuidof(ID3D11Texture2D), &back_buffer);
	if (IS_ERROR(result)) throw error{ result, "Failed to get backbuffer" };

	D3D11_RENDER_TARGET_VIEW_DESC* render_target_description = nullptr;
	result = device()->CreateRenderTargetView(back_buffer.Get(), render_target_description, &renderTarget_m);
	if (IS_ERROR(result)) throw error{ result, "Failed to create render target for backbuffer" };
}

void window_renderer::resources::createMaskedPixelShader() {
	auto size = ARRAYSIZE(g_MaskedPixelShader);
	ID3D11ClassLinkage* linkage = nullptr;
	auto result = device()->CreatePixelShader(g_MaskedPixelShader, size, linkage, &maskedPixelShader_m);
	if (IS_ERROR(result)) throw error{ result, "Failed to create pixel shader" };
}

void window_renderer::resources::createLinearSamplerState() {
	linearSamplerState_m = createLinearSampler();
}
