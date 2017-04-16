#pragma once
#include "stable.h"

#include "meta/comptr.h"
#include "meta/result.h"

#include <vector>
#include <optional>

namespace renderer {
	struct error {
		HRESULT result;
		const char* message;
	};

	struct device_data {
		ComPtr<ID3D11Device> device;
		ComPtr<ID3D11DeviceContext> deviceContext;
		D3D_FEATURE_LEVEL selectedFeatureLevel;
	};
	device_data createDevice();

	ComPtr<IDXGIFactory2> getFactory(const ComPtr<ID3D11Device>& device);

	ComPtr<IDXGISwapChain1> createSwapChain(const ComPtr<IDXGIFactory2>& factory, const ComPtr<ID3D11Device>& device, HWND window);

	struct dimension_data {
		RECT desktop_rect;
		std::vector<int> used_desktops;
	};

	dimension_data getDesktopData(const ComPtr<ID3D11Device>& device, const std::vector<int> desktops);

	ComPtr<ID3D11Texture2D> createTexture(const ComPtr<ID3D11Device>& device, RECT dimensions);
	ComPtr<ID3D11RenderTargetView> renderToTexture(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11Texture2D>& texture);


	struct device_args {
		ComPtr<ID3D11Device> device;
		ComPtr<ID3D11DeviceContext> deviceContext;
		ComPtr<IDXGISwapChain1> swapChain;
	};

	struct device {
		static device create(const device_args& args);
		device() = default;
		device(const device&) = default;
		device(device&&) = default;
		device& operator =(const device&) = default;
		device& operator =(device&&) = default;

		void reset();

		float zoom() const { return zoom_m; }

		void resize(int width, int height);
		void setZoom(float zoom);

		void render(ComPtr<ID3D11Texture2D> texture);
		void swap();

	private:
		device(const device_args & args, int width, int height);

		void makeRenderTarget();
		void createSamplerState();
		void createBlendState();
		void createVertexShader();
		void createPixelShader();

		void resizeSwapBuffer();
		void setViewPort();

	private:
		int width_m, height_m;
		float zoom_m = 1.f;

		ComPtr<ID3D11Device> device_m;
		ComPtr<ID3D11DeviceContext> deviceContext_m;
		ComPtr<IDXGISwapChain1> swapChain_m;
		ComPtr<ID3D11RenderTargetView> renderTarget_m;

		ComPtr<ID3D11SamplerState> samplerState_m;
		ComPtr<ID3D11BlendState> blendState_m;

		ComPtr<ID3D11VertexShader> vertexShader_m;
		ComPtr<ID3D11InputLayout> inputLayout_m;
		ComPtr<ID3D11PixelShader> pixelShader_m;
	};
}
