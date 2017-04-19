#pragma once
#include "base_renderer.h"

struct pointer_data;

struct window_renderer : base_renderer {
	struct init_args : base_renderer::init_args {
		HWND windowHandle;
		ComPtr<ID3D11Texture2D> texture; // texture is rendered as quad
	};

	void init(init_args&& args);
	void reset();

	float zoom() const { return zoom_m; }
	POINT offset() const { return { (long)offset_m.x, (long)offset_m.y }; }

	bool resize(SIZE size);
	void setZoom(float zoom);
	void moveOffset(POINT delta);

	void render();
	void renderMouse(const pointer_data& pointer);
	void swap();

private:
	void createSwapChain();
	void makeRenderTarget();

	void resizeSwapBuffer();
	void setViewPort();

private:
	struct vec2f { float x, y; };

	float zoom_m = 1.f;
	vec2f offset_m = { 0, 0 };
	SIZE size_m;

	bool pendingResizeBuffers_m = false;
	HWND windowHandle_m;

	ComPtr<ID3D11Texture2D> texture_m;
	ComPtr<IDXGISwapChain1> swapChain_m;
	ComPtr<ID3D11RenderTargetView> renderTarget_m;
};
