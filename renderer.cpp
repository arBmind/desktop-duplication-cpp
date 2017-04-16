#include "renderer.h"

#include "meta/array.h"

#include <limits>

#include "PixelShader.h"
#include "VertexShader.h"

namespace renderer {

	device_data createDevice() {
		device_data dev;

		const auto driver_types = {
			D3D_DRIVER_TYPE_HARDWARE,
			D3D_DRIVER_TYPE_WARP,
			D3D_DRIVER_TYPE_REFERENCE
		};
		const auto feature_levels = make_array(
			D3D_FEATURE_LEVEL_11_0,
			D3D_FEATURE_LEVEL_10_1,
			D3D_FEATURE_LEVEL_10_0,
			D3D_FEATURE_LEVEL_9_1
		);
		HRESULT result;
		for (auto driver_type : driver_types) {
			auto adapter = nullptr;
			auto software = nullptr;
			auto flags = 0; // D3D11_CREATE_DEVICE_DEBUG;
			result = D3D11CreateDevice(adapter, driver_type, software, flags,
				feature_levels.data(), (uint32_t)feature_levels.size(),
				D3D11_SDK_VERSION, &dev.device, &dev.selectedFeatureLevel, &dev.deviceContext);
			if (SUCCEEDED(result)) break;
		}
		if (IS_ERROR(result)) throw error{ result, "Failed to create device" };
		return dev;
	}

	ComPtr<IDXGIFactory2> getFactory(const ComPtr<ID3D11Device>& device) {
		ComPtr<IDXGIDevice> dxgi_device;
		auto result = device.As(&dxgi_device);
		if (IS_ERROR(result)) throw error{ result, "Failed to get IDXGIDevice from device" };

		ComPtr<IDXGIAdapter> dxgi_adapter;
		result = dxgi_device->GetParent(__uuidof(IDXGIAdapter), &dxgi_adapter);
		if (IS_ERROR(result)) throw error{ result, "Failed to get IDXGIAdapter from device" };

		ComPtr<IDXGIFactory2> dxgi_factory;
		result = dxgi_adapter->GetParent(__uuidof(IDXGIFactory2), &dxgi_factory);
		if (IS_ERROR(result)) throw error{ result, "Failed to get IDXGIFactory2 from adapter" };

		return dxgi_factory;
	}

	ComPtr<IDXGISwapChain1> createSwapChain(const ComPtr<IDXGIFactory2>& factory, const ComPtr<ID3D11Device>& device, HWND window) {
		RECT rect;
		GetClientRect(window, &rect);
		auto width = rect.right - rect.left;
		auto height = rect.bottom - rect.top;

		DXGI_SWAP_CHAIN_DESC1 swapChainDesc;
		RtlZeroMemory(&swapChainDesc, sizeof(swapChainDesc));
		swapChainDesc.Width = width;
		swapChainDesc.Height = height;
		swapChainDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
		swapChainDesc.SampleDesc.Count = 1;
		swapChainDesc.SampleDesc.Quality = 0;
		swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
		swapChainDesc.BufferCount = 2;
		swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;

		ComPtr<IDXGISwapChain1> swapChain;
		auto result = factory->CreateSwapChainForHwnd(device.Get(), window, &swapChainDesc, nullptr, nullptr, &swapChain);
		if (IS_ERROR(result)) throw error{ result, "Failed to create window swapchain" };

		result = factory->MakeWindowAssociation(window, DXGI_MWA_NO_ALT_ENTER);
		if (IS_ERROR(result)) throw error{ result, "Failed to associate window" };
		
		return swapChain;
	}

	dimension_data getDesktopData(const ComPtr<ID3D11Device>& device, const std::vector<int> desktops) {
		ComPtr<IDXGIDevice> dxgi_device;
		auto result = device.As(&dxgi_device);
		if (IS_ERROR(result)) throw error{ result, "Failed to get IDXGIDevice from device" };

		ComPtr<IDXGIAdapter> dxgi_adapter;
		result = dxgi_device->GetParent(__uuidof(IDXGIAdapter), &dxgi_adapter);
		if (IS_ERROR(result)) throw error{ result, "Failed to get IDXGIAdapter from device" };

		dimension_data output;
		output.desktop_rect.top = output.desktop_rect.left = std::numeric_limits<int>::max();
		output.desktop_rect.bottom = output.desktop_rect.right = std::numeric_limits<int>::min();

		for (auto desktop : desktops) {
			ComPtr<IDXGIOutput> dxgi_output;
			result = dxgi_adapter->EnumOutputs(desktop, &dxgi_output);
			if (DXGI_ERROR_NOT_FOUND == result) continue;
			if (IS_ERROR(result)) throw error{ result, "Failed to enumerate Output" };

			DXGI_OUTPUT_DESC description;
			dxgi_output->GetDesc(&description);
			output.desktop_rect.top = std::min(output.desktop_rect.top, description.DesktopCoordinates.top);
			output.desktop_rect.left = std::min(output.desktop_rect.left, description.DesktopCoordinates.left);
			output.desktop_rect.bottom = std::max(output.desktop_rect.bottom, description.DesktopCoordinates.bottom);
			output.desktop_rect.right = std::max(output.desktop_rect.right, description.DesktopCoordinates.right);
			output.used_desktops.push_back(desktop);
		}
		if (output.used_desktops.empty()) throw error{ result, "Found no valid Outputs" };

		return output;
	}

	ComPtr<ID3D11Texture2D> createTexture(const ComPtr<ID3D11Device>& device, RECT rect) {
		D3D11_TEXTURE2D_DESC description;
		RtlZeroMemory(&description, sizeof(D3D11_TEXTURE2D_DESC));
		description.Width = rect.right - rect.left;
		description.Height = rect.bottom - rect.top;
		description.MipLevels = 1;
		description.ArraySize = 1;
		description.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
		description.SampleDesc.Count = 1;
		description.Usage = D3D11_USAGE_DEFAULT;
		description.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
		description.CPUAccessFlags = 0;
		//description.MiscFlags = D3D11_RESOURCE_MISC_SHARED;

		ComPtr<ID3D11Texture2D> texture;
		auto result = device->CreateTexture2D(&description, nullptr, &texture);
		if (IS_ERROR(result)) throw error{ result, "Failed to create texture" };

		return texture;
	}

	ComPtr<ID3D11RenderTargetView> renderToTexture(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11Texture2D>& texture) {
		ComPtr<ID3D11RenderTargetView> output;
		D3D11_RENDER_TARGET_VIEW_DESC* render_target_description = nullptr;
		auto result = device->CreateRenderTargetView(texture.Get(), render_target_description, &output);
		if (IS_ERROR(result)) throw error{ result, "Failed to create render target to texture" };
		return output;
	}

	using vector4f = float[4];
	struct vertex { float x, y, z, u, v; };

	device::device(const device_args & args, int width, int height)
		: width_m(width), height_m(height)
		, device_m(args.device)
		, deviceContext_m(args.deviceContext)
		, swapChain_m(args.swapChain) {}

	device device::create(const device_args &args) {
		DXGI_SWAP_CHAIN_DESC swap_desc;
		args.swapChain->GetDesc(&swap_desc);

		RECT rect;
		GetClientRect(swap_desc.OutputWindow, &rect);
		auto width = rect.right - rect.left;
		auto height = rect.bottom - rect.top;

		auto output = device(args, width, height);
		output.makeRenderTarget();
		output.setViewPort();
		output.createSamplerState();
		output.createBlendState();
		output.createVertexShader();
		output.createPixelShader();
		return output;
	}

	void device::reset() {
		swapChain_m.Reset();
		renderTarget_m.Reset();

		samplerState_m.Reset();
		blendState_m.Reset();

		vertexShader_m.Reset();
		inputLayout_m.Reset();
		pixelShader_m.Reset();

		deviceContext_m->ClearState();
		deviceContext_m->Flush();
		deviceContext_m.Reset();
		device_m.Reset();
	}

	void device::resize(int width, int height) {
		if (width == width_m && height == height_m) return;

		width_m = width;
		height_m = height;
		resizeSwapBuffer();
		makeRenderTarget();
		setViewPort();
	}

	void device::setZoom(float zoom) {
		if (zoom_m == zoom) return;
		zoom_m = zoom;
	}

	void device::createSamplerState() {
		D3D11_SAMPLER_DESC descrption;
		RtlZeroMemory(&descrption, sizeof(descrption));
		//descrption.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
		descrption.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
		descrption.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
		descrption.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
		descrption.ComparisonFunc = D3D11_COMPARISON_NEVER;
		descrption.MinLOD = 0;
		descrption.MaxLOD = D3D11_FLOAT32_MAX;
		auto result = device_m->CreateSamplerState(&descrption, &samplerState_m);
		if (IS_ERROR(result)) throw error{ result, "Failed to create sampler state" };

		deviceContext_m->PSSetSamplers(0, 1, samplerState_m.GetAddressOf());
	}

	void device::createBlendState() {
		D3D11_BLEND_DESC description;
		description.AlphaToCoverageEnable = FALSE;
		description.IndependentBlendEnable = FALSE;
		description.RenderTarget[0].BlendEnable = TRUE;
		description.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
		description.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
		description.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
		description.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
		description.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
		description.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
		description.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
		auto result = device_m->CreateBlendState(&description, &blendState_m);
		if (IS_ERROR(result)) throw error{ result, "Failed to create blend state" };
	}

	void device::createVertexShader() {
		auto Size = ARRAYSIZE(g_VS);
		auto result = device_m->CreateVertexShader(g_VS, Size, nullptr, &vertexShader_m);
		if (IS_ERROR(result)) throw error{ result, "Failed to create vertex shader" };

		deviceContext_m->VSSetShader(vertexShader_m.Get(), nullptr, 0);

		static const auto layout = make_array(
			D3D11_INPUT_ELEMENT_DESC{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
			D3D11_INPUT_ELEMENT_DESC{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 }
		);
		result = device_m->CreateInputLayout(layout.data(), (uint32_t)layout.size(), g_VS, Size, &inputLayout_m);
		if (IS_ERROR(result)) throw error{ result, "Failed to create input layout" };

		deviceContext_m->IASetInputLayout(inputLayout_m.Get());
	}

	void device::createPixelShader() {
		auto Size = ARRAYSIZE(g_PS);
		ID3D11ClassLinkage* linkage = nullptr;
		auto result = device_m->CreatePixelShader(g_PS, Size, linkage, &pixelShader_m);
		if (IS_ERROR(result)) throw error{ result, "Failed to create pixel shader" };

		deviceContext_m->PSSetShader(pixelShader_m.Get(), nullptr, 0);
	}

	void device::resizeSwapBuffer() {
		DXGI_SWAP_CHAIN_DESC description;
		swapChain_m->GetDesc(&description);
		auto result = swapChain_m->ResizeBuffers(
			description.BufferCount, 
			width_m, height_m, 
			description.BufferDesc.Format, description.Flags);
		if (IS_ERROR(result)) throw error{ result, "Failed to resize swap buffers" };
	}

	void device::setViewPort() {
		D3D11_VIEWPORT viewPort;
		viewPort.Width = width_m * zoom_m;
		viewPort.Height = height_m * zoom_m;
		viewPort.MinDepth = 0.0f;
		viewPort.MaxDepth = 1.0f;
		viewPort.TopLeftX = 0;
		viewPort.TopLeftY = 0;
		deviceContext_m->RSSetViewports(1, &viewPort);
	}

	void device::makeRenderTarget() {
		ComPtr<ID3D11Texture2D> back_buffer;
		auto buffer = 0;
		auto result = swapChain_m->GetBuffer(buffer, __uuidof(ID3D11Texture2D), &back_buffer);
		if (IS_ERROR(result)) throw error{ result, "Failed to get backbuffer" };

		D3D11_RENDER_TARGET_VIEW_DESC* render_target_description = nullptr;
		result = device_m->CreateRenderTargetView(back_buffer.Get(), render_target_description, &renderTarget_m);
		if (IS_ERROR(result)) throw error{ result, "Failed to create render target for backbuffer" };
	}

	void device::render(ComPtr<ID3D11Texture2D> texture) {
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

		static const vertex vertices[] = {
			vertex{ -1.0f, -1.0f, 0.f, 0.0f, 1.0f },
			vertex{ -1.0f, 1.0f, 0.f, 0.0f, 0.0f },
			vertex{ 1.0f, -1.0f, 0.f, 1.0f, 1.0f },
			vertex{ 1.0f, -1.0f, 0.f, 1.0f, 1.0f },
			vertex{ -1.0f, 1.0f, 0.f, 0.0f, 0.0f },
			vertex{ 1.0f,  1.0f, 0.f, 1.0f, 0.0f }
		};

		//using Color = float[4];
		//auto color = Color{ 0, 1.f,0, 0.5f };
		//deviceContext_m->ClearRenderTargetView(renderTarget_m.Get(), color);

		deviceContext_m->OMSetRenderTargets(1, renderTarget_m.GetAddressOf(), nullptr);

		D3D11_TEXTURE2D_DESC texture_description;
		texture->GetDesc(&texture_description);

		D3D11_SHADER_RESOURCE_VIEW_DESC shader_description;
		shader_description.Format = texture_description.Format;
		shader_description.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
		shader_description.Texture2D.MostDetailedMip = texture_description.MipLevels - 1;
		shader_description.Texture2D.MipLevels = texture_description.MipLevels;

		ComPtr<ID3D11ShaderResourceView> shader_resource;
		auto result = device_m->CreateShaderResourceView(texture.Get(), nullptr, &shader_resource);
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

		uint32_t Stride = sizeof(vertex);
		uint32_t Offset = 0;
		deviceContext_m->IASetVertexBuffers(0, 1, vertex_buffer.GetAddressOf(), &Stride, &Offset);
		uint32_t numVertices = (uint32_t)sizeof(vertices)/sizeof(vertex);
		deviceContext_m->Draw(numVertices, 0);

		deviceContext_m->OMSetRenderTargets(0, nullptr, nullptr);
	}

	void device::swap() {
		auto result = swapChain_m->Present(1, 0);
		if (IS_ERROR(result)) throw error{ result, "Failed to swap buffers" };
	}

}