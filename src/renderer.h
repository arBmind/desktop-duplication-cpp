#pragma once
#include "stable.h"

#include "meta/comptr.h"

#include <vector>

inline auto rectSize(const RECT &rect) noexcept -> SIZE {
    return SIZE{rect.right - rect.left, rect.bottom - rect.top};
}

namespace renderer {

struct error {
    HRESULT result;
    const char *message;
};

struct device_data {
    ComPtr<ID3D11Device> device;
    ComPtr<ID3D11DeviceContext> deviceContext;
    D3D_FEATURE_LEVEL selectedFeatureLevel{};
};
device_data createDevice();

ComPtr<IDXGIFactory2> getFactory(const ComPtr<ID3D11Device> &device);

ComPtr<IDXGISwapChain1> createSwapChain(
    const ComPtr<IDXGIFactory2> &factory, const ComPtr<ID3D11Device> &device, HWND window);

struct dimension_data {
    RECT rect{};
    std::vector<int> used_displays;
};

dimension_data
getDimensionData(const ComPtr<ID3D11Device> &device, const std::vector<int> displays);

ComPtr<ID3D11Texture2D> createTexture(const ComPtr<ID3D11Device> &device, SIZE size);
ComPtr<ID3D11Texture2D> createSharedTexture(const ComPtr<ID3D11Device> &device, SIZE size);

// note: Do N-O-T close this handle!
HANDLE getSharedHandle(const ComPtr<ID3D11Texture2D> &texture);
ComPtr<ID3D11Texture2D> getTextureFromHandle(const ComPtr<ID3D11Device> &device, HANDLE handle);

ComPtr<ID3D11RenderTargetView>
renderToTexture(const ComPtr<ID3D11Device> &device, const ComPtr<ID3D11Texture2D> &texture);

} // namespace renderer
