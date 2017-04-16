#include "frame_updater.h"

namespace {
	SIZE rectSize(const RECT& rect) {
		return SIZE{ rect.right - rect.left, rect.bottom - rect.top };
	}
	RECT rectMoveTo(const RECT& rect, const POINT& p) {
		auto size = rectSize(rect);
		return RECT{ p.x, p.y,p.x + size.cx, p.y + size.cy };
	}

	RECT rotate(const RECT& rect, DXGI_MODE_ROTATION rotation, const SIZE& size) {
		switch (rotation)
		{
		case DXGI_MODE_ROTATION_UNSPECIFIED:
		case DXGI_MODE_ROTATION_IDENTITY:
			return rect;
		case DXGI_MODE_ROTATION_ROTATE90:
			return {
				size.cy - rect.bottom,
				rect.left,
				size.cy - rect.top,
				rect.right
			};
		case DXGI_MODE_ROTATION_ROTATE180:
			return {
				size.cx - rect.right,
				size.cy - rect.bottom,
				size.cx - rect.left,
				size.cy - rect.top
			};
		case DXGI_MODE_ROTATION_ROTATE270:
			return {
				rect.top,
				size.cx - rect.right,
				rect.bottom,
				size.cx - rect.left
			};
		default:
			return { 0, 0, 0, 0 };
		}
	}
}

void frame_updater::update(const captured_update &data, const frame_data &context) {
	performMoves(data.moved(), context);
	updateDirty(data.desktop_image, data.dirty(), context);
}

void frame_updater::performMoves(const move_view & moves, const frame_data &context) {
	if (moves.empty()) return;

	auto desktopSize = rectSize(context.output_desc.DesktopCoordinates);

	if (!moveTmp_m) {
		D3D11_TEXTURE2D_DESC target_desc;
		target_m->GetDesc(&target_desc);

		D3D11_TEXTURE2D_DESC move_desc;
		move_desc = target_desc;
		move_desc.Width = desktopSize.cx;
		move_desc.Height = desktopSize.cy;
		move_desc.BindFlags = D3D11_BIND_RENDER_TARGET;
		move_desc.MiscFlags = 0;
		auto result = device_m->CreateTexture2D(&move_desc, nullptr, &moveTmp_m);
		if (IS_ERROR(result)) throw RenderFailure(result, "Failed to create move temporary texture");
	}

	auto targetX = context.output_desc.DesktopCoordinates.left - context.offset.x;
	auto targetY = context.output_desc.DesktopCoordinates.top - context.offset.y;
	auto rotation = context.output_desc.Rotation;

	for (const auto& move : moves) {
		auto source = rotate(rectMoveTo(move.DestinationRect, move.SourcePoint), rotation, desktopSize);
		auto dest = rotate(move.DestinationRect, rotation, desktopSize);

		D3D11_BOX box;
		box.left = source.left + targetX;
		box.top = source.top + targetY;
		box.front = 0;
		box.right = source.right + targetX;
		box.bottom = source.bottom + targetY;
		box.back = 1;

		deviceContext_m->CopySubresourceRegion(moveTmp_m.Get(), 0, source.left, source.top, 0, target_m.Get(), 0, &box);

		box.left = source.left;
		box.top = source.top;
		box.right = source.right;
		box.bottom = source.bottom;

		deviceContext_m->CopySubresourceRegion(target_m.Get(), 0, dest.left + targetX, dest.top + targetY, 0, moveTmp_m.Get(), 0, &box);
	}
}

void frame_updater::updateDirty(const ComPtr<ID3D11Texture2D> &desktop, const dirty_view& dirts, const frame_data& context) {
	if (dirts.empty()) return;

	if (!renderTarget_m) {
		auto result = device_m->CreateRenderTargetView(target_m.Get(), nullptr, &renderTarget_m);
		if (IS_ERROR(result)) throw RenderFailure(result, "Failed to create render target from target texture");
	}

	//using Color = float[4];
	//auto color = Color{ 1.f,0,0,0 };
	//deviceContext_m->ClearRenderTargetView(renderTarget_m.Get(), color);
	//return;

	deviceContext_m->OMSetRenderTargets(1, renderTarget_m.GetAddressOf(), nullptr);
	deviceContext_m->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	D3D11_TEXTURE2D_DESC target_desc;
	target_m->GetDesc(&target_desc);

	D3D11_TEXTURE2D_DESC desktop_desc;
	desktop->GetDesc(&desktop_desc);

	D3D11_SHADER_RESOURCE_VIEW_DESC shader_resource_desc;
	shader_resource_desc.Format = desktop_desc.Format;
	shader_resource_desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
	shader_resource_desc.Texture2D.MostDetailedMip = desktop_desc.MipLevels - 1;
	shader_resource_desc.Texture2D.MipLevels = desktop_desc.MipLevels;

	ComPtr<ID3D11ShaderResourceView> shader_resource;
	auto result = device_m->CreateShaderResourceView(desktop.Get(), &shader_resource_desc, &shader_resource);
	if (IS_ERROR(result)) throw RenderFailure(result, "Failed to create desktop texture resource");

	deviceContext_m->PSSetShaderResources(0, 1, shader_resource.GetAddressOf());

	auto targetX = context.output_desc.DesktopCoordinates.left - context.offset.x;
	auto targetY = context.output_desc.DesktopCoordinates.top - context.offset.y;
	auto desktopSize = rectSize(context.output_desc.DesktopCoordinates);
	auto rotation = context.output_desc.Rotation;

	auto centerX = (long)target_desc.Width / 2;
	auto centerY = (long)target_desc.Height / 2;

	auto toVertex = [&](int x, int y) {
		return Vertex{ 
			(x + targetX - centerX) / (float)centerX,
			-1 * (y + targetY - centerY) / (float)centerY, 
			0, 
			x / static_cast<float>(desktop_desc.Width),
			y / static_cast<float>(desktop_desc.Height)
		};
	};

	dirtyQuads_m.reserve(dirts.size());
	dirtyQuads_m.clear();
	for (const auto& dirt : dirts) {
		RECT rotated = rotate(dirt, rotation, desktopSize);
		QuadVertices vertices;

		vertices[0] = toVertex(rotated.left, rotated.bottom);
		vertices[1] = toVertex(rotated.left, rotated.top);
		vertices[2] = toVertex(rotated.right, rotated.bottom);
		vertices[3] = vertices[2];
		vertices[4] = vertices[1];
		vertices[5] = toVertex(rotated.right, rotated.top);

		dirtyQuads_m.push_back(vertices);
	}

	D3D11_BUFFER_DESC buffer_description;
	RtlZeroMemory(&buffer_description, sizeof(buffer_description));
	buffer_description.Usage = D3D11_USAGE_DEFAULT;
	buffer_description.ByteWidth = static_cast<uint32_t>(sizeof(QuadVertices) * dirts.size());
	buffer_description.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	buffer_description.CPUAccessFlags = 0;

	D3D11_SUBRESOURCE_DATA init_data;
	RtlZeroMemory(&init_data, sizeof(init_data));
	init_data.pSysMem = dirtyQuads_m.data();

	ComPtr<ID3D11Buffer> vertex_buffer;
	result = device_m->CreateBuffer(&buffer_description, &init_data, &vertex_buffer);
	if (IS_ERROR(result)) throw RenderFailure(result, "Failed to create dirty vertex buffer");

	uint32_t Stride = sizeof(Vertex);
	uint32_t Offset = 0;
	deviceContext_m->IASetVertexBuffers(0, 1, vertex_buffer.GetAddressOf(), &Stride, &Offset);

	D3D11_VIEWPORT view_port;
	view_port.Width = static_cast<FLOAT>(target_desc.Width);
	view_port.Height = static_cast<FLOAT>(target_desc.Height);
	view_port.MinDepth = 0.0f;
	view_port.MaxDepth = 1.0f;
	view_port.TopLeftX = 0.0f;
	view_port.TopLeftY = 0.0f;
	deviceContext_m->RSSetViewports(1, &view_port);

	deviceContext_m->Draw(static_cast<uint32_t>(QuadVertices().size() * dirts.size()), 0);

	deviceContext_m->OMSetRenderTargets(0, nullptr, nullptr);
}
