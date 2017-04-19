#include "window_renderer.h"
#include "pointer_updater.h"

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

void window_renderer::render() {
	if (pendingResizeBuffers_m) {
		pendingResizeBuffers_m = false;
		renderTarget_m.Reset();
		resizeSwapBuffer();
		makeRenderTarget();
	}

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

	float blendFactor[] = { 0.f, 0.f, 0.f, 0.f };
	uint32_t sampleMask = 0xffffffff;
	deviceContext_m->OMSetBlendState(nullptr, blendFactor, sampleMask);
	deviceContext_m->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	static const Vertex vertices[] = {
		Vertex{ -1.0f, -1.0f, 0.f, 0.0f, 1.0f },
		Vertex{ -1.0f, 1.0f, 0.f, 0.0f, 0.0f },
		Vertex{ 1.0f, -1.0f, 0.f, 1.0f, 1.0f },
		Vertex{ 1.0f, -1.0f, 0.f, 1.0f, 1.0f },
		Vertex{ -1.0f, 1.0f, 0.f, 0.0f, 0.0f },
		Vertex{ 1.0f,  1.0f, 0.f, 1.0f, 0.0f }
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

	ComPtr<ID3D11Buffer> vertex_buffer;
	result = device_m->CreateBuffer(&buffer_description, &init_data, &vertex_buffer);
	if (IS_ERROR(result)) throw error{ result, "Failed to create vertex buffer" };

	uint32_t stride = sizeof(Vertex), offset = 0;
	deviceContext_m->IASetVertexBuffers(0, 1, vertex_buffer.GetAddressOf(), &stride, &offset);

	uint32_t numVertices = (uint32_t)sizeof(vertices) / sizeof(Vertex);
	deviceContext_m->Draw(numVertices, 0);

	deviceContext_m->OMSetRenderTargets(0, nullptr, nullptr);
}

void window_renderer::renderMouse(const pointer_data & pointer) {
	switch (pointer.shape_info.Type) {
	case DXGI_OUTDUPL_POINTER_SHAPE_TYPE_COLOR:
		break;

	case DXGI_OUTDUPL_POINTER_SHAPE_TYPE_MONOCHROME:
		return;

	case DXGI_OUTDUPL_POINTER_SHAPE_TYPE_MASKED_COLOR:		
		return; // TODO

	default:
		return; // invalid
	}

	const auto size = SIZE{ 
		(int)pointer.shape_info.Width, 
		(int)pointer.shape_info.Height 
	};

	ComPtr<ID3D11Buffer> vertex_buffer;
	uint32_t vertice_count;
	{
		Vertex vertices[] = {
			Vertex{ -1.0f, -1.0f, 0.f, 0.0f, 1.0f },
			Vertex{ -1.0f, 1.0f, 0.f, 0.0f, 0.0f },
			Vertex{ 1.0f, -1.0f, 0.f, 1.0f, 1.0f },
			Vertex{ 1.0f, -1.0f, 0.f, 1.0f, 1.0f },
			Vertex{ -1.0f, 1.0f, 0.f, 0.0f, 0.0f },
			Vertex{ 1.0f,  1.0f, 0.f, 1.0f, 0.0f }
		};
		vertice_count = sizeof(vertices) / sizeof(Vertex);

		D3D11_TEXTURE2D_DESC texture_description;
		texture_m->GetDesc(&texture_description);
		auto texture_size = SIZE{ (int)texture_description.Width, (int)texture_description.Height };
		auto center = POINT{ texture_size.cx / 2, texture_size.cy / 2 };

		auto position = pointer.position;

		auto mouse_to_desktop = [&](Vertex& v, int x, int y) {
			v.x = (position.x + x - center.x) / float(center.x);
			v.y = -1 * (position.y + y - center.y) / float(center.y);
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

		auto result = device_m->CreateBuffer(&buffer_description, &resource_data, &vertex_buffer);
	}
	ComPtr<ID3D11Texture2D> texture;
	ComPtr<ID3D11ShaderResourceView> shader_resource;
	{
		//D3D11_TEXTURE2D_DESC texture_description;
		//RtlZeroMemory(&texture_description, sizeof(D3D11_TEXTURE2D_DESC));
		//texture_description.Width = 2;
		//texture_description.Height = 2;
		//texture_description.MipLevels = 1;
		//texture_description.ArraySize = 1;
		//texture_description.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		//texture_description.SampleDesc.Count = 1;
		//texture_description.Usage = D3D11_USAGE_DEFAULT;
		//texture_description.BindFlags = D3D11_BIND_SHADER_RESOURCE;
		////description.MiscFlags = D3D11_RESOURCE_MISC_SHARED;

		//using color4b = uint8_t[4];
		//static const color4b buffer[] = {
		//	{ 255, 0, 0, 0 },
		//	{ 0, 255, 0, 0 },
		//	{ 0, 0, 255, 255 },
		//	{ 128, 128, 128, 255 }
		//};
		//D3D11_SUBRESOURCE_DATA data;
		//data.pSysMem = buffer;
		//data.SysMemPitch = 2 * sizeof(color4b);
		//data.SysMemSlicePitch = sizeof(buffer);

		//auto result = device_m->CreateTexture2D(&texture_description, &data, &texture);
		//if (IS_ERROR(result)) throw error{ result, "Failed to create texture" };

		D3D11_TEXTURE2D_DESC texture_description;
		texture_description.MipLevels = 1;
		texture_description.ArraySize = 1;
		texture_description.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
		texture_description.SampleDesc.Count = 1;
		texture_description.SampleDesc.Quality = 0;
		texture_description.Usage = D3D11_USAGE_DEFAULT;
		texture_description.BindFlags = D3D11_BIND_SHADER_RESOURCE;
		texture_description.CPUAccessFlags = 0;
		texture_description.MiscFlags = 0;
		texture_description.Width = size.cx;
		texture_description.Height = size.cy;

		D3D11_SUBRESOURCE_DATA resource_data;
		resource_data.pSysMem = pointer.shape_data.data();
		resource_data.SysMemPitch = pointer.shape_info.Pitch;
		resource_data.SysMemSlicePitch = 0;

		auto result = device_m->CreateTexture2D(
			&texture_description, 
			&resource_data, 
			&texture);

		D3D11_SHADER_RESOURCE_VIEW_DESC shader_resource_description;
		shader_resource_description.Format = texture_description.Format;
		shader_resource_description.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
		shader_resource_description.Texture2D.MostDetailedMip = texture_description.MipLevels - 1;
		shader_resource_description.Texture2D.MipLevels = texture_description.MipLevels;

		result = device_m->CreateShaderResourceView(
			texture.Get(), 
			&shader_resource_description, 
			&shader_resource);
	}


	uint32_t stride = sizeof(Vertex), offset = 0;
	deviceContext_m->IASetVertexBuffers(0, 1, vertex_buffer.GetAddressOf(), &stride, &offset);
	float blend_factor[] = { 0.f, 0.f, 0.f, 0.f };
	deviceContext_m->OMSetBlendState(blendState_m.Get(), blend_factor, 0xFFFFFFFF);
	deviceContext_m->OMSetRenderTargets(1, renderTarget_m.GetAddressOf(), nullptr);
	deviceContext_m->VSSetShader(vertexShader_m.Get(), nullptr, 0);
	deviceContext_m->PSSetShader(pixelShader_m.Get(), nullptr, 0);
	deviceContext_m->PSSetShaderResources(0, 1, shader_resource.GetAddressOf());
	deviceContext_m->PSSetSamplers(0, 1, samplerState_m.GetAddressOf());

	deviceContext_m->Draw(vertice_count, 0);
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
	view_port.Width = float(texture_description.Width) * zoom_m;
	view_port.Height = float(texture_description.Height) * zoom_m;
	view_port.MinDepth = 0.0f;
	view_port.MaxDepth = 1.0f;
	view_port.TopLeftX = float(offset_m.x) * zoom_m;
	view_port.TopLeftY = float(offset_m.y) * zoom_m;
	deviceContext_m->RSSetViewports(1, &view_port);
}
