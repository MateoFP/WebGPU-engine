#pragma once
#define STB_IMAGE_IMPLEMENTATION 
#define CGLTF_IMPLEMENTATION

#include <iostream>
#include <memory>
#include <string_view>
#include <vector>
#include <webgpu/webgpu.h>
#include <GLFW/glfw3.h>
#ifdef WEB
#include <emscripten.h>
#include <webgpu/webgpu_cpp.h>
#else
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>
#include <windows.h>
#include <webgpu/wgpu.h>
#endif
#include "include/stb_image.h"
#include "include/mateo_math.h"
#include "include/cgltf.h"
#include "include/ufbx.h"
#include "include/font.h"
#include "include/imgui/imgui.h"

struct MyWindow
{
    int x;
    int y;
    GLFWwindow* handle;
};

struct EngineContext
{
    WGPUInstance instance = nullptr;
    WGPUDevice device = nullptr;
    WGPUQueue queue = nullptr;
    WGPUAdapter adapter = nullptr;
    WGPUSurface surface = nullptr;
    bool adapter_request = false;
    bool device_request = false;
    WGPUTextureFormat surface_format;

    WGPURenderPipeline plane_pipeline;
    WGPURenderPipeline object_pipeline;
    WGPURenderPipeline ui_pipeline;
    WGPURenderPipeline wire_pipeline;
    WGPURenderPipeline outline_pipeline;

    WGPUPipelineLayout              global_pipeline_layout;
    WGPUBindGroup                   global_bg;

    WGPUTexture     depth_texture;
    WGPUTextureView depth_view;
    WGPUTexture     texture_array;
    WGPUTextureView texture_array_view;
    WGPUSampler     linear_sampler;
};
EngineContext vk = {};
MyWindow win = {};

void UpdateGame()
{

}
void InitWorld()
{
}

void ConfigSurface()
{
    WGPUSurfaceCapabilities caps = {};
    wgpuSurfaceGetCapabilities(vk.surface, vk.adapter, &caps);

    vk.surface_format = caps.formats[0];
    for (uint32_t i = 0; i < caps.formatCount; i++)
    {
        if (caps.formats[i] == WGPUTextureFormat_BGRA8Unorm ||
            caps.formats[i] == WGPUTextureFormat_RGBA8Unorm)
        {
            vk.surface_format = caps.formats[i];
            break;
        }
    }
    if (vk.surface_format == caps.formats[0])
    {
        printf("sRGB surface format not found. Using manual shader gamma correction.\n");
    }
    wgpuSurfaceCapabilitiesFreeMembers(caps);

    WGPUSurfaceConfiguration config = {};
    config.device = vk.device;
    config.format = vk.surface_format;
    config.usage = WGPUTextureUsage_RenderAttachment;
    config.alphaMode = WGPUCompositeAlphaMode_Opaque;
    config.presentMode = WGPUPresentMode_Fifo;
    config.width = win.x;
    config.height = win.y;
    config.viewFormatCount = 0;
    config.viewFormats = nullptr;

    wgpuSurfaceConfigure(vk.surface, &config);
}
void PrintString(WGPUStringView sv)
{
    if (sv.data == nullptr || sv.length == 0)
    {
        printf("null\n");
        return;
    }

    std::string_view view(sv.data, sv.length);
    std::cout << view;
}
void DeviceCallback(WGPURequestDeviceStatus status, WGPUDevice device, WGPUStringView message, void* userdata1, void* userdata2)
{
    if (device == nullptr)
    {
        std::cerr << "failed to find device." << std::endl;
        PrintString(message);
        return;
    }

    vk.device = device;
    vk.device_request = true;
}
void AdapterCallback(WGPURequestAdapterStatus status, WGPUAdapter adapter, WGPUStringView message, void* userdata1, void* userdata2)
{
    if (adapter == nullptr)
    {
        std::cerr << "failed to find adapter." << std::endl;
        PrintString(message);
        return;
    }

    vk.adapter = adapter;
    vk.adapter_request = true;
}
void QueueCallback(WGPUQueueWorkDoneStatus status, WGPUStringView message, void* userdata1, void* userdata2)
{
    switch (status)
    {
    case WGPUQueueWorkDoneStatus_Success:
    {
        printf("queued work successful.\n");
    } break;
    case WGPUQueueWorkDoneStatus_CallbackCancelled:
    {
        std::cerr << "callback cancelled." << std::endl;
        PrintString(message);
        std::cout << std::endl;
    } break;
    case WGPUQueueWorkDoneStatus_Error:
    {
        std::cerr << "queued work failed." << std::endl;
        PrintString(message);
        std::cout << std::endl;
    } break;
    default:
    {
        std::cout << "unknown status code: " << status << std::endl;
    } break;
    }
}
void MappedCallback(WGPUMapAsyncStatus status, WGPUStringView message, void* userdata1, void* userdata2)
{
    if (status == WGPUMapAsyncStatus_Success)
    {
        WGPUBuffer buffer = static_cast<WGPUBuffer>(userdata1);

        const void* mapped = wgpuBufferGetMappedRange(buffer, 0, 16);
        //do something

        wgpuBufferUnmap(buffer);
    }
}
void ErrorCallback(WGPUDevice const* device, WGPUErrorType type, WGPUStringView message, void* userdata1, void* userdata2)
{
    if (message.data) {
        std::string_view msg(message.data, message.length == WGPU_STRLEN ? strlen(message.data) : message.length);

        // Print to standard output (if a console window is open)
        std::cout << "[WebGPU Validation Error]: " << msg << std::endl;
    }
}

void InitVk()
{
    if (!glfwInit())
    {
        std::cerr << "could not initialize GLFW." << std::endl;
        return;
    }

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

    win.handle = glfwCreateWindow(1920, 1080, "Earthworm Applications", nullptr, nullptr);
    if (win.handle == nullptr)
    {
        std::cerr << "could not open window." << std::endl;
        return;
    }
    glfwGetFramebufferSize(win.handle, &win.x, &win.y);

    if (glfwRawMouseMotionSupported())
    {
        glfwSetInputMode(win.handle, GLFW_RAW_MOUSE_MOTION, GLFW_TRUE);
    }

    //instance
    vk.instance = wgpuCreateInstance(nullptr);
    if (vk.instance == nullptr)
    {
        std::cerr << "could not create instance." << std::endl;
    }
    std::cout << "instance: " << vk.instance << std::endl;

    //surface
    WGPUSurfaceSourceWindowsHWND hwnd_desc = {};
    hwnd_desc.chain.sType = WGPUSType_SurfaceSourceWindowsHWND;
    hwnd_desc.hinstance = GetModuleHandle(NULL);
    hwnd_desc.hwnd = glfwGetWin32Window(win.handle);

    WGPUSurfaceDescriptor surface_desc = {};
    surface_desc.nextInChain = (WGPUChainedStruct*)&hwnd_desc; //maybe const WGPUChainedStruct* for web Idk
    vk.surface = wgpuInstanceCreateSurface(vk.instance, &surface_desc);

    if (vk.surface == nullptr)
    {
        std::cerr << "could not create surface." << std::endl;
        return;
    }
    std::cout << "surface: " << vk.surface << std::endl;

    //adapter
    WGPURequestAdapterOptions adapter_options = {};
    adapter_options.powerPreference = WGPUPowerPreference_HighPerformance;
    adapter_options.forceFallbackAdapter = false;
    adapter_options.compatibleSurface = vk.surface;

    WGPURequestAdapterCallbackInfo adapter_callback_info = {};
    adapter_callback_info.mode = WGPUCallbackMode_AllowSpontaneous;
    adapter_callback_info.callback = AdapterCallback;
    adapter_callback_info.userdata1 = nullptr;

    wgpuInstanceRequestAdapter(vk.instance, &adapter_options, adapter_callback_info);

    if (vk.adapter == nullptr)
    {
        std::cerr << "could not request adapter." << std::endl;
        return;
    }
    std::cout << "adapter: ";

    WGPULimits adapter_limits = {};
    if (wgpuAdapterGetLimits(vk.adapter, &adapter_limits) == WGPUStatus_Success)
    {
        std::cout << adapter_limits.maxTextureDimension2D << " ";
        std::cout << adapter_limits.maxTextureDimension3D << " ";
        std::cout << adapter_limits.maxVertexBuffers << " ";
        std::cout << adapter_limits.maxTextureArrayLayers << " ";
        std::cout << adapter_limits.maxBindGroups << " ";
        std::cout << adapter_limits.maxBindingsPerBindGroup << " ";
        std::cout << adapter_limits.maxBufferSize << " ";
        std::cout << adapter_limits.maxVertexAttributes << std::endl;
    }

    //device
    WGPUFeatureName required_feature = WGPUFeatureName_IndirectFirstInstance;
    WGPUDeviceDescriptor device_descriptor = {};
    device_descriptor.requiredLimits = &adapter_limits;
    device_descriptor.uncapturedErrorCallbackInfo.callback = ErrorCallback;
    device_descriptor.requiredFeatures = &required_feature;
    device_descriptor.requiredFeatureCount = 1;

    WGPURequestDeviceCallbackInfo device_callback_info = {};
    device_callback_info.mode = WGPUCallbackMode_AllowSpontaneous;
    device_callback_info.callback = DeviceCallback;

    wgpuAdapterRequestDevice(vk.adapter, &device_descriptor, device_callback_info);
#ifdef WEB
    while (!vk.device_request)
    {
        emscripten_sleep(10);
    }
#endif
    if (vk.device == nullptr)
    {
        std::cerr << "could not request device." << std::endl;
        return;
    }
    std::cout << "device:  ";

    WGPULimits actual_limits = {};
    if (wgpuDeviceGetLimits(vk.device, &actual_limits) == WGPUStatus_Success)
    {
        std::cout << actual_limits.maxTextureDimension2D << " ";
        std::cout << actual_limits.maxTextureDimension3D << " ";
        std::cout << actual_limits.maxVertexBuffers << " ";
        std::cout << actual_limits.maxTextureArrayLayers << " ";
        std::cout << actual_limits.maxBindGroups << " ";
        std::cout << actual_limits.maxBindingsPerBindGroup << " ";
        std::cout << actual_limits.maxBufferSize << " ";
        std::cout << actual_limits.maxVertexAttributes << std::endl;
    }

    //depth image
    WGPUTextureDescriptor depthDesc = {};
    depthDesc.usage = WGPUTextureUsage_RenderAttachment;
    depthDesc.dimension = WGPUTextureDimension_2D;
    depthDesc.size = { (uint32_t)win.x, (uint32_t)win.y, 1 };
    depthDesc.format = WGPUTextureFormat_Depth32Float;
    depthDesc.mipLevelCount = 1;
    depthDesc.sampleCount = 1;
    vk.depth_texture = wgpuDeviceCreateTexture(vk.device, &depthDesc);
    vk.depth_view = wgpuTextureCreateView(vk.depth_texture, nullptr);

    //queue
    WGPUQueueWorkDoneCallbackInfo queue_callback_info = {};
    queue_callback_info.callback = QueueCallback;
    queue_callback_info.mode = WGPUCallbackMode_AllowSpontaneous;
    vk.queue = wgpuDeviceGetQueue(vk.device);

    ConfigSurface();
}

void MainLoop()
{
    glfwPollEvents();
    //UpdateGame(vk.frame_delta);
}

int main()
{
    InitVk();
    printf("engine initialized.\n");
    InitWorld();
    printf("world initialized.\n");

    while (!glfwWindowShouldClose(win.handle))
    {
        MainLoop();
    }

    return 0;
}