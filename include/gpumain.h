#pragma once

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

#define WIDTH               1920
#define HEIGHT              1080
#define MAX_INSTANCES       200
#define MAX_INSTANCES_WIRE  200
#define MAX_INSTANCES_UI    100
#define MAX_INDIRECT_CMDS   100
#define MAX_TEXTURES        20
#define MAX_BONES           256

#define ArrayCount(Array) (sizeof(Array) / sizeof(Array[0]))

const uint32_t FLAG_STONE   = 1 << 0;

struct alignas(16) GUBO
{
    mat4 projView;
    v2 player_dest;
    v2 window;
    float time;
    float time_rb;
    float time_a;
    uint32_t debug;
};
struct BoundingBox
{
    v3 max;
    v3 min;
};
struct PipelineInfo
{
    WGPUBlendState                  blend_state;
    WGPUOptionalBool                depthWriteEnabled;
    WGPUPrimitiveTopology           topology;
    WGPUCompareFunction             depthCompare;
    WGPUCullMode                    cullMode;

    uint32_t                        vertex_buffer_count;
    WGPUVertexBufferLayout          vertex_buffer_layout;
    WGPUVertexAttribute             vertex_attributes[5];
};
struct DrawIndexedIndirectCommand 
{
    uint32_t index_count;    
    uint32_t instance_count; 
    uint32_t first_index;    
    int32_t vertex_offset;
    uint32_t first_instance; 
}; 
struct Submesh
{
    uint32_t first_index;
    uint32_t index_count;
    int32_t  vertex_offset;
};
struct ModelMeshes
{
    ufbx_scene* scene;
    ufbx_anim*  animation[5];
    ufbx_mesh*  mesh;

    uint32_t submesh_index;
    uint32_t submesh_count;
    uint32_t active_instances;
    uint32_t base_instance;
    uint32_t max_instances;
    std::string name;
};
struct InstanceData
{
    mat4 matrix;
    uint32_t id;
    uint32_t skinned;
    uint32_t bone_offset;   
    uint32_t pad;
    v4 color;
};
struct InstanceDataUI
{
    v2 uv;
    v2 size;
    v2 pos;
};
struct InstanceDataWire
{
    v4 max;
    v4 min;
    v4 color;
    uint32_t id;
    uint32_t pad[3];
};
struct Vertex
{
    v3 pos;
    v2 uv;
    float weights[4];
    uint32_t joints[4];
    v3 normals;
};
struct Buffer
{
    WGPUBuffer buffer;
    uint64_t size;
};
struct Autos
{
    v3 origin;
    v3 position;
    v3 destination;
    v3 movement;
    float angle;
    float speed;
    BoundingBox bb;
    bool moving;
};
struct Entity
{
    v3 position;
    v3 destination;
    v3 movement;
    bool moving;
    float angle;
    float speed;
    BoundingBox bb;
    Autos autos;

    ModelMeshes* model_mesh;
    InstanceData instance_data;

    uint32_t current_anim_id = 0;
    double anim_time = 0.0;
    float anim_speed = 1.0f;
    bool is_looping = true;
    bool in_range = false;
    bool can_move = true;
   
    float trigger_time = 0.3f;
    bool triggered_this_loop = false;

    float hp = 100.0f;
};
struct WindowContext
{
    GLFWwindow* window;
    v2 win_size;
    int extent_width;
    int extent_height;
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

    std::vector<mat4>               bones_data;
    std::vector<Vertex>             vertex_data;
    std::vector<uint32_t>           index_data;
    std::vector<ModelMeshes>	    model_meshes;
    std::vector<Submesh>		    meshes_alloc;
    DrawIndexedIndirectCommand      indirect_cmds[MAX_INDIRECT_CMDS];
    InstanceData                    instance_data[MAX_INSTANCES];
    InstanceDataUI                  instance_data_ui[MAX_INSTANCES_UI];
    InstanceDataWire                instance_data_wire[MAX_INSTANCES_WIRE];
    uint32_t                        map_data[676] =
    {
0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 0, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 1, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 
0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 1, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 
0, 0, 0, 1, 1, 1, 0, 1, 0, 1, 1, 0, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 0, 0, 1, 0, 1, 1, 1, 1, 1, 1, 0, 0, 
1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 0, 1, 0, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
1, 1, 0, 0, 1, 1, 1, 0, 0, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 0, 0, 0, 1, 1, 0, 0, 1, 1, 1, 1, 
1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 0, 0, 0, 1, 1, 1, 0, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 
0, 0, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 0, 1, 0, 1, 0, 0, 1, 1, 1, 1, 1, 0, 
0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 0, 1, 1, 1, 0, 1, 1, 0, 0, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 1, 1, 0, 
0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 1, 1, 1, 1, 1, 0, 0, 0, 
0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 0, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1, 1, 1, 1, 1, 1, 0, 
0, 0, 1, 1, 0, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 
0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 0, 0, 0, 1, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 0, 0, 1, 0, 0, 0, 
1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 0, 0, 0, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 
1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 1, 1, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 
1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1, 0, 1, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 1, 0, 
1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 
0, 0, 0, 0,
    };

    uint32_t active_commands = 0;
    uint32_t active_instances = 0;
    uint32_t active_ui = 0;
    uint32_t total_submesh_count = 0;
    uint32_t hovering = 0;

    Buffer vertex_buffer;
    Buffer index_buffer;
    Buffer instance_buffer;
    Buffer instance_ui_buffer;
    Buffer instance_wire_buffer;
    Buffer gubo_buffer;
    Buffer indirect_buffer;
    Buffer map_buffer;
    Buffer bones_buffer;

    WGPUTexture     depth_texture;
    WGPUTextureView depth_view;
    WGPUTexture     texture_array;
    WGPUTextureView texture_array_view;
    WGPUSampler     linear_sampler;

    double accumulator = {};
    float physics_dt = 1.0f / 60.0f;
    double frame_delta = physics_dt;
    double last_time = 0.0;
};

extern GUBO gubo;
extern EngineContext vk;
extern WindowContext win;
extern const char* plane_shader;
extern const char* object_shader;
extern const char* ui_shader;
extern const char* wire_shader;
extern const char* outline_shader;
extern Entity p1;
extern Entity p2;

void PrintString(WGPUStringView sv);
void DeviceCallback(WGPURequestDeviceStatus status, WGPUDevice device, WGPUStringView message, void* userdata1, void* userdata2);
void AdapterCallback(WGPURequestAdapterStatus status, WGPUAdapter adapter, WGPUStringView message, void* userdata1, void* userdata2);
void QueueCallback(WGPUQueueWorkDoneStatus status, WGPUStringView message, void* userdata1, void* userdata2);
void MappedCallback(WGPUMapAsyncStatus status, WGPUStringView message, void* userdata1, void* userdata2);
void ErrorCallback(WGPUDevice const* device, WGPUErrorType type, WGPUStringView message, void* userdata1, void* userdata2);
void ConfigSurface();
WGPUTextureView AcquireNextFrameView(WGPUSurfaceTexture* surface_tex);
WGPUCommandEncoder BeginCMDbuffer();
WGPUCommandBuffer EndCMDBuffer(WGPUCommandEncoder encoder);
void SubmitCMD(WGPUCommandBuffer command);
WGPUTexture CreateTextureArray(uint32_t resolution);
void UploadTextureLayer(uint32_t tex_id, uint32_t width, uint32_t height, const uint8_t* raw_pixels);
void LoadTexture(const char* path, uint32_t tex_id);
void LoadTextures();
PipelineInfo CreatePipelineInfo(uint32_t opaque, uint32_t v_buffer_count, 
                                WGPUOptionalBool write, WGPUPrimitiveTopology topology, 
                                WGPUCompareFunction compare, WGPUCullMode cull_mode);
WGPURenderPipeline CreatePipeline(const char* shaderSource, PipelineInfo pipeline_info);
WGPUBindGroupLayout CreateBindGroupsLayout();
WGPUBindGroup CreateBindGroups(WGPUBindGroupLayout layout);
WGPUBuffer CreateBuffer(WGPUBufferUsage flags, uint64_t size);
WGPUBuffer CreateMappedBuffer(WGPUBufferUsage flags, uint64_t size, const void* initialData);
void CopyBuffer(WGPUBuffer source, WGPUBuffer dest, uint32_t size);
void InitializeBuffers();
void RecordIndirectCmds(ModelMeshes model);
void InitModel(const char* path, uint32_t max_instances, uint32_t offset);
void InitModels();
ModelMeshes LoadModel(const char* path);
void GetBoneMatrices(Entity* player);

void Render();

#ifdef WEB
void SetCursor(const char* path);

extern "C" 
{
    void JS_InitAudio();
    void JS_LoadSound(const char* url_ptr, const char* name_ptr);
    void JS_PlaySound(const char* name_ptr, float volume);
    void JS_PlayMusic(const char* name_ptr, float volume);
    bool JS_IsSoundLoaded(const char* name_ptr);
}
#endif 
