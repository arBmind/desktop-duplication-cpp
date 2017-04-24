#pragma once
#include "stable.h"

#include "base_renderer.h"

#include "meta/optional.h"

struct pointer_buffer;

// manages state how to render background & pointer to output window
struct window_renderer {
	struct init_args : base_renderer::init_args {
		HWND windowHandle; // window to render to
		ComPtr<ID3D11Texture2D> texture; // texture is rendered as quad
	};
	using vertex = base_renderer::vertex;
	struct vec2f { float x, y; };

	void init(init_args&& args);
	void reset();

	float zoom() const { return zoom_m; }
	POINT offset() const { return { (long)offset_m.x, (long)offset_m.y }; }

	bool resize(SIZE size);
	void setZoom(float zoom);
	void moveOffset(POINT delta);
	void moveToBorder(int x, int y);
	void moveTo(vec2f offset);

	void render();
	void renderPointer(const pointer_buffer& pointer);
	void swap();

private:
	void resizeSwapBuffer();
	void setViewPort();
	void updatePointerShape(const pointer_buffer &pointer);
	void updatePointerVertices(const pointer_buffer &pointer);

private:
	float zoom_m = 1.f;
	vec2f offset_m = { 0, 0 };
	SIZE size_m;

	bool pendingResizeBuffers_m = false;
	HWND windowHandle_m;

	uint64_t lastPointerShapeUpdate_m = 0;
	uint64_t lastPointerPositionUpdate_m = 0;

	struct resources : base_renderer {
		resources(window_renderer::init_args&& args);

	private:
		void createBackgroundTextureShaderResource();
		void createBackgroundVertexBuffer();
		void createSwapChain(HWND windowHandle);
		void createMaskedPixelShader();
		void createLinearSamplerState();
		void createPointerVertexBuffer();

	public:
		void createRenderTarget();

		void clearRenderTarget(const color& c) {
			deviceContext()->ClearRenderTargetView(renderTarget_m.Get(), c);
		}
		void activateRenderTarget() {
			deviceContext()->OMSetRenderTargets(1, renderTarget_m.GetAddressOf(), nullptr);
		}

		void activateBackgroundTexture() {
			deviceContext()->PSSetShaderResources(0, 1, backgroundTextureShaderResource_m.GetAddressOf());
		}
		void activateBackgroundVertexBuffer() {
			uint32_t stride = sizeof(vertex), offset = 0;
			deviceContext()->IASetVertexBuffers(0, 1, backgroundVertexBuffer_m.GetAddressOf(), &stride, &offset);
		}

		void activateMaskedPixelShader() const {
			deviceContext()->PSSetShader(maskedPixelShader_m.Get(), nullptr, 0);
		}
		void activatePointerTexture(int index = 0) {
			deviceContext()->PSSetShaderResources(index, 1, pointerTextureShaderResource_m.GetAddressOf());
		}
		void activateLinearSampler(int index = 0) const {
			deviceContext()->PSSetSamplers(index, 1, linearSamplerState_m.GetAddressOf());
		}
		void activatePointerVertexBuffer() {
			uint32_t stride = sizeof(vertex), offset = 0;
			deviceContext()->IASetVertexBuffers(0, 1, pointerVertexBuffer_m.GetAddressOf(), &stride, &offset);
		}

		ComPtr<ID3D11Texture2D> backgroundTexture_m;
		ComPtr<ID3D11ShaderResourceView> backgroundTextureShaderResource_m;
		ComPtr<ID3D11Buffer> backgroundVertexBuffer_m;
		ComPtr<IDXGISwapChain1> swapChain_m;
		ComPtr<ID3D11RenderTargetView> renderTarget_m;
		ComPtr<ID3D11PixelShader> maskedPixelShader_m;
		ComPtr<ID3D11SamplerState> linearSamplerState_m;

		ComPtr<ID3D11Texture2D> pointerTexture_m;
		ComPtr<ID3D11ShaderResourceView> pointerTextureShaderResource_m;
		ComPtr<ID3D11Buffer> pointerVertexBuffer_m;
	};

	meta::optional<resources> dx_m;
};
