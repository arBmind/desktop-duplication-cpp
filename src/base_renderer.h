#pragma once
#include "stable.h"

#include "meta/comptr.h"
#include <cinttypes>

// shared between frame updater & window renderer
struct base_renderer {
	struct init_args {
		ComPtr<ID3D11Device> device;
		ComPtr<ID3D11DeviceContext> deviceContext;
	};
	struct vertex { float x, y, u, v; };
	using color = float[4];

	base_renderer(init_args&& args);
	~base_renderer();

	base_renderer() = default;
	base_renderer(const base_renderer&) = default;
	base_renderer(base_renderer&&) = default;
	base_renderer& operator=(const base_renderer&) = default;
	base_renderer& operator=(base_renderer&&) = default;

	ComPtr<ID3D11ShaderResourceView> createShaderTexture(ID3D11Texture2D* texture) const;
	ComPtr<ID3D11SamplerState> createLinearSampler();

	ID3D11Device* device() const { return device_m.Get(); }
	ID3D11DeviceContext* deviceContext() const { return deviceContext_m.Get(); }

	void activateNoRenderTarget() const {
		deviceContext_m->OMSetRenderTargets(0, nullptr, nullptr);
	}

	void activateDiscreteSampler(int index = 0) const { 
		deviceContext_m->PSSetSamplers(index, 1, discreteSamplerState_m.GetAddressOf());
	}
	
	void activateAlphaBlendState() const { 
		float blendFactor[] = { 0.f, 0.f, 0.f, 0.f };
		uint32_t sampleMask = 0xffffffff;
		deviceContext_m->OMSetBlendState(alphaBlendState_m.Get(), blendFactor, sampleMask);
	}
	void activateNoBlendState() const {
		float blendFactor[] = { 0.f, 0.f, 0.f, 0.f };
		uint32_t sampleMask = 0xffffffff;
		deviceContext_m->OMSetBlendState(nullptr, blendFactor, sampleMask);
	}

	void activateVertexShader() const {
		deviceContext_m->VSSetShader(vertexShader_m.Get(), nullptr, 0);
		deviceContext_m->IASetInputLayout(inputLayout_m.Get());
	}
	void activatePlainPixelShader() const {
		deviceContext_m->PSSetShader(plainPixelShader_m.Get(), nullptr, 0);
	}

	void activateTriangleList() const {
		deviceContext_m->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	}

private:
	void createDiscreteSamplerState();
	void createAlphaBlendState();
	void createVertexShader();
	void createPlainPixelShader();

private:
	ComPtr<ID3D11Device> device_m;
	ComPtr<ID3D11DeviceContext> deviceContext_m;

	ComPtr<ID3D11SamplerState> discreteSamplerState_m;
	ComPtr<ID3D11BlendState> alphaBlendState_m;

	ComPtr<ID3D11VertexShader> vertexShader_m;
	ComPtr<ID3D11InputLayout> inputLayout_m;
	ComPtr<ID3D11PixelShader> plainPixelShader_m;
};
