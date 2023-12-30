#include "renderer.h"

#include <array>
#include <limits>

namespace renderer {

auto createDevice() -> DeviceData {
    auto dev = DeviceData{};

    constexpr auto driver_types = std::array{
        D3D_DRIVER_TYPE_HARDWARE,
        D3D_DRIVER_TYPE_WARP,
        D3D_DRIVER_TYPE_REFERENCE,
    };
    auto const feature_levels = std::array{
        D3D_FEATURE_LEVEL_11_0,
        D3D_FEATURE_LEVEL_10_1,
        D3D_FEATURE_LEVEL_10_0,
        D3D_FEATURE_LEVEL_9_1,
    };
    auto result = HRESULT{};
    for (auto driver_type : driver_types) {
        static constexpr auto const adapter = nullptr;
        static constexpr auto const software = nullptr;
        auto const flags = 0; // D3D11_CREATE_DEVICE_DEBUG;
        result = D3D11CreateDevice(
            adapter,
            driver_type,
            software,
            flags,
            feature_levels.data(),
            static_cast<uint32_t>(feature_levels.size()),
            D3D11_SDK_VERSION,
            &dev.device,
            &dev.selectedFeatureLevel,
            &dev.deviceContext);
        if (SUCCEEDED(result)) break;
    }
    if (IS_ERROR(result)) throw Error{result, "Failed to create device"};
    return dev;
}

auto getFactory(const ComPtr<ID3D11Device> &device) -> ComPtr<IDXGIFactory2> {
    [[gsl::suppress("26415"), gsl::suppress("26418")]] // ComPtr is not just a smart pointer
    auto dxgi_device = ComPtr<IDXGIDevice>{};
    {
        auto const result = device.As(&dxgi_device);
        if (IS_ERROR(result)) throw Error{result, "Failed to get IDXGIDevice from device"};
    }
    auto dxgi_adapter = ComPtr<IDXGIAdapter>{};
    {
        auto const result = dxgi_device->GetParent(__uuidof(IDXGIAdapter), &dxgi_adapter);
        if (IS_ERROR(result)) throw Error{result, "Failed to get IDXGIAdapter from device"};
    }
    auto dxgi_factory = ComPtr<IDXGIFactory2>{};
    {
        auto const result = dxgi_adapter->GetParent(__uuidof(IDXGIFactory2), &dxgi_factory);
        if (IS_ERROR(result)) throw Error{result, "Failed to get IDXGIFactory2 from adapter"};
    }
    return dxgi_factory;
}

auto createSwapChain(const ComPtr<IDXGIFactory2> &factory, const ComPtr<ID3D11Device> &device, HWND window)
    -> ComPtr<IDXGISwapChain2> {
    [[gsl::suppress("26415"), gsl::suppress("26418")]] // ComPtr is not just a smart pointer
    auto rect = RECT{};
    ::GetClientRect(window, &rect);
    auto const width = static_cast<uint32_t>(rect.right - rect.left);
    auto const height = static_cast<uint32_t>(rect.bottom - rect.top);

    auto swap_chain_desription = DXGI_SWAP_CHAIN_DESC1{
        .Width = width,
        .Height = height,
        .Format = DXGI_FORMAT_B8G8R8A8_UNORM,
        .Stereo = {},
        .SampleDesc = DXGI_SAMPLE_DESC{.Count = 1, .Quality = 0},
        .BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT,
        .BufferCount = 2,
        .Scaling = DXGI_SCALING_NONE,
        .SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL,
        .AlphaMode = {},
        .Flags = DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT,
    };
    auto swap_chain = ComPtr<IDXGISwapChain1>{};
    {
        auto const result = factory->CreateSwapChainForHwnd(
            device.Get(), window, &swap_chain_desription, nullptr, nullptr, &swap_chain);
        if (IS_ERROR(result)) throw Error{result, "Failed to create window swapchain"};
    }
    {
        auto const result = factory->MakeWindowAssociation(window, DXGI_MWA_NO_ALT_ENTER);
        if (IS_ERROR(result)) throw Error{result, "Failed to associate window"};
    }
    auto swap_chain2 = ComPtr<IDXGISwapChain2>{};
    {
        auto const result = swap_chain.As(&swap_chain2);
        if (IS_ERROR(result)) throw Error{result, "Failed to get swapchain2"};
    }
    return swap_chain2;
}

auto getDimensionData(const ComPtr<ID3D11Device> &device, const std::vector<int> &displays) -> DimensionData {
    [[gsl::suppress("26415"), gsl::suppress("26418")]] // ComPtr is not just a smart pointer
    auto dxgi_device = ComPtr<IDXGIDevice>{};
    {
        auto const result = device.As(&dxgi_device);
        if (IS_ERROR(result)) throw Error{result, "Failed to get IDXGIDevice from device"};
    }
    auto dxgi_adapter = ComPtr<IDXGIAdapter>{};
    {
        auto const result = dxgi_device->GetParent(__uuidof(IDXGIAdapter), &dxgi_adapter);
        if (IS_ERROR(result)) throw Error{result, "Failed to get IDXGIAdapter from device"};
    }
    auto rect = RECT{};
    rect.top = rect.left = std::numeric_limits<LONG>::max();
    rect.bottom = rect.right = std::numeric_limits<LONG>::min();

    auto output = DimensionData{};
    for (auto display : displays) {
        auto dxgi_output = ComPtr<IDXGIOutput>{};
        {
            auto const result = dxgi_adapter->EnumOutputs(display, &dxgi_output);
            if (DXGI_ERROR_NOT_FOUND == result) continue;
            if (IS_ERROR(result)) throw Error{result, "Failed to enumerate Output"};
        }
        auto description = DXGI_OUTPUT_DESC{};
        dxgi_output->GetDesc(&description);

        rect.top = std::min(rect.top, description.DesktopCoordinates.top);
        rect.left = std::min(rect.left, description.DesktopCoordinates.left);
        rect.bottom = std::max(rect.bottom, description.DesktopCoordinates.bottom);
        rect.right = std::max(rect.right, description.DesktopCoordinates.right);
        output.used_displays.push_back(display);
    }
    if (output.used_displays.empty()) throw Error{HRESULT{}, "Found no valid displays"};

    output.rect = Rect::fromRECT(rect);
    return output;
}

auto createTexture(const ComPtr<ID3D11Device> &device, Dimension dimension) -> ComPtr<ID3D11Texture2D> {
    [[gsl::suppress("26415"), gsl::suppress("26418")]] // ComPtr is not just a smart pointer
    auto const description = D3D11_TEXTURE2D_DESC{
        .Width = static_cast<UINT>(dimension.width),
        .Height = static_cast<UINT>(dimension.height),
        .MipLevels = 1,
        .ArraySize = 1,
        .Format = DXGI_FORMAT_B8G8R8A8_UNORM,
        .SampleDesc = DXGI_SAMPLE_DESC{.Count = 1, .Quality = {}},
        .Usage = D3D11_USAGE_DEFAULT,
        .BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE,
        .CPUAccessFlags = 0,
        .MiscFlags = {}, // D3D11_RESOURCE_MISC_SHARED
    };
    auto texture = ComPtr<ID3D11Texture2D>{};
    {
        auto const result = device->CreateTexture2D(&description, nullptr, &texture);
        if (IS_ERROR(result)) throw Error{result, "Failed to create texture"};
    }
    return texture;
}

auto createSharedTexture(const ComPtr<ID3D11Device> &device, Dimension dimension) -> ComPtr<ID3D11Texture2D> {
    [[gsl::suppress("26415"), gsl::suppress("26418")]] // ComPtr is not just a smart pointer
    auto const description = D3D11_TEXTURE2D_DESC{
        .Width = static_cast<UINT>(dimension.width),
        .Height = static_cast<UINT>(dimension.height),
        .MipLevels = 1,
        .ArraySize = 1,
        .Format = DXGI_FORMAT_B8G8R8A8_UNORM,
        .SampleDesc = DXGI_SAMPLE_DESC{.Count = 1, .Quality = {}},
        .Usage = D3D11_USAGE_DEFAULT,
        .BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE,
        .CPUAccessFlags = 0,
        .MiscFlags = D3D11_RESOURCE_MISC_SHARED, // | D3D11_RESOURCE_MISC_GENERATE_MIPS
    };
    auto texture = ComPtr<ID3D11Texture2D>{};
    {
        auto const result = device->CreateTexture2D(&description, nullptr, &texture);
        if (IS_ERROR(result)) throw Error{result, "Failed to create shared texture"};
    }
    return texture;
}

auto getSharedHandle(const ComPtr<ID3D11Texture2D> &texture) -> HANDLE {
    [[gsl::suppress("26415"), gsl::suppress("26418")]] // ComPtr is not just a smart pointer
    auto DXGIResource = ComPtr<IDXGIResource>{};
    {
        auto const result = texture.As(&DXGIResource);
        if (IS_ERROR(result)) throw Error{result, "Failed to convert shared texture"};
    }
    auto handle = HANDLE{};
    {
        auto const result = DXGIResource->GetSharedHandle(&handle);
        if (IS_ERROR(result)) throw Error{result, "Failed to retrieve handle"};
    }
    return handle;
}

auto getTextureFromHandle(const ComPtr<ID3D11Device> &device, HANDLE handle) -> ComPtr<ID3D11Texture2D> {
    [[gsl::suppress("26415"), gsl::suppress("26418")]] // ComPtr is not just a smart pointer
    auto output = ComPtr<ID3D11Texture2D>{};
    {
        auto const result = device->OpenSharedResource(handle, __uuidof(ID3D11Texture2D), &output);
        if (IS_ERROR(result)) throw Error{result, "Failed to fetch texture from handle"};
    }
    return output;
}

auto renderToTexture(const ComPtr<ID3D11Device> &device, const ComPtr<ID3D11Texture2D> &texture)
    -> ComPtr<ID3D11RenderTargetView> {
    [[gsl::suppress("26415"), gsl::suppress("26418")]] // ComPtr is not just a smart pointer
    auto output = ComPtr<ID3D11RenderTargetView>{};
    static constexpr const auto render_target_description = nullptr;
    {
        auto const result = device->CreateRenderTargetView(texture.Get(), render_target_description, &output);
        if (IS_ERROR(result)) throw Error{result, "Failed to create render target to texture"};
    }
    return output;
}

} // namespace renderer
