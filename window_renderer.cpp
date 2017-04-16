#include "window_renderer.h"

#include "renderer.h"

#include <memory>

using error = renderer::error;

void window_renderer::init(init_args&& args) {
	base_renderer::init(std::move(args));
	windowHandle_m = args.windowHandle;
	texture_m = std::move(args.texture);

	createSwapChain();
	makeRenderTarget();

	RECT rect;
	GetClientRect(windowHandle_m, &rect);
	size_m = rectSize(rect);
}

void window_renderer::reset() {
	swapChain_m.Reset();
	renderTarget_m.Reset();

	base_renderer::reset();
}

void window_renderer::resize(SIZE size) {
	if (size.cx == size_m.cx && size.cy == size_m.cy) return;
	size_m = size;
	resetBuffers_m = true;
}

void window_renderer::setZoom(float zoom) {
	if (zoom_m == zoom) return;
	zoom_m = zoom;
}

void window_renderer::render() {
	if (resetBuffers_m) {
		resetBuffers_m = false;
		renderTarget_m.Reset();
		resizeSwapBuffer();
		makeRenderTarget();
	}
	//{
	//	D3D11_TEXTURE2D_DESC texture_description;
	//	texture->GetDesc(&texture_description);

	//	D3D11_TEXTURE2D_DESC description;
	//	RtlZeroMemory(&description, sizeof(D3D11_TEXTURE2D_DESC));
	//	description.Width = 2;
	//	description.Height = 2;
	//	description.MipLevels = 1;
	//	description.ArraySize = 1;
	//	description.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	//	description.SampleDesc.Count = 1;
	//	description.Usage = D3D11_USAGE_DEFAULT;
	//	description.BindFlags = D3D11_BIND_SHADER_RESOURCE;
	//	//description.MiscFlags = D3D11_RESOURCE_MISC_SHARED;

	//	using color4b = uint8_t[4];
	//	static const color4b buffer[] = {
	//		{ 255, 0, 0, 0 },
	//		{ 0, 255, 0, 0 },
	//		{ 0, 0, 255, 255 },
	//		{ 128, 128, 128, 255 }
	//	};
	//	D3D11_SUBRESOURCE_DATA data;
	//	data.pSysMem = buffer;
	//	data.SysMemPitch = 2 * sizeof(color4b);
	//	data.SysMemSlicePitch = sizeof(buffer);

	//	auto result = device_m->CreateTexture2D(&description, &data, &texture);
	//	if (IS_ERROR(result)) throw error{ result, "Failed to create texture" };
	//}

	static const Vertex vertices[] = {
		Vertex{ -1.0f, -1.0f, 0.f, 0.0f, 1.0f },
		Vertex{ -1.0f, 1.0f, 0.f, 0.0f, 0.0f },
		Vertex{ 1.0f, -1.0f, 0.f, 1.0f, 1.0f },
		Vertex{ 1.0f, -1.0f, 0.f, 1.0f, 1.0f },
		Vertex{ -1.0f, 1.0f, 0.f, 0.0f, 0.0f },
		Vertex{ 1.0f,  1.0f, 0.f, 1.0f, 0.0f }
	};

	using Color = float[4];
	auto color = Color{ 0, 0, 0, 0 };
	deviceContext_m->ClearRenderTargetView(renderTarget_m.Get(), color);

	setViewPort();
	deviceContext_m->OMSetRenderTargets(1, renderTarget_m.GetAddressOf(), nullptr);

	D3D11_TEXTURE2D_DESC texture_description;
	texture_m->GetDesc(&texture_description);

	D3D11_SHADER_RESOURCE_VIEW_DESC shader_description;
	shader_description.Format = texture_description.Format;
	shader_description.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
	shader_description.Texture2D.MostDetailedMip = texture_description.MipLevels - 1;
	shader_description.Texture2D.MipLevels = texture_description.MipLevels;

	ComPtr<ID3D11ShaderResourceView> shader_resource;
	auto result = device_m->CreateShaderResourceView(texture_m.Get(), nullptr, &shader_resource);
	if (IS_ERROR(result)) throw error{ result, "Failed to create shader resource" };

	deviceContext_m->PSSetShaderResources(0, 1, shader_resource.GetAddressOf());

	//vector4f blendFactor = { 0.f, 0.f, 0.f, 0.f };
	//uint32_t sampleMask = 0xffffffff;
	//deviceContext_m->OMSetBlendState(nullptr, blendFactor, sampleMask);
	deviceContext_m->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	D3D11_BUFFER_DESC buffer_description;
	RtlZeroMemory(&buffer_description, sizeof(buffer_description));
	buffer_description.Usage = D3D11_USAGE_DEFAULT;
	buffer_description.ByteWidth = sizeof(vertices);
	buffer_description.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	buffer_description.CPUAccessFlags = 0;

	D3D11_SUBRESOURCE_DATA init_data;
	RtlZeroMemory(&init_data, sizeof(init_data));
	init_data.pSysMem = vertices;

	ComPtr<ID3D11Buffer> vertex_buffer;
	result = device_m->CreateBuffer(&buffer_description, &init_data, &vertex_buffer);
	if (IS_ERROR(result)) throw error{ result, "Failed to create vertex buffer" };

	uint32_t stride = sizeof(Vertex);
	uint32_t offset = 0;
	deviceContext_m->IASetVertexBuffers(0, 1, vertex_buffer.GetAddressOf(), &stride, &offset);
	uint32_t numVertices = (uint32_t)sizeof(vertices) / sizeof(Vertex);
	deviceContext_m->Draw(numVertices, 0);

	deviceContext_m->OMSetRenderTargets(0, nullptr, nullptr);
}

void window_renderer::swap() {
	auto result = swapChain_m->Present(1, 0);
	if (IS_ERROR(result)) throw error{ result, "Failed to swap buffers" };
}

void window_renderer::createSwapChain() {
	auto factory = renderer::getFactory(device_m);
	swapChain_m = renderer::createSwapChain(factory, device_m, windowHandle_m);
}

void window_renderer::makeRenderTarget() {
	ComPtr<ID3D11Texture2D> back_buffer;
	auto buffer = 0;
	auto result = swapChain_m->GetBuffer(buffer, __uuidof(ID3D11Texture2D), &back_buffer);
	if (IS_ERROR(result)) throw error{ result, "Failed to get backbuffer" };

	D3D11_RENDER_TARGET_VIEW_DESC* render_target_description = nullptr;
	result = device_m->CreateRenderTargetView(back_buffer.Get(), render_target_description, &renderTarget_m);
	if (IS_ERROR(result)) throw error{ result, "Failed to create render target for backbuffer" };
}

void window_renderer::resizeSwapBuffer() {
	DXGI_SWAP_CHAIN_DESC description;
	swapChain_m->GetDesc(&description);
	auto result = swapChain_m->ResizeBuffers(
		description.BufferCount,
		size_m.cx, size_m.cy,
		description.BufferDesc.Format, description.Flags);
	if (IS_ERROR(result)) throw error{ result, "Failed to resize swap buffers" };
}

void window_renderer::setViewPort() {
	D3D11_TEXTURE2D_DESC texture_description;
	texture_m->GetDesc(&texture_description);

	D3D11_VIEWPORT view_port;
	view_port.Width = (float)texture_description.Width * zoom_m;
	view_port.Height = (float)texture_description.Height * zoom_m;
	view_port.MinDepth = 0.0f;
	view_port.MaxDepth = 1.0f;
	view_port.TopLeftX = 0;
	view_port.TopLeftY = 0;
	deviceContext_m->RSSetViewports(1, &view_port);
}
