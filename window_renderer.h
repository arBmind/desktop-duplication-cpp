#pragma once
#include "base_renderer.h"

struct window_renderer : base_renderer {
	struct init_args : base_renderer::init_args {
		HWND windowHandle;
		ComPtr<ID3D11Texture2D> texture; // texture is rendered as quad
	};

	void init(init_args&& args);
	void reset();

	float zoom() const { return zoom_m; }

	void resize(SIZE size);
	void setZoom(float zoom);

	void render();
	void swap();

private:
	void createSwapChain();
	void makeRenderTarget();

	void resizeSwapBuffer();
	void setViewPort();

private:
	float zoom_m = 1.f;
	SIZE size_m;

	bool resetBuffers_m = false;
	HWND windowHandle_m;

	ComPtr<ID3D11Texture2D> texture_m;
	ComPtr<IDXGISwapChain1> swapChain_m;
	ComPtr<ID3D11RenderTargetView> renderTarget_m;
};
