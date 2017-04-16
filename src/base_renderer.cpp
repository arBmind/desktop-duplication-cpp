#include "base_renderer.h"

#include "renderer.h"

#include "meta/array.h"

#include "PixelShader.h"
#include "VertexShader.h"

using error = renderer::error;

void base_renderer::init(init_args&& args) {
	device_m = std::move(args.device);
	deviceContext_m = std::move(args.deviceContext);

	createSamplerState();
	createBlendState();
	createVertexShader();
	createPixelShader();
}

void base_renderer::reset() {
	samplerState_m.Reset();
	blendState_m.Reset();

	vertexShader_m.Reset();
	inputLayout_m.Reset();
	pixelShader_m.Reset();

	// ensure device is clean
	deviceContext_m->ClearState();
	deviceContext_m->Flush();

	deviceContext_m.Reset();
	device_m.Reset();
}

void base_renderer::createSamplerState() {
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

void base_renderer::createBlendState() {
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

void base_renderer::createVertexShader() {
	auto size = ARRAYSIZE(g_VertexShader);
	auto result = device_m->CreateVertexShader(g_VertexShader, size, nullptr, &vertexShader_m);
	if (IS_ERROR(result)) throw error{ result, "Failed to create vertex shader" };

	deviceContext_m->VSSetShader(vertexShader_m.Get(), nullptr, 0);

	static const auto layout = make_array(
		D3D11_INPUT_ELEMENT_DESC{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		D3D11_INPUT_ELEMENT_DESC{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 }
	);
	result = device_m->CreateInputLayout(layout.data(), (uint32_t)layout.size(), g_VertexShader, size, &inputLayout_m);
	if (IS_ERROR(result)) throw error{ result, "Failed to create input layout" };

	deviceContext_m->IASetInputLayout(inputLayout_m.Get());
}

void base_renderer::createPixelShader() {
	auto size = ARRAYSIZE(g_PixelShader);
	ID3D11ClassLinkage* linkage = nullptr;
	auto result = device_m->CreatePixelShader(g_PixelShader, size, linkage, &pixelShader_m);
	if (IS_ERROR(result)) throw error{ result, "Failed to create pixel shader" };

	deviceContext_m->PSSetShader(pixelShader_m.Get(), nullptr, 0);
}
