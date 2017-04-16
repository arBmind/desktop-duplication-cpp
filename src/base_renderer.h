#pragma once
#include "stable.h"

#include "meta/comptr.h"
#include <array>

// shared between frame updater & window renderer
struct base_renderer {
	struct init_args {
		ComPtr<ID3D11Device> device;
		ComPtr<ID3D11DeviceContext> deviceContext;
	};

	void init(init_args&& args);
	void reset();
	
protected:
	struct Vertex { float x, y, z, u, v; };
	using QuadVertices = std::array<Vertex, 6>;

protected:
	void createSamplerState();
	void createBlendState();
	void createVertexShader();
	void createPixelShader();

protected:
	ComPtr<ID3D11Device> device_m;
	ComPtr<ID3D11DeviceContext> deviceContext_m;

	ComPtr<ID3D11SamplerState> samplerState_m;
	ComPtr<ID3D11BlendState> blendState_m;

	ComPtr<ID3D11VertexShader> vertexShader_m;
	ComPtr<ID3D11InputLayout> inputLayout_m;
	ComPtr<ID3D11PixelShader> pixelShader_m;
};
