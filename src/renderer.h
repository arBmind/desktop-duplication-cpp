#pragma once
#include "meta/comptr.h"
#include "win32/Geometry.h"

#include <d3d11.h>
#include <dxgi1_3.h>

#include <vector>

namespace renderer {

using win32::Dimension;
using win32::Rect;

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
    Rect rect{};
    std::vector<int> used_displays;
};

auto getDimensionData(const ComPtr<ID3D11Device> &device, const std::vector<int> displays)
    -> DimensionData;

auto createTexture(const ComPtr<ID3D11Device> &device, Dimension) -> ComPtr<ID3D11Texture2D>;
auto createSharedTexture(const ComPtr<ID3D11Device> &device, Dimension) -> ComPtr<ID3D11Texture2D>;

// note: Do N-O-T close this handle!
auto getSharedHandle(const ComPtr<ID3D11Texture2D> &texture) -> HANDLE;
auto getTextureFromHandle(const ComPtr<ID3D11Device> &device, HANDLE handle)
    -> ComPtr<ID3D11Texture2D>;

auto renderToTexture(const ComPtr<ID3D11Device> &device, const ComPtr<ID3D11Texture2D> &texture)
    -> ComPtr<ID3D11RenderTargetView>;

} // namespace renderer
