#pragma once
#include "stable.h"

#include "meta/comptr.h"

#include <vector>

inline auto rectSize(const RECT &rect) noexcept -> SIZE {
    return SIZE{rect.right - rect.left, rect.bottom - rect.top};
}

namespace renderer {

struct Error {
    HRESULT result;
    const char *message;
};

struct DeviceData {
    ComPtr<ID3D11Device> device;
    ComPtr<ID3D11DeviceContext> deviceContext;
    D3D_FEATURE_LEVEL selectedFeatureLevel{};
};
auto createDevice() -> DeviceData;

auto getFactory(const ComPtr<ID3D11Device> &device) -> ComPtr<IDXGIFactory2>;

auto createSwapChain(
    const ComPtr<IDXGIFactory2> &factory, const ComPtr<ID3D11Device> &device, HWND window)
    -> ComPtr<IDXGISwapChain2>;

struct DimensionData {
    RECT rect{};
    std::vector<int> used_displays;
};

auto getDimensionData(const ComPtr<ID3D11Device> &device, const std::vector<int> displays)
    -> DimensionData;

auto createTexture(const ComPtr<ID3D11Device> &device, SIZE size) -> ComPtr<ID3D11Texture2D>;
auto createSharedTexture(const ComPtr<ID3D11Device> &device, SIZE size) -> ComPtr<ID3D11Texture2D>;

// note: Do N-O-T close this handle!
auto getSharedHandle(const ComPtr<ID3D11Texture2D> &texture) -> HANDLE;
auto getTextureFromHandle(const ComPtr<ID3D11Device> &device, HANDLE handle)
    -> ComPtr<ID3D11Texture2D>;

auto renderToTexture(const ComPtr<ID3D11Device> &device, const ComPtr<ID3D11Texture2D> &texture)
    -> ComPtr<ID3D11RenderTargetView>;

} // namespace renderer
