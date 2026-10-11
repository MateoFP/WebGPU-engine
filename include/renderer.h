#pragma once
#include <webgpu/webgpu.h>
#include <GLFW/glfw3.h>
#include <iostream>
#include <string_view>
#include <vector> 
#include "include/ufbx.h"
#include "include/mateo_math.h"
#include "game.h"

#define WIDTH               1920
#define HEIGHT              1080
#define MAX_INSTANCES       200
#define MAX_INSTANCES_WIRE  200
#define MAX_INSTANCES_UI    100
#define MAX_INDIRECT_CMDS   100
#define MAX_TEXTURES        20
#define MAX_BONES           256

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
    ufbx_anim* animation[5];
    ufbx_mesh* mesh;

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
struct Image
{
    WGPUTexture tex;
    WGPUTextureView view;
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
struct WindowContext
{
    int x;
    int y;
    int extent_x;
    int extent_y;
    GLFWwindow* handle;
    bool focused;
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

    Image depth_image;
    Image tex_array;
    WGPUSampler linear_sampler;

    double accumulator = {};
    float physics_dt = 1.0f / 60.0f;
    double frame_delta = physics_dt;
    double last_time = 0.0;
};

extern EngineContext vk;
extern WindowContext win;
extern GUBO gubo;
extern const char* plane_shader;
extern const char* object_shader;
extern const char* ui_shader;
extern const char* wire_shader;
extern const char* outline_shader;
extern Entity p1;
extern Entity p2;

void InitVk();