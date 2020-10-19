#include "renderer.h"

#include <array>
#include <gsl.h>
#include <limits>

namespace renderer {

auto createDevice() -> DeviceData {
    DeviceData dev;

    constexpr auto driver_types = std::array{
        D3D_DRIVER_TYPE_HARDWARE,
        D3D_DRIVER_TYPE_WARP,
        D3D_DRIVER_TYPE_REFERENCE,
    };
    const auto feature_levels = std::array{
        D3D_FEATURE_LEVEL_11_0,
        D3D_FEATURE_LEVEL_10_1,
        D3D_FEATURE_LEVEL_10_0,
        D3D_FEATURE_LEVEL_9_1,
    };
    auto result = HRESULT{};
    for (auto driver_type : driver_types) {
        const auto adapter = nullptr;
        const auto software = nullptr;
        const auto flags = 0; // D3D11_CREATE_DEVICE_DEBUG;
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
    ComPtr<IDXGIDevice>
        dxgi_device;
    auto result = device.As(&dxgi_device);
    if (IS_ERROR(result)) throw Error{result, "Failed to get IDXGIDevice from device"};

    ComPtr<IDXGIAdapter> dxgi_adapter;
    result = dxgi_device->GetParent(__uuidof(IDXGIAdapter), &dxgi_adapter);
    if (IS_ERROR(result)) throw Error{result, "Failed to get IDXGIAdapter from device"};

    ComPtr<IDXGIFactory2> dxgi_factory;
    result = dxgi_adapter->GetParent(__uuidof(IDXGIFactory2), &dxgi_factory);
    if (IS_ERROR(result)) throw Error{result, "Failed to get IDXGIFactory2 from adapter"};

    return dxgi_factory;
}

auto createSwapChain(
    const ComPtr<IDXGIFactory2> &factory, const ComPtr<ID3D11Device> &device, HWND window)
    -> ComPtr<IDXGISwapChain2> {
    [[gsl::suppress("26415"), gsl::suppress("26418")]] // ComPtr is not just a smart pointer
    RECT rect;
    GetClientRect(window, &rect);
    const auto width = static_cast<uint32_t>(rect.right - rect.left);
    const auto height = static_cast<uint32_t>(rect.bottom - rect.top);

    DXGI_SWAP_CHAIN_DESC1 swap_chain_desription;
    RtlZeroMemory(&swap_chain_desription, sizeof(swap_chain_desription));
    swap_chain_desription.Width = width;
    swap_chain_desription.Height = height;
    swap_chain_desription.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    swap_chain_desription.SampleDesc.Count = 1;
    swap_chain_desription.SampleDesc.Quality = 0;
    swap_chain_desription.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swap_chain_desription.BufferCount = 2;
    swap_chain_desription.Scaling = DXGI_SCALING_NONE;
    swap_chain_desription.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
    swap_chain_desription.Flags = DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT;

    ComPtr<IDXGISwapChain1> swap_chain;
    auto result = factory->CreateSwapChainForHwnd(
        device.Get(), window, &swap_chain_desription, nullptr, nullptr, &swap_chain);
    if (IS_ERROR(result)) throw Error{result, "Failed to create window swapchain"};

    result = factory->MakeWindowAssociation(window, DXGI_MWA_NO_ALT_ENTER);
    if (IS_ERROR(result)) throw Error{result, "Failed to associate window"};

    ComPtr<IDXGISwapChain2> swap_chain2;
    result = swap_chain.As(&swap_chain2);
    if (IS_ERROR(result)) throw Error{result, "Failed to get swapchain2"};

    return swap_chain2;
}

auto getDimensionData(const ComPtr<ID3D11Device> &device, const std::vector<int> displays)
    -> DimensionData {
    [[gsl::suppress("26415"), gsl::suppress("26418")]] // ComPtr is not just a smart pointer
    ComPtr<IDXGIDevice>
        dxgi_device;
    auto result = device.As(&dxgi_device);
    if (IS_ERROR(result)) throw Error{result, "Failed to get IDXGIDevice from device"};

    ComPtr<IDXGIAdapter> dxgi_adapter;
    result = dxgi_device->GetParent(__uuidof(IDXGIAdapter), &dxgi_adapter);
    if (IS_ERROR(result)) throw Error{result, "Failed to get IDXGIAdapter from device"};

    DimensionData output;
    output.rect.top = output.rect.left = std::numeric_limits<int>::max();
    output.rect.bottom = output.rect.right = std::numeric_limits<int>::min();

    for (auto display : displays) {
        ComPtr<IDXGIOutput> dxgi_output;
        result = dxgi_adapter->EnumOutputs(display, &dxgi_output);
        if (DXGI_ERROR_NOT_FOUND == result) continue;
        if (IS_ERROR(result)) throw Error{result, "Failed to enumerate Output"};

        DXGI_OUTPUT_DESC description;
        dxgi_output->GetDesc(&description);
        output.rect.top = std::min(output.rect.top, description.DesktopCoordinates.top);
        output.rect.left = std::min(output.rect.left, description.DesktopCoordinates.left);
        output.rect.bottom = std::max(output.rect.bottom, description.DesktopCoordinates.bottom);
        output.rect.right = std::max(output.rect.right, description.DesktopCoordinates.right);
        output.used_displays.push_back(display);
    }
    if (output.used_displays.empty()) throw Error{result, "Found no valid displays"};

    return output;
}

auto createTexture(const ComPtr<ID3D11Device> &device, SIZE size) -> ComPtr<ID3D11Texture2D> {
    [[gsl::suppress("26415"), gsl::suppress("26418")]] // ComPtr is not just a smart pointer
    D3D11_TEXTURE2D_DESC description;
    RtlZeroMemory(&description, sizeof(D3D11_TEXTURE2D_DESC));
    description.Width = size.cx;
    description.Height = size.cy;
    description.MipLevels = 1;
    description.ArraySize = 1;
    description.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    description.SampleDesc.Count = 1;
    description.Usage = D3D11_USAGE_DEFAULT;
    description.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
    description.CPUAccessFlags = 0;
    // description.MiscFlags = D3D11_RESOURCE_MISC_SHARED;

    ComPtr<ID3D11Texture2D> texture;
    const auto result = device->CreateTexture2D(&description, nullptr, &texture);
    if (IS_ERROR(result)) throw Error{result, "Failed to create texture"};

    return texture;
}

auto createSharedTexture(const ComPtr<ID3D11Device> &device, SIZE size) -> ComPtr<ID3D11Texture2D> {
    [[gsl::suppress("26415"), gsl::suppress("26418")]] // ComPtr is not just a smart pointer
    D3D11_TEXTURE2D_DESC description;
    RtlZeroMemory(&description, sizeof(D3D11_TEXTURE2D_DESC));
    description.Width = size.cx;
    description.Height = size.cy;
    description.MipLevels = 1;
    description.ArraySize = 1;
    description.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    description.SampleDesc.Count = 1;
    description.Usage = D3D11_USAGE_DEFAULT;
    description.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
    description.CPUAccessFlags = 0;
    description.MiscFlags = D3D11_RESOURCE_MISC_SHARED /*| D3D11_RESOURCE_MISC_GENERATE_MIPS*/;

    ComPtr<ID3D11Texture2D> texture;
    const auto result = device->CreateTexture2D(&description, nullptr, &texture);
    if (IS_ERROR(result)) throw Error{result, "Failed to create shared texture"};

    return texture;
}

auto getSharedHandle(const ComPtr<ID3D11Texture2D> &texture) -> HANDLE {
    [[gsl::suppress("26415"), gsl::suppress("26418")]] // ComPtr is not just a smart pointer
    ComPtr<IDXGIResource>
        DXGIResource;
    auto result = texture.As(&DXGIResource);
    if (IS_ERROR(result)) throw Error{result, "Failed to convert shared texture"};
    HANDLE handle = nullptr;
    result = DXGIResource->GetSharedHandle(&handle);
    if (IS_ERROR(result)) throw Error{result, "Failed to retrieve handle"};
    return handle;
}

auto getTextureFromHandle(const ComPtr<ID3D11Device> &device, HANDLE handle)
    -> ComPtr<ID3D11Texture2D> {
    [[gsl::suppress("26415"), gsl::suppress("26418")]] // ComPtr is not just a smart pointer
    ComPtr<ID3D11Texture2D>
        output;
    const auto result = device->OpenSharedResource(handle, __uuidof(ID3D11Texture2D), &output);
    if (IS_ERROR(result)) throw Error{result, "Failed to fetch texture from handle"};
    return output;
}

auto renderToTexture(const ComPtr<ID3D11Device> &device, const ComPtr<ID3D11Texture2D> &texture)
    -> ComPtr<ID3D11RenderTargetView> {
    [[gsl::suppress("26415"), gsl::suppress("26418")]] // ComPtr is not just a smart pointer
    ComPtr<ID3D11RenderTargetView>
        output;
    const D3D11_RENDER_TARGET_VIEW_DESC *render_target_description = nullptr;
    const auto result =
        device->CreateRenderTargetView(texture.Get(), render_target_description, &output);
    if (IS_ERROR(result)) throw Error{result, "Failed to create render target to texture"};
    return output;
}

} // namespace renderer
