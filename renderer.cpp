#define STB_IMAGE_IMPLEMENTATION 
#define CGLTF_IMPLEMENTATION
#include <GLFW/glfw3.h>
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#include <webgpu/webgpu_cpp.h>
#else
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>
#include <windows.h>
#include <webgpu/wgpu.h>
#endif
#include "include/mateo_math.h"
#include "include/font.h"
#include "include/imgui/imgui.h"
#include "include/stb_image.h"
#include "include/renderer.h"

EngineContext vk = {};
WindowContext win = {};
GameContext gc = {};
GUBO gubo = {};
Input input = {};
Camera cam = {};
Entity p1 = {};
Entity p2 = {};
BoundingBox world_box = {};

#ifdef __EMSCRIPTEN__
EM_JS(void, JS_InitAudio, (),
    {
        window.audioCtx = new (window.AudioContext || window.webkitAudioContext)();
        window.audioBuffers = {};
    });
EM_JS(void, JS_LoadSound, (const char* url_ptr, const char* name_ptr),
    {
        var url = UTF8ToString(url_ptr);
        var name = UTF8ToString(name_ptr);

        fetch(url)
            .then(function(response)
            {
                return response.arrayBuffer();
            })
            .then(function(data)
            {
                return window.audioCtx.decodeAudioData(data);
            })
            .then(function(buffer)
            {
                window.audioBuffers[name] = buffer;
                console.log("audio:", name);
            })
            .catch (function(err)
            {
                console.error("Audio load failed:", err);
            });
    });
EM_JS(void, JS_PlaySound, (const char* name_ptr, float volume),
    {
        const name = UTF8ToString(name_ptr);
        const buffer = window.audioBuffers[name];

        if (!buffer)
        {
            console.warn("Sound not loaded yet:", name);
            return;
        }

        if (window.audioCtx.state == 'suspended')
        {
            window.audioCtx.resume();
        }

        const source = window.audioCtx.createBufferSource();
        source.buffer = buffer;

        const gainNode = window.audioCtx.createGain();
        gainNode.gain.value = volume;

        source.connect(gainNode);
        gainNode.connect(window.audioCtx.destination);

        source.start(0);
    });
EM_JS(void, JS_PlayMusic, (const char* name_ptr, float volume),
    {
        var name = UTF8ToString(name_ptr);
        var buffer = window.audioBuffers[name];
        if (!buffer) return;

        if (window.currentBgmSource)
        {
            try { window.currentBgmSource.stop(); }
            catch (e) {}
        }

        if (window.audioCtx.state == 'suspended')
        {
            window.audioCtx.resume();
        }

        var source = window.audioCtx.createBufferSource();
        source.buffer = buffer;
        source.loop = true;

        var gainNode = window.audioCtx.createGain();
        gainNode.gain.value = volume;
        window.bgmGainNode = gainNode;

        source.connect(gainNode);
        gainNode.connect(window.audioCtx.destination);
        source.start(0);

        window.currentBgmSource = source;
    });
EM_JS(bool, JS_IsSoundLoaded, (const char* name_ptr),
    {
        var name = UTF8ToString(name_ptr);
        return !!window.audioBuffers[name];
    });
void FocusCanvas()
{
    emscripten_run_script("document.getElementById('canvas').focus();");
}
void SetCursor(const char* path)
{
    EM_ASM(
        {
            var pathStr = UTF8ToString($0);
            var x = $1;
            var y = $2;
            var canvas = Module['canvas'] || document.getElementById('canvas') || document.querySelector('canvas');
            if (canvas)
            {
                canvas.style.cursor = 'url("' + pathStr + '") ' + x + ' ' + y + ', auto';
            }
            else
            {
                console.error("SetCursor: could not find HTML canvas element\n");
            }
        }, path, 0, 0);
}
#endif

v3 RayCast(double xpos, double ypos, mat4 view, mat4 projection, float width, float height)
{
    if (view.element[0][0] == 0.0f)
    {
        printf("No valid view matrix.\n");
        return { 0,0,0 };
    }

    float x = (2.0f * (float)xpos) / width - 1.0f;
    float y = 1.0f - (2.0f * (float)ypos) / height;

    v4 ray_clip = { x, y, 0.0f, 1.0f };

    mat4 invProjMatrix = mat4_inv_perspective(projection);
    v4 ray_eye = mat4_v4_linear_combine(ray_clip, invProjMatrix);

    ray_eye.z = -1.0f;
    ray_eye.w = 0.0f;

    mat4 invViewMatrix = mat4_inv_lookat(view);
    v4 inv_ray_wor = mat4_v4_linear_combine(ray_eye, invViewMatrix);

    v3 ray_wor = { inv_ray_wor.x, inv_ray_wor.y, inv_ray_wor.z };
    ray_wor = v3_normalize(ray_wor);

    return ray_wor;
}
RayCollision GetRayCollisionBox(Ray ray, BoundingBox box)
{
    RayCollision collision = { 0 };

    if (v3_dot(ray.direction, ray.direction) < 0.000001f)
    {
        printf("No valid ray.\n");
        return collision = { 0 };
    }

    float inv_dx = 1.0f / ray.direction.x;
    float inv_dy = 1.0f / ray.direction.y;
    float inv_dz = 1.0f / ray.direction.z;

    float t1 = (box.min.x - ray.position.x) * inv_dx;
    float t2 = (box.max.x - ray.position.x) * inv_dx;
    float t3 = (box.min.y - ray.position.y) * inv_dy;
    float t4 = (box.max.y - ray.position.y) * inv_dy;
    float t5 = (box.min.z - ray.position.z) * inv_dz;
    float t6 = (box.max.z - ray.position.z) * inv_dz;

    float tmin = fmaxf(fmaxf(fminf(t1, t2), fminf(t3, t4)), fminf(t5, t6));
    float tmax = fminf(fminf(fmaxf(t1, t2), fmaxf(t3, t4)), fmaxf(t5, t6));

    if (tmax < 0.0f || tmin > tmax)
    {
        collision.hit = false;
        return collision;
    }

    collision.hit = true;

    bool insideBox = (tmin < 0.0f);
    if (insideBox)
    {
        collision.distance = tmax;
    }
    else
    {
        collision.distance = tmin;
    }

    collision.point = (ray.position + (collision.distance * ray.direction));

    v3 box_center = v3_lerp(box.min, box.max, 0.5f);
    v3 box_extents = box.max - box.min;

    collision.normal = collision.point - box_center;
    collision.normal *= 2.001f;
    collision.normal = v3_divide(collision.normal, box_extents);

    collision.normal.x = roundf(collision.normal.x);
    collision.normal.y = roundf(collision.normal.y);
    collision.normal.z = roundf(collision.normal.z);
    collision.normal = v3_normalize(collision.normal);

    if (insideBox)
    {
        collision.normal = -collision.normal;
    }

    return collision;
}
mat4 TSR(v3 translate, float scale, float angle)
{
    mat4 result = mat4_diagonal(1.0f);

    mat4 scale_mat = mat4_scale({ scale, scale, scale });
    mat4 rotate_mat = mat4_rotate_RH(angle, { 0.0f, 0.0f, 1.0f });
    mat4 translate_mat = mat4_translate({ translate.x, translate.y, translate.z });

    result = mat4_multiply(translate_mat, mat4_multiply(rotate_mat, scale_mat));

    return result;
}
v2 WorldToScreen(v3 world_pos, mat4 view_proj)
{
    float clip_x = world_pos.x * view_proj.column[0].x + world_pos.y * view_proj.column[1].x + world_pos.z * view_proj.column[2].x + view_proj.column[3].x;
    float clip_y = world_pos.x * view_proj.column[0].y + world_pos.y * view_proj.column[1].y + world_pos.z * view_proj.column[2].y + view_proj.column[3].y;
    float clip_w = world_pos.x * view_proj.column[0].w + world_pos.y * view_proj.column[1].w + world_pos.z * view_proj.column[2].w + view_proj.column[3].w;

    float ndc_x = clip_x / clip_w;
    float ndc_y = clip_y / clip_w;

    v2 position_2d;
    position_2d.x = (ndc_x + 1.0f) * 0.5f * 1920.0f;
    position_2d.y = (1.0f - ndc_y) * 0.5f * 1080.0f;

    return position_2d;
}
void UpdateStatic()
{
    ModelMeshes& model = vk.model_meshes[1];
    vk.model_meshes[1].active_instances = 42;

    for (uint32_t i = 0; i < model.submesh_count; ++i)
    {
        vk.indirect_cmds[model.submesh_index + i].instance_count = model.active_instances;
    }

    ModelMeshes& model2 = vk.model_meshes[2];
    vk.model_meshes[2].active_instances = 2;

    for (uint32_t i = 0; i < model2.submesh_count; ++i)
    {
        vk.indirect_cmds[model2.submesh_index + i].instance_count = model2.active_instances;
    }

    size_t indirect_upload_size = sizeof(DrawIndexedIndirectCommand) * 100;
    wgpuQueueWriteBuffer(vk.queue, vk.indirect_buffer.buffer, 0, vk.indirect_cmds, indirect_upload_size);

    vk.instance_data[0].matrix = TSR({ 0.0f, 0.0f, 0.0f }, 1.0f, 0.0f);
    vk.instance_data[0].id = 1;
    vk.instance_data[0].skinned = 0;
    vk.instance_data[0].color = { 0.5f, 0.5f, 0.5f, 1.0f };
    vk.instance_data[1].matrix = TSR({ 19.6461, 47.1684, 0.0f }, 0.7f, 0.0f);
    vk.instance_data[2].matrix = TSR({ 1.58028, 27.5539, 0.0f }, 0.7f, 0.0f);
    vk.instance_data[3].matrix = TSR({ 0.876242, 24.0858, 0.0f }, 0.7f, 0.0f);
    vk.instance_data[4].matrix = TSR({ 2.7633, 21.7043, 0.0f }, 0.7f, 0.0f);
    vk.instance_data[5].matrix = TSR({ 1.54829, 18.8526, 0.0f }, 0.7f, 0.0f);
    vk.instance_data[6].matrix = TSR({ 2.56049, 16.1132, 0.0f }, 0.7f, 0.0f);
    vk.instance_data[7].matrix = TSR({ 0.465165, 13.4252, 0.0f }, 0.7f, 0.0f);
    vk.instance_data[8].matrix = TSR({ 13.8049, 0.872237, 0.0f }, 0.7f, 0.0f);
    vk.instance_data[9].matrix = TSR({ 17.0794, 2.28585, 0.0f }, 0.7f, 0.0f);
    vk.instance_data[10].matrix = TSR({ 20.7312, 0.548582, 0.0f }, 0.7f, 0.0f);
    vk.instance_data[11].matrix = TSR({ 25.3496, 1.68974, 0.0f }, 0.7f, 0.0f);
    vk.instance_data[12].matrix = TSR({ 29.5099, 1.06734, 0.0f }, 0.7f, 0.0f);
    vk.instance_data[13].matrix = TSR({ 34.0692, 1.26314, 0.0f }, 0.7f, 0.0f);
    vk.instance_data[14].matrix = TSR({ 38.3785, 0.774941, 0.0f }, 0.7f, 0.0f);
    vk.instance_data[15].matrix = TSR({ 43.3268, 2.18606, 0.0f }, 0.7f, 0.0f);
    vk.instance_data[16].matrix = TSR({ 44.5613, 1.00223, 0.0f }, 0.7f, 0.0f);
    vk.instance_data[17].matrix = TSR({ 49.4174, 2.02013, 0.0f }, 0.7f, 0.0f);
    vk.instance_data[18].matrix = TSR({ 41.7479, 1.19775, 0.0f }, 0.7f, 0.0f);
    vk.instance_data[19].matrix = TSR({ 48.0504, 2.71727, 0.0f }, 0.7f, 0.0f);
    vk.instance_data[20].matrix = TSR({ 47.2918, 0.80998, 0.0f }, 0.7f, 0.0f);
    vk.instance_data[21].matrix = TSR({ 48.6042, 7.93406, 0.0f }, 0.7f, 0.0f);
    vk.instance_data[22].matrix = TSR({ 46.2745, 6.78893, 0.0f }, 0.7f, 0.0f);
    vk.instance_data[23].matrix = TSR({ 48.1731, 12.3996, 0.0f }, 0.7f, 0.0f);
    vk.instance_data[24].matrix = TSR({ 48.4973, 17.2712, 0.0f }, 0.7f, 0.0f);
    vk.instance_data[25].matrix = TSR({ 48.6649, 22.1225, 0.0f }, 0.7f, 0.0f);
    vk.instance_data[26].matrix = TSR({ 47.1194, 25.3414, 0.0f }, 0.7f, 0.0f);
    vk.instance_data[27].matrix = TSR({ 48.7091, 29.3777, 0.0f }, 0.7f, 0.0f);
    vk.instance_data[28].matrix = TSR({ 48.6705, 35.184, 0.0f }, 0.7f, 0.0f);
    vk.instance_data[29].matrix = TSR({ 0.760804, 31.4633, 0.0f }, 0.7f, 0.0f);
    vk.instance_data[30].matrix = TSR({ 2.0965, 35.2207, 0.0f }, 0.7f, 0.0f);
    vk.instance_data[31].matrix = TSR({ 0.841536, 39.2842, 0.0f }, 0.7f, 0.0f);
    vk.instance_data[32].matrix = TSR({ 2.89485, 43.6928, 0.0f }, 0.7f, 0.0f);
    vk.instance_data[33].matrix = TSR({ 1.44184, 48.3296, 0.0f }, 0.7f, 0.0f);
    vk.instance_data[34].matrix = TSR({ 5.59274, 48.3067, 0.0f }, 0.7f, 0.0f);
    vk.instance_data[35].matrix = TSR({ 9.56756, 49.1769, 0.0f }, 0.7f, 0.0f);
    vk.instance_data[36].matrix = TSR({ 13.3267, 47.9841, 0.0f }, 0.7f, 0.0f);
    vk.instance_data[37].matrix = TSR({ 17.6959, 48.8852, 0.0f }, 0.7f, 0.0f);
    vk.instance_data[38].matrix = TSR({ 22.8705, 48.3067, 0.0f }, 0.7f, 0.0f);
    vk.instance_data[39].matrix = TSR({ 27.6375, 48.8126, 0.0f }, 0.7f, 0.0f);
    vk.instance_data[40].matrix = TSR({ 32.8059, 48.1631, 0.0f }, 0.7f, 0.0f);
    vk.instance_data[41].matrix = TSR({ 37.5887, 49.1769, 0.0f }, 0.7f, 0.0f);
    vk.instance_data[42].matrix = TSR({ 31.0865, 47.098, 0.0f }, 0.7f, 0.0f);
    vk.instance_data[43].matrix = TSR({ 9.79052, 46.9576, 0.0f }, 0.7f, 0.0f);

    for (uint32_t i = vk.model_meshes[1].base_instance; i < vk.model_meshes[1].active_instances + 2; i++)
    {
        vk.instance_data[i].id = 4;
        vk.instance_data[i].skinned = 0;
        vk.instance_data[i].color = {};
    }

    size_t instance_upload_size = sizeof(InstanceData) * 200;
    wgpuQueueWriteBuffer(vk.queue, vk.instance_buffer.buffer, 0, vk.instance_data, instance_upload_size);
}

void WriteLargeBarEnemy(v3 pos, float height, float hp)
{
    uint32_t offset = vk.active_ui;

    v2 bar = WorldToScreen({ pos.x, pos.y, pos.z + height }, gubo.projView);
    bar.x -= 50.0f;
    bar.y -= 45.0f;

    vk.instance_data_ui[offset].uv = { 0.0f, 0.0f };
    vk.instance_data_ui[offset].size = { 100.0f, 18.0f };
    vk.instance_data_ui[offset].pos = { bar };

    vk.instance_data_ui[offset + 1].uv = { 300.0f, 0.0f };
    vk.instance_data_ui[offset + 1].size = { hp, 18.0f };
    vk.instance_data_ui[offset + 1].pos = { bar };

    vk.instance_data_ui[offset + 2].uv = { 200.0f, 0.0f };
    vk.instance_data_ui[offset + 2].size = { 100.0f, 18.0f };
    vk.instance_data_ui[offset + 2].pos = { bar };

    vk.active_ui = vk.active_ui + 3;
}
void WriteLargeBar(v3 pos, float height, float hp)
{
    uint32_t offset = vk.active_ui;

    v2 bar = WorldToScreen({ pos.x, pos.y, pos.z + height }, gubo.projView);
    bar.x -= 50.0f;
    bar.y -= 45.0f;

    vk.instance_data_ui[offset].uv = { 0.0f, 0.0f };
    vk.instance_data_ui[offset].size = { 100.0f, 18.0f };
    vk.instance_data_ui[offset].pos = { bar };

    vk.instance_data_ui[offset + 1].uv = { 100.0f, 0.0f };
    vk.instance_data_ui[offset + 1].size = { hp, 18.0f };
    vk.instance_data_ui[offset + 1].pos = { bar };

    vk.instance_data_ui[offset + 2].uv = { 200.0f, 0.0f };
    vk.instance_data_ui[offset + 2].size = { 100.0f, 18.0f };
    vk.instance_data_ui[offset + 2].pos = { bar };

    vk.active_ui = vk.active_ui + 3;
}
void WriteSmallBar(v3 pos, float height)
{
    uint32_t offset = vk.active_ui;

    v2 bar = WorldToScreen({ pos.x, pos.y, pos.z + height }, gubo.projView);
    bar.x -= 25.0f;
    bar.y -= 45.0f;

    vk.instance_data_ui[offset].uv = { 0.0f, 19.0f };
    vk.instance_data_ui[offset].size = { 50.0f, 7.0f };
    vk.instance_data_ui[offset].pos = { bar };

    vk.instance_data_ui[offset + 1].uv = { 100.0f, 19.0f };
    vk.instance_data_ui[offset + 1].size = { 50.0f, 7.0f };
    vk.instance_data_ui[offset + 1].pos = { bar };

    vk.active_ui = vk.active_ui + 2;
}

void UpdateGPU()
{
    cam.view = mat4_look_at(cam.pos, cam.look_at, { 0.0f, 0.0f, 1.0f });
    gubo.projView = mat4_multiply(cam.proj, cam.view);
    wgpuQueueWriteBuffer(vk.queue, vk.gubo_buffer.buffer, 0, &gubo, sizeof(gubo));

    vk.instance_data[vk.model_meshes[2].base_instance].matrix = TSR({ p1.position.x, p1.position.y, 0.2f }, 1.0f, p1.angle);
    vk.instance_data[vk.model_meshes[2].base_instance].id = 3;
    vk.instance_data[vk.model_meshes[2].base_instance].skinned = 1;
    vk.instance_data[vk.model_meshes[2].base_instance].bone_offset = 0;
    vk.instance_data[vk.model_meshes[2].base_instance].color = { 0.0f, 0.0f, 0.0f , 1.0f };

    vk.instance_data[vk.model_meshes[2].base_instance + 1].matrix = TSR({ p2.position.x, p2.position.y, 0.2f }, 1.0f, p2.angle);
    vk.instance_data[vk.model_meshes[2].base_instance + 1].id = 3;
    vk.instance_data[vk.model_meshes[2].base_instance + 1].skinned = 1;
    vk.instance_data[vk.model_meshes[2].base_instance + 1].bone_offset = 33;
    vk.instance_data[vk.model_meshes[2].base_instance + 1].color = { 0.0f, 0.0f, 0.0f , 1.0f };

    //vk.instance_data[vk.model_meshes[3].base_instance].matrix = TSR({ minion.position.x, minion.position.y, 0.0f }, 0.3f, 0.0f);
    //vk.instance_data[vk.model_meshes[3].base_instance].id = 5;
    //vk.instance_data[vk.model_meshes[3].base_instance].skinned = 1;
    //vk.instance_data[vk.model_meshes[3].base_instance].bone_offset = 66;
    //vk.instance_data[vk.model_meshes[3].base_instance].color = { 0.1f, 0.1f, 0.1f , 1.0f };

    size_t start_index = vk.model_meshes[2].base_instance;
    uint64_t gpu_byte_offset = start_index * sizeof(InstanceData);
    size_t instance_upload_size = sizeof(InstanceData) * 2;
    const void* cpu_data_pointer = &vk.instance_data[start_index];
    wgpuQueueWriteBuffer(vk.queue, vk.instance_buffer.buffer, gpu_byte_offset, cpu_data_pointer, instance_upload_size);

    vk.active_ui = 0;
    //WriteSmallBar(minion.position, 1.0f);
    WriteLargeBarEnemy(p2.position, 3.7f, p2.hp);
    WriteLargeBar(p1.position, 3.7f, p1.hp);

    vk.instance_data_ui[vk.active_ui].uv = { 0.0f, 128.0f };
    vk.instance_data_ui[vk.active_ui].size = { 512.0f, 384.0f };
    vk.instance_data_ui[vk.active_ui].pos = { 0.0f, 0.0f };
    vk.active_ui = vk.active_ui + 1;

    size_t ui_upload_size = sizeof(InstanceDataUI) * vk.active_ui;
    wgpuQueueWriteBuffer(vk.queue, vk.instance_ui_buffer.buffer, 0, vk.instance_data_ui, ui_upload_size);

    float offset = 1.5f;
    float o_auto = 0.12f;

    p1.bb.max = { p1.position.x + offset, p1.position.y + offset, p1.position.z + 2.7f };
    p1.bb.min = { p1.position.x - offset, p1.position.y - offset, p1.position.z };

    p2.bb.max = { p2.position.x + offset, p2.position.y + offset, p2.position.z + 2.7f };
    p2.bb.min = { p2.position.x - offset, p2.position.y - offset, p2.position.z };

    p1.autos.bb.min = { p1.autos.position.x + o_auto,  p1.autos.position.y + o_auto,  p1.autos.position.z + 4.2f };
    p1.autos.bb.max = { p1.autos.position.x - o_auto,  p1.autos.position.y - o_auto,  p1.autos.position.z + 4.0f };

    p2.autos.bb.min = { p2.autos.position.x + o_auto,  p2.autos.position.y + o_auto,  p2.autos.position.z + 4.2f };
    p2.autos.bb.max = { p2.autos.position.x - o_auto,  p2.autos.position.y - o_auto,  p2.autos.position.z + 4.0f };

    vk.instance_data_wire[0].max = { p1.autos.bb.max.x, p1.autos.bb.max.y, p1.autos.bb.max.z , 0.0f };
    vk.instance_data_wire[0].min = { p1.autos.bb.min.x, p1.autos.bb.min.y, p1.autos.bb.min.z , 0.0f };
    vk.instance_data_wire[0].color = { 1.0f, 0.0f, 0.0f, 1.0f };
    vk.instance_data_wire[0].id = 0;

    vk.instance_data_wire[1].max = { p2.autos.bb.max.x, p2.autos.bb.max.y, p2.autos.bb.max.z , 0.0f };
    vk.instance_data_wire[1].min = { p2.autos.bb.min.x, p2.autos.bb.min.y, p2.autos.bb.min.z , 0.0f };
    vk.instance_data_wire[1].color = { 1.0f, 0.0f, 0.0f, 1.0f };
    vk.instance_data_wire[1].id = 0;

    vk.instance_data_wire[2].max = { p1.bb.max.x, p1.bb.max.y, p1.bb.max.z };
    vk.instance_data_wire[2].min = { p1.bb.min.x, p1.bb.min.y, p1.bb.min.z };
    vk.instance_data_wire[2].color = { 1.0f, 0.0f, 0.0f, 1.0f };
    vk.instance_data_wire[2].id = 0;

    vk.instance_data_wire[3].max = { p2.bb.max.x, p2.bb.max.y, p2.bb.max.z };
    vk.instance_data_wire[3].min = { p2.bb.min.x, p2.bb.min.y, p2.bb.min.z };
    vk.instance_data_wire[3].color = { 1.0f, 0.0f, 0.0f, 1.0f };
    vk.instance_data_wire[3].id = 0;

    size_t bb_upload_size = sizeof(InstanceDataWire) * 4;
    wgpuQueueWriteBuffer(vk.queue, vk.instance_wire_buffer.buffer, 0, vk.instance_data_wire, bb_upload_size);
}
void UpdateEntityAnimation(Entity* entity, float delta)
{
    entity->anim_time += (double)delta * entity->anim_speed;
    ufbx_anim* current_anim = entity->model_mesh->animation[entity->current_anim_id];

    if (current_anim)
    {
        double duration = current_anim->time_end - current_anim->time_begin;

        if (entity->is_looping && duration > 0.0)
        {
            entity->anim_time = fmod(entity->anim_time, duration);
        }
        else if (entity->anim_time > duration)
        {
            entity->anim_time = duration;
        }
    }
}
void CheckAnimationEvents(Entity* entity, float delta)
{
    if (entity->anim_time < 0.1f)
    {
        entity->triggered_this_loop = false;
    }

    if ((!entity->triggered_this_loop) && (entity->anim_time >= entity->trigger_time) && (entity->current_anim_id == 3))
    {
        entity->autos.moving = true;
#ifdef __EMSCRIPTEN__
        JS_PlaySound("click", 0.015f);
#endif
        entity->triggered_this_loop = true;
    }
}
void SetEntityAnimation(Entity* entity, uint32_t new_anim_id, bool loop)
{
    if (entity->current_anim_id != new_anim_id)
    {
        entity->current_anim_id = new_anim_id;
        entity->anim_time = 0.0;
        entity->is_looping = loop;
    }
}

void KeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods)
{   
    switch (key)
    {
        
        case GLFW_KEY_Q: 
        {

        } break;
        case GLFW_KEY_E:
        {

        } break;
        case GLFW_KEY_S:
        {
            if (action == GLFW_PRESS)
            {
                p1.moving = false;
                p1.destination = p1.position;
            }
        } break;
        case GLFW_KEY_B:
        {
            if (action == GLFW_PRESS)
            {
                // if (gubo.debug)
                // {
                //     gubo.debug = 0;
                // }
                 //else { gubo.debug = 1; }
            }
        } break;
        case GLFW_KEY_SPACE:
        {
            if (action == GLFW_PRESS)
            {
                input.spacebar.ended_down = true;
            }
            if (action == GLFW_RELEASE)
            {
                input.spacebar.ended_down = false;
            }
        } break;
        case GLFW_KEY_A:
        {
            if (action == GLFW_PRESS)
            {
                input.a.pressed = true;
            }
        } break;
    }
}
void CursorPosCallback(GLFWwindow* window, double x, double y)
{
    input.mouse_pos.x = clamp(x, 0.0f, win.extent_x - 20.0f);
    input.mouse_pos.y = clamp(y, 0.0f, win.extent_y - 20.0f);
}
void MouseButtonCallback(GLFWwindow* window, int button, int action, int mods)
{
    switch (button)
    {
        case GLFW_MOUSE_BUTTON_RIGHT:
        {
            if (action == GLFW_PRESS)
            {
                input.collision = GetRayCollisionBox(input.ray, p2.bb);
                if (input.collision.hit)
                {
                    p1.destination = input.collision.point;
                    p1.movement.x = p1.destination.x - p1.position.x;
                    p1.movement.y = p1.destination.y - p1.position.y;

                    p1.angle = (atan2f(p1.movement.y, p1.movement.x)) + 1.5708;

                    p1.moving = false;
                    SetEntityAnimation(&p1, 3, true);
                }
                else
                {
                    input.collision = GetRayCollisionBox(input.ray, world_box);
                    if (!input.collision.hit) break;

                    p1.destination = input.collision.point;
                    p1.movement.x = p1.destination.x - p1.position.x;
                    p1.movement.y = p1.destination.y - p1.position.y;
                    p1.movement = v3_normalize(p1.movement);
                    p1.movement *= p1.speed;

                    p1.angle = (atan2f(p1.movement.y, p1.movement.x)) + 1.5708;
                    p1.moving = true;

                    //UpdateVertexGrid();
                    //AddTree();
                }
            }
        } break;
        case GLFW_MOUSE_BUTTON_LEFT:
        {
            if (action == GLFW_PRESS)
            {
                if (input.a.pressed)
                {
                    input.collision = GetRayCollisionBox(input.ray, p2.bb);
                    if (!input.collision.hit) break;

                    p1.destination = input.collision.point;
                    p1.movement.x = p1.destination.x - p1.position.x;
                    p1.movement.y = p1.destination.y - p1.position.y;

                    p1.angle = (atan2f(p1.movement.y, p1.movement.x)) + 1.5708;

                    p1.moving = false;
                    SetEntityAnimation(&p1, 3, true);
                    input.a.pressed = false;
                }
                //DeleteTree();
            }
        } break;
    }
}
void FocusCallback(GLFWwindow* window, int focused)
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
    config.width = win.extent_x;
    config.height = win.extent_y;
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

WGPUTextureView AcquireNextFrameView(WGPUSurfaceTexture* surface_tex)
{
    wgpuSurfaceGetCurrentTexture(vk.surface, surface_tex);

    if (surface_tex->status != WGPUSurfaceGetCurrentTextureStatus_SuccessOptimal &&
        surface_tex->status != WGPUSurfaceGetCurrentTextureStatus_SuccessSuboptimal)
    {
        std::cerr << "failed to acquire surface texture. status: "
            << static_cast<int>(surface_tex->status)
            << std::endl;
        wgpuTextureRelease(surface_tex->texture);
        return nullptr;
    }

    WGPUTextureViewDescriptor view_desc = {};
    view_desc.format = vk.surface_format;
    view_desc.dimension = WGPUTextureViewDimension_2D;
    view_desc.baseMipLevel = 0;
    view_desc.mipLevelCount = 1;
    view_desc.baseArrayLayer = 0;
    view_desc.arrayLayerCount = 1;
    view_desc.aspect = WGPUTextureAspect_All;

    WGPUTextureView view = wgpuTextureCreateView(surface_tex->texture, &view_desc);

    return view;
}
WGPUCommandEncoder BeginCMDbuffer()
{
    WGPUCommandEncoderDescriptor encoder_desc = {};
    WGPUCommandEncoder encoder = wgpuDeviceCreateCommandEncoder(vk.device, &encoder_desc);
    return encoder;
}
WGPUCommandBuffer EndCMDBuffer(WGPUCommandEncoder encoder)
{
    WGPUCommandBufferDescriptor cmd_buffer_desc = {};
    WGPUCommandBuffer command = wgpuCommandEncoderFinish(encoder, &cmd_buffer_desc);
    wgpuCommandEncoderRelease(encoder);
    return command;
}
void SubmitCMD(WGPUCommandBuffer command)
{
    WGPUCommandBuffer command_buffers[] = { command };
    wgpuQueueSubmit(vk.queue, 1, command_buffers);
    wgpuCommandBufferRelease(command);
}
WGPUTexture CreateTextureArray(uint32_t resolution)
{
    WGPUTextureDescriptor desc = {};
    desc.dimension = WGPUTextureDimension_2D;
    desc.format = WGPUTextureFormat_RGBA8UnormSrgb;
    desc.size.width = resolution;
    desc.size.height = resolution;
    desc.size.depthOrArrayLayers = MAX_TEXTURES;
    desc.mipLevelCount = 1;
    desc.sampleCount = 1;
    desc.usage = WGPUTextureUsage_TextureBinding | WGPUTextureUsage_CopyDst;

    WGPUTexture tex_array = {};
    return tex_array = wgpuDeviceCreateTexture(vk.device, &desc);
}
void UploadTextureLayer(uint32_t tex_id, uint32_t width, uint32_t height, const uint8_t* raw_pixels)
{
    WGPUTexelCopyTextureInfo dest = {};
    dest.texture = vk.tex_array.tex;
    dest.mipLevel = 0;
    dest.origin.x = 0;
    dest.origin.y = 0;
    dest.origin.z = tex_id;
    dest.aspect = WGPUTextureAspect_All;

    uint32_t bytes_per_row = ((width * 4) + 255) & ~255;

    WGPUTexelCopyBufferLayout layout = {};
    layout.offset = 0;
    layout.bytesPerRow = bytes_per_row;
    layout.rowsPerImage = height;

    WGPUExtent3D extent = {};
    extent.width = width;
    extent.height = height;
    extent.depthOrArrayLayers = 1;

    wgpuQueueWriteTexture(vk.queue, &dest, raw_pixels, width * height * 4, &layout, &extent);
}
void LoadTexture(const char* path, uint32_t tex_id)
{
    int w, h, c;
    stbi_uc* pixels = stbi_load(path, &w, &h, &c, 4);

    if (pixels == nullptr)
    {
        std::cerr << "failed to load texture: " << path << std::endl;
        return;
    }

    uint32_t image_size = w * h * 4;
    uint8_t* dest = (uint8_t*)malloc(image_size);
    memcpy(dest, pixels, image_size);
    stbi_image_free(pixels);

    UploadTextureLayer(tex_id, w, h, dest);

    free(dest);
}
void LoadTextures()
{
    uint32_t offset = 0;
    LoadTexture("assets/alphamask.png", offset);
    offset++;
    LoadTexture("assets/brick.png", offset);
    offset++;
    LoadTexture("assets/dirt.png", offset);
    offset++;
    LoadTexture("assets/player.png", offset);
    offset++;
    LoadTexture("assets/tree.png", offset);
    offset++;
    LoadTexture("assets/stone_blue.png", offset);
    offset++;
    LoadTexture("assets/atlas_ui.png", offset);
}
PipelineInfo CreatePipelineInfo(uint32_t opaque, uint32_t v_buffer_count, WGPUOptionalBool write, WGPUPrimitiveTopology topology,
                                WGPUCompareFunction compare, WGPUCullMode cull_mode)
{
    PipelineInfo result = {};

    WGPUBlendState blend_state = {};

    if (opaque)
    {
        blend_state.color.operation = WGPUBlendOperation_Add;
        blend_state.color.srcFactor = WGPUBlendFactor_One;
        blend_state.color.dstFactor = WGPUBlendFactor_Zero;
        blend_state.alpha.operation = WGPUBlendOperation_Add;
        blend_state.alpha.srcFactor = WGPUBlendFactor_One;
        blend_state.alpha.dstFactor = WGPUBlendFactor_Zero;
    }
    else
    {
        blend_state.color.operation = WGPUBlendOperation_Add;
        blend_state.color.srcFactor = WGPUBlendFactor_SrcAlpha;
        blend_state.color.dstFactor = WGPUBlendFactor_OneMinusSrcAlpha;
        blend_state.alpha.operation = WGPUBlendOperation_Add;
        blend_state.alpha.srcFactor = WGPUBlendFactor_One;
        blend_state.alpha.dstFactor = WGPUBlendFactor_OneMinusSrcAlpha;
    }

    if (v_buffer_count == 1)
    {
        result.vertex_attributes[0] = { .format = WGPUVertexFormat_Float32x3, .offset = offsetof(Vertex, pos),     .shaderLocation = 0 };
        result.vertex_attributes[1] = { .format = WGPUVertexFormat_Float32x2, .offset = offsetof(Vertex, uv),      .shaderLocation = 1 };
        result.vertex_attributes[2] = { .format = WGPUVertexFormat_Float32x4, .offset = offsetof(Vertex, weights), .shaderLocation = 2 };
        result.vertex_attributes[3] = { .format = WGPUVertexFormat_Uint32x4,  .offset = offsetof(Vertex, joints),  .shaderLocation = 3 };
        result.vertex_attributes[4] = { .format = WGPUVertexFormat_Float32x3, .offset = offsetof(Vertex, normals), .shaderLocation = 4 };

        result.vertex_buffer_layout.attributeCount = 5;
        result.vertex_buffer_layout.attributes = result.vertex_attributes;
        result.vertex_buffer_layout.arrayStride = sizeof(Vertex);
        result.vertex_buffer_layout.stepMode = WGPUVertexStepMode_Vertex;
        result.vertex_buffer_count = 1;
    }
    else
    {
        result.vertex_buffer_count = 0;
        result.vertex_buffer_layout = {};
    }

    result.blend_state = blend_state;
    result.depthCompare = compare;
    result.depthWriteEnabled = write;
    result.topology = topology;
    result.cullMode = cull_mode;

    return result;
}
WGPURenderPipeline CreatePipeline(const char* shaderSource, PipelineInfo pipeline_info)
{
    WGPUShaderSourceWGSL shaderCodeDesc = {};
    shaderCodeDesc.chain.sType = WGPUSType_ShaderSourceWGSL;
    shaderCodeDesc.chain.next = nullptr;
    shaderCodeDesc.code = { shaderSource, WGPU_STRLEN };
    WGPUShaderModuleDescriptor shaderDesc = {};
    shaderDesc.nextInChain = &shaderCodeDesc.chain;

    WGPUShaderModule shaderModule = wgpuDeviceCreateShaderModule(vk.device, &shaderDesc);

    WGPUColorTargetState colorTarget = {};
    colorTarget.format = vk.surface_format;
    colorTarget.blend = &pipeline_info.blend_state;
    colorTarget.writeMask = WGPUColorWriteMask_All;

    WGPUFragmentState fragmentState = {};
    fragmentState.module = shaderModule;
    fragmentState.entryPoint = WGPUStringView{ "fs_main", WGPU_STRLEN };
    fragmentState.constantCount = 0;
    fragmentState.constants = nullptr;
    fragmentState.targetCount = 1;
    fragmentState.targets = &colorTarget;

    WGPUDepthStencilState depthStencilState = {};
    depthStencilState.format = WGPUTextureFormat_Depth32Float;
    depthStencilState.depthWriteEnabled = pipeline_info.depthWriteEnabled;
    depthStencilState.depthCompare = pipeline_info.depthCompare;

    WGPURenderPipelineDescriptor pipelineDesc = {};
    pipelineDesc.vertex.bufferCount = pipeline_info.vertex_buffer_count;
    pipelineDesc.vertex.buffers = &pipeline_info.vertex_buffer_layout;
    pipelineDesc.vertex.module = shaderModule;
    pipelineDesc.vertex.entryPoint = WGPUStringView{ "vs_main", WGPU_STRLEN };
    pipelineDesc.vertex.constantCount = 0;
    pipelineDesc.vertex.constants = nullptr;
    pipelineDesc.primitive.topology = pipeline_info.topology;
    pipelineDesc.primitive.stripIndexFormat = WGPUIndexFormat_Undefined;
    pipelineDesc.primitive.frontFace = WGPUFrontFace_CCW;
    pipelineDesc.primitive.cullMode = pipeline_info.cullMode;
    pipelineDesc.depthStencil = &depthStencilState;
    pipelineDesc.multisample.count = 1;
    pipelineDesc.multisample.mask = ~0u;
    pipelineDesc.multisample.alphaToCoverageEnabled = false;
    pipelineDesc.fragment = &fragmentState;
    pipelineDesc.layout = vk.global_pipeline_layout;

    WGPURenderPipeline pipeline = wgpuDeviceCreateRenderPipeline(vk.device, &pipelineDesc);

    wgpuShaderModuleRelease(shaderModule);

    return pipeline;
}
WGPUBindGroupLayout CreateBindGroupsLayout()
{
    WGPUBindGroupLayoutEntry entries[8] = {};
    //gubo
    entries[0].binding = 0;
    entries[0].visibility = WGPUShaderStage_Vertex | WGPUShaderStage_Fragment;
    entries[0].buffer.type = WGPUBufferBindingType_Uniform;
    entries[0].buffer.minBindingSize = sizeof(GUBO);
    //instances
    entries[1].binding = 1;
    entries[1].visibility = WGPUShaderStage_Vertex | WGPUShaderStage_Fragment;
    entries[1].buffer.type = WGPUBufferBindingType_ReadOnlyStorage;
    entries[1].buffer.minBindingSize = sizeof(InstanceData) * MAX_INSTANCES;
    //texture array
    entries[2].binding = 2;
    entries[2].visibility = WGPUShaderStage_Fragment;
    entries[2].texture.sampleType = WGPUTextureSampleType_Float;
    entries[2].texture.viewDimension = WGPUTextureViewDimension_2DArray;
    entries[2].texture.multisampled = false;
    //sampler
    entries[3].binding = 3;
    entries[3].visibility = WGPUShaderStage_Fragment;
    entries[3].sampler.type = WGPUSamplerBindingType_Filtering;
    //map
    entries[4].binding = 4;
    entries[4].visibility = WGPUShaderStage_Vertex | WGPUShaderStage_Fragment;
    entries[4].buffer.type = WGPUBufferBindingType_ReadOnlyStorage;
    entries[4].buffer.minBindingSize = sizeof(vk.map_data);
    //bones
    entries[5].binding = 5;
    entries[5].visibility = WGPUShaderStage_Vertex | WGPUShaderStage_Fragment;
    entries[5].buffer.type = WGPUBufferBindingType_ReadOnlyStorage;
    entries[5].buffer.minBindingSize = sizeof(mat4) * MAX_BONES;
    //instances ui
    entries[6].binding = 6;
    entries[6].visibility = WGPUShaderStage_Vertex | WGPUShaderStage_Fragment;
    entries[6].buffer.type = WGPUBufferBindingType_ReadOnlyStorage;
    entries[6].buffer.minBindingSize = sizeof(InstanceDataUI) * MAX_INSTANCES_UI;
    //instances bb
    entries[7].binding = 7;
    entries[7].visibility = WGPUShaderStage_Vertex | WGPUShaderStage_Fragment;
    entries[7].buffer.type = WGPUBufferBindingType_ReadOnlyStorage;
    entries[7].buffer.minBindingSize = sizeof(InstanceDataWire) * MAX_INSTANCES_WIRE;

    WGPUBindGroupLayoutDescriptor layout_desc = {};
    layout_desc.entryCount = 8;
    layout_desc.entries = entries;
    WGPUBindGroupLayout layout = wgpuDeviceCreateBindGroupLayout(vk.device, &layout_desc);

    return layout;
}
WGPUBindGroup CreateBindGroups(WGPUBindGroupLayout layout)
{
    WGPUBindGroupEntry entries[8] = {};
    //gubo buffer
    entries[0].binding = 0;
    entries[0].buffer = vk.gubo_buffer.buffer;
    entries[0].size = sizeof(GUBO);
    entries[0].offset = 0;
    //instances buffer
    entries[1].binding = 1;
    entries[1].buffer = vk.instance_buffer.buffer;
    entries[1].size = WGPU_WHOLE_SIZE;
    entries[1].offset = 0;
    //texture array
    WGPUTextureViewDescriptor texture_array_view = {};
    texture_array_view.format = WGPUTextureFormat_RGBA8UnormSrgb;
    texture_array_view.dimension = WGPUTextureViewDimension_2DArray;
    texture_array_view.baseMipLevel = 0;
    texture_array_view.mipLevelCount = 1;
    texture_array_view.baseArrayLayer = 0;
    texture_array_view.arrayLayerCount = MAX_TEXTURES;
    texture_array_view.aspect = WGPUTextureAspect_All;
    vk.tex_array.view = wgpuTextureCreateView(vk.tex_array.tex, &texture_array_view);
    entries[2].binding = 2;
    entries[2].textureView = vk.tex_array.view;
    //sampler
    WGPUSamplerDescriptor sampler_desc = {};
    sampler_desc.addressModeU = WGPUAddressMode_Repeat;
    sampler_desc.addressModeV = WGPUAddressMode_Repeat;
    sampler_desc.addressModeW = WGPUAddressMode_Repeat;
    sampler_desc.magFilter = WGPUFilterMode_Nearest;
    sampler_desc.minFilter = WGPUFilterMode_Nearest;
    sampler_desc.mipmapFilter = WGPUMipmapFilterMode_Nearest;
    sampler_desc.lodMinClamp = 0.0f;
    sampler_desc.lodMaxClamp = 32.0f;
    sampler_desc.maxAnisotropy = 1;
    vk.linear_sampler = wgpuDeviceCreateSampler(vk.device, &sampler_desc);
    entries[3].binding = 3;
    entries[3].sampler = vk.linear_sampler;
    //map buffer
    entries[4].binding = 4;
    entries[4].buffer = vk.map_buffer.buffer;
    entries[4].size = WGPU_WHOLE_SIZE;
    entries[4].offset = 0;
    //bones buffer
    entries[5].binding = 5;
    entries[5].buffer = vk.bones_buffer.buffer;
    entries[5].size = WGPU_WHOLE_SIZE;
    entries[5].offset = 0;
    //instances ui buffer
    entries[6].binding = 6;
    entries[6].buffer = vk.instance_ui_buffer.buffer;
    entries[6].size = WGPU_WHOLE_SIZE;
    entries[6].offset = 0;
    //instances buffer
    entries[7].binding = 7;
    entries[7].buffer = vk.instance_wire_buffer.buffer;
    entries[7].size = WGPU_WHOLE_SIZE;
    entries[7].offset = 0;

    WGPUBindGroupDescriptor group_desc = {};
    group_desc.layout = layout;
    group_desc.entryCount = 8;
    group_desc.entries = entries;

    WGPUBindGroup bind_groups = wgpuDeviceCreateBindGroup(vk.device, &group_desc);

    return bind_groups;
}
WGPUBuffer CreateBuffer(WGPUBufferUsage flags, uint64_t size)
{
    WGPUBufferDescriptor desc = {};
    desc.nextInChain = nullptr;
    desc.label = WGPUStringView{ "GPU buffer.", WGPU_STRLEN };
    desc.usage = flags;
    desc.size = size;
    desc.mappedAtCreation = false;
    WGPUBuffer buffer = wgpuDeviceCreateBuffer(vk.device, &desc);

    return buffer;
}
WGPUBuffer CreateMappedBuffer(WGPUBufferUsage flags, uint64_t size, const void* initialData)
{
    WGPUBufferDescriptor desc = {};
    desc.nextInChain = nullptr;
    desc.label = WGPUStringView{ "GPU buffer.", WGPU_STRLEN };
    desc.usage = flags;
    desc.size = size;
    desc.mappedAtCreation = true;
    WGPUBuffer buffer = wgpuDeviceCreateBuffer(vk.device, &desc);

    void* mapped = wgpuBufferGetMappedRange(buffer, 0, size);
    memcpy(mapped, initialData, size);
    wgpuBufferUnmap(buffer);

    return buffer;
}
void CopyBuffer(WGPUBuffer source, WGPUBuffer dest, uint32_t size)
{
    WGPUCommandEncoder encoder = BeginCMDbuffer();

    wgpuCommandEncoderCopyBufferToBuffer(encoder, source, 0, dest, 0, size);

    WGPUCommandBuffer command = EndCMDBuffer(encoder);
    SubmitCMD(command);
}
void InitializeBuffers()
{
    //vertex 
    vk.vertex_buffer.size = sizeof(Vertex) * vk.vertex_data.size();
    vk.vertex_buffer.buffer = CreateBuffer(WGPUBufferUsage_CopyDst | WGPUBufferUsage_Vertex, vk.vertex_buffer.size);
    wgpuQueueWriteBuffer(vk.queue, vk.vertex_buffer.buffer, 0, vk.vertex_data.data(), vk.vertex_buffer.size);

    //index 
    vk.index_buffer.size = sizeof(uint32_t) * vk.index_data.size();
    vk.index_buffer.buffer = CreateBuffer(WGPUBufferUsage_Index | WGPUBufferUsage_CopyDst, vk.index_buffer.size);
    wgpuQueueWriteBuffer(vk.queue, vk.index_buffer.buffer, 0, vk.index_data.data(), vk.index_buffer.size);

    //gubo 
    vk.gubo_buffer.size = sizeof(GUBO);
    vk.gubo_buffer.buffer = CreateBuffer(WGPUBufferUsage_Uniform | WGPUBufferUsage_CopyDst, vk.gubo_buffer.size);
    wgpuQueueWriteBuffer(vk.queue, vk.gubo_buffer.buffer, 0, &gubo, vk.gubo_buffer.size);

    //instance 
    vk.instance_buffer.size = sizeof(InstanceData) * MAX_INSTANCES;
    vk.instance_buffer.buffer = CreateBuffer(WGPUBufferUsage_Storage | WGPUBufferUsage_CopyDst, vk.instance_buffer.size);
    wgpuQueueWriteBuffer(vk.queue, vk.instance_buffer.buffer, 0, vk.instance_data, vk.instance_buffer.size);

    //indirect 
    vk.indirect_buffer.size = sizeof(DrawIndexedIndirectCommand) * MAX_INDIRECT_CMDS;
    vk.indirect_buffer.buffer = CreateBuffer(WGPUBufferUsage_Indirect | WGPUBufferUsage_CopyDst, vk.indirect_buffer.size);
    wgpuQueueWriteBuffer(vk.queue, vk.indirect_buffer.buffer, 0, vk.indirect_cmds, vk.indirect_buffer.size);

    //map 
    vk.map_buffer.size = sizeof(vk.map_data);
    vk.map_buffer.buffer = CreateBuffer(WGPUBufferUsage_Storage | WGPUBufferUsage_CopyDst, vk.map_buffer.size);
    wgpuQueueWriteBuffer(vk.queue, vk.map_buffer.buffer, 0, vk.map_data, vk.map_buffer.size);

    //bones 
    vk.bones_buffer.size = sizeof(mat4) * MAX_BONES;
    vk.bones_buffer.buffer = CreateBuffer(WGPUBufferUsage_Storage | WGPUBufferUsage_CopyDst, vk.bones_buffer.size);

    //instance ui
    vk.instance_ui_buffer.size = sizeof(InstanceDataUI) * MAX_INSTANCES_UI;
    vk.instance_ui_buffer.buffer = CreateBuffer(WGPUBufferUsage_Storage | WGPUBufferUsage_CopyDst, vk.instance_ui_buffer.size);
    wgpuQueueWriteBuffer(vk.queue, vk.instance_ui_buffer.buffer, 0, vk.instance_data_ui, vk.instance_ui_buffer.size);

    //instance bb
    vk.instance_wire_buffer.size = sizeof(InstanceDataWire) * MAX_INSTANCES_WIRE;
    vk.instance_wire_buffer.buffer = CreateBuffer(WGPUBufferUsage_Storage | WGPUBufferUsage_CopyDst, vk.instance_wire_buffer.size);
    wgpuQueueWriteBuffer(vk.queue, vk.instance_wire_buffer.buffer, 0, vk.instance_data_wire, vk.instance_wire_buffer.size);
}
void RecordIndirectCmds(ModelMeshes model)
{
    for (uint32_t i = 0; i < model.submesh_count; ++i)
    {
        uint32_t index = model.submesh_index + i;

        DrawIndexedIndirectCommand cmd{};
        cmd.index_count = vk.meshes_alloc[index].index_count;
        cmd.instance_count = model.active_instances;
        cmd.first_index = vk.meshes_alloc[index].first_index;
        cmd.vertex_offset = vk.meshes_alloc[index].vertex_offset;
        cmd.first_instance = model.base_instance;

        vk.indirect_cmds[vk.active_commands++] = cmd;
    }
}
ModelMeshes LoadModel(const char* path)
{
    ModelMeshes my_model = {};

    ufbx_load_opts opts =
    {
        .target_axes = ufbx_axes_right_handed_z_up,
        .target_unit_meters = 1.0f,
    };

    ufbx_error error;
    ufbx_scene* scene = ufbx_load_file(path, &opts, &error);
    if (!scene)
    {
        fprintf(stderr, "failed to load scene: %s\n", error.description.data);
        return my_model;
    }

    my_model.submesh_index = static_cast<uint32_t>(vk.meshes_alloc.size());

    for (uint32_t n = 0; n < scene->nodes.count; n++)
    {
        ufbx_node* node = scene->nodes.data[n];
        if (!node->mesh) continue;

        if (node->mesh->skin_deformers.count > 0)
        {
            my_model.mesh = node->mesh;
        }

        printf("submesh %d: %s ", my_model.submesh_index, node->name.data);
        ufbx_mesh* mesh = node->mesh;

        ufbx_skin_deformer* skin = (node->mesh->skin_deformers.count > 0) ? node->mesh->skin_deformers.data[0] : nullptr;
        for (uint32_t s = 0; s < mesh->material_parts.count; s++)
        {
            ufbx_mesh_part submesh = mesh->material_parts.data[s];

            Submesh my_submesh;
            my_submesh.first_index = vk.index_data.size();
            my_submesh.vertex_offset = vk.vertex_data.size();

            size_t num_tri_indices = mesh->max_face_triangles * 3;
            uint32_t* tri_indices = (uint32_t*)calloc(num_tri_indices, sizeof(uint32_t));

            for (size_t f = 0; f < submesh.num_faces; f++)
            {
                ufbx_face face = mesh->faces.data[submesh.face_indices.data[f]];

                uint32_t num_tris = ufbx_triangulate_face(tri_indices, num_tri_indices, mesh, face);

                for (size_t i = 0; i < num_tris * 3; i++)
                {
                    uint32_t index = tri_indices[i];
                    Vertex my_vertex = {};

                    for (int w = 0; w < 4; w++)
                    {
                        my_vertex.weights[w] = 0.0f;
                        my_vertex.joints[w] = 0;
                    }

                    if (skin)
                    {
                        uint32_t cp_index = mesh->vertex_indices.data[index];

                        if (cp_index < skin->vertices.count)
                        {
                            ufbx_skin_vertex skin_vert = skin->vertices.data[cp_index];
                            size_t num_weights = skin_vert.num_weights;
                            if (num_weights > 4) num_weights = 4;

                            float total_weight = 0.0f;
                            for (size_t i = 0; i < num_weights; i++)
                            {
                                ufbx_skin_weight skin_weight = skin->weights.data[skin_vert.weight_begin + i];
                                my_vertex.joints[i] = skin_weight.cluster_index;
                                my_vertex.weights[i] = (float)skin_weight.weight;
                                total_weight += (float)skin_weight.weight;
                            }

                            if (total_weight > 0.0f)
                            {
                                for (size_t w = 0; w < 4; w++)
                                {
                                    my_vertex.weights[w] /= total_weight;
                                }
                            }
                        }
                    }
                    ufbx_vec3 pos = ufbx_get_vertex_vec3(&mesh->vertex_position, index);
                    ufbx_vec2 uv = ufbx_get_vertex_vec2(&mesh->vertex_uv, index);
                    ufbx_vec3 normal = ufbx_get_vertex_vec3(&mesh->vertex_normal, index);

                    my_vertex.pos.x = (float)pos.x;
                    my_vertex.pos.y = (float)pos.y;
                    my_vertex.pos.z = (float)pos.z;
                    my_vertex.uv.x = (float)uv.x;
                    my_vertex.uv.y = 1.0 - (float)uv.y;
                    my_vertex.normals.x = (float)normal.x;
                    my_vertex.normals.y = (float)normal.y;
                    my_vertex.normals.z = (float)normal.z;

                    vk.vertex_data.push_back(my_vertex);
                    uint32_t relative_index = static_cast<uint32_t>(vk.vertex_data.size() - 1 - my_submesh.vertex_offset);
                    vk.index_data.push_back(relative_index);
                }
            }
            free(tri_indices);
            my_submesh.index_count = static_cast<uint32_t>(vk.index_data.size() - my_submesh.first_index);
            vk.meshes_alloc.push_back(my_submesh);
        }
        printf("\n");
    }
    my_model.submesh_count = static_cast<uint32_t>(vk.meshes_alloc.size()) - my_model.submesh_index;

    if (scene->anim_stacks.count > 0)
    {
        my_model.scene = scene;
        my_model.animation[0] = scene->anim_stacks.data[0]->anim;
        my_model.animation[1] = scene->anim_stacks.data[2]->anim;
        my_model.animation[2] = scene->anim_stacks.data[4]->anim;
        my_model.animation[3] = scene->anim_stacks.data[6]->anim;
        my_model.animation[4] = scene->anim_stacks.data[7]->anim;
    }

    return my_model;
}
void InitModel(const char* path, uint32_t max_instances, uint32_t offset)
{
    ModelMeshes model = LoadModel(path);
    model.base_instance = offset;
    model.active_instances = 1;
    model.max_instances = max_instances;

    RecordIndirectCmds(model);

    vk.total_submesh_count += model.submesh_count;
    vk.model_meshes.push_back(model);
}
void InitModels()
{
    uint32_t offset = 0;
    InitModel("assets/plane.fbx", 1, offset);
    offset += vk.model_meshes[0].max_instances;
    InitModel("assets/tree.fbx", 100, offset);
    offset += vk.model_meshes[1].max_instances;
    InitModel("assets/player.fbx", 2, offset);
    offset += vk.model_meshes[2].max_instances;
    InitModel("assets/minion.fbx", 1, offset);
}
void GetBoneMatrices(Entity* player)
{
    vk.bones_data.clear();
    ModelMeshes* mesh = player->model_mesh;
    ufbx_evaluate_opts opts = { 0 };

    ufbx_scene* eval = ufbx_evaluate_scene(mesh->scene, mesh->animation[player->current_anim_id], player->anim_time, &opts, NULL);

    if (mesh->mesh && mesh->mesh->skin_deformers.count > 0)
    {
        ufbx_skin_deformer* skin = mesh->mesh->skin_deformers.data[0];

        for (size_t b = 0; b < skin->clusters.count; ++b)
        {
            ufbx_skin_cluster* cluster = skin->clusters.data[b];
            ufbx_matrix bone_world = eval->nodes.data[cluster->bone_node->typed_id]->node_to_world;
            ufbx_matrix skin_mat = ufbx_matrix_mul(&bone_world, &cluster->geometry_to_bone);

            mat4 bones = {};

            bones.column[0] = { (float)skin_mat.cols[0].x, (float)skin_mat.cols[0].y, (float)skin_mat.cols[0].z, 0.0f };
            bones.column[1] = { (float)skin_mat.cols[1].x, (float)skin_mat.cols[1].y, (float)skin_mat.cols[1].z, 0.0f };
            bones.column[2] = { (float)skin_mat.cols[2].x, (float)skin_mat.cols[2].y, (float)skin_mat.cols[2].z, 0.0f };
            bones.column[3] = { (float)skin_mat.cols[3].x, (float)skin_mat.cols[3].y, (float)skin_mat.cols[3].z, 1.0f };

            vk.bones_data.push_back(bones);
        }
    }

    uint64_t gpu_byte_offset = (uint64_t)player->instance_data.bone_offset * sizeof(mat4);
    const void* cpu_data_pointer = vk.bones_data.data();
    uint32_t upload_size = sizeof(mat4) * vk.bones_data.size();

    wgpuQueueWriteBuffer(vk.queue, vk.bones_buffer.buffer, gpu_byte_offset, cpu_data_pointer, upload_size);

    ufbx_free_scene(eval);
}

void UpdatePositions()
{
    if ((p1.moving) && (v2_distance({ p1.position.x, p1.position.y },
        { p1.destination.x, p1.destination.y }) > 0.1f))
    {
        p1.position += p1.movement;
    }
    else { p1.moving = false; }

    if (!p1.autos.moving)
    {
        p1.autos.position = p1.position;
    }
    else
    {
        p1.autos.destination = p2.position;
        p1.autos.movement.x = p1.autos.destination.x - p1.autos.position.x;
        p1.autos.movement.y = p1.autos.destination.y - p1.autos.position.y;
        p1.autos.movement = v3_normalize(p1.autos.movement);
        p1.autos.movement *= p1.autos.speed;
        p1.autos.position += p1.autos.movement;

        if (v2_distance({ p1.autos.position.x, p1.autos.position.y }, { p2.position.x, p2.position.y }) <= 1.0f)
        {
            p1.autos.moving = false;
            p1.autos.position = p1.position;
            if (p2.hp != 0.0f)
            {
                p2.hp -= 5.0f;
            }
        }
    }

    if ((input.mouse_pos.x < 30) && (cam.pos.x > 5.0))
    {
        cam.pos.x -= cam.speed;
        cam.look_at.x -= cam.speed;
    }
    else if ((input.mouse_pos.x > win.extent_x - 30.0f) && (cam.pos.x < 45.0f))
    {
        cam.pos.x += cam.speed;
        cam.look_at.x += cam.speed;
    }
    if ((input.mouse_pos.y < 30) && (cam.pos.y < 30.0f))
    {
        cam.pos.y += cam.speed;
        cam.look_at.y += cam.speed;
    }
    else if ((input.mouse_pos.y > win.extent_y - 30) && (cam.pos.y > -10.0f))
    {
        cam.pos.y -= cam.speed;
        cam.look_at.y -= cam.speed;
    }

    if (input.spacebar.ended_down)
    {
        cam.pos = { p1.position.x, p1.position.y - 15.0f, 35.0f };
        cam.look_at = { p1.position.x, p1.position.y + 1.0f, 1.0f };
    }

    input.ray.position = { cam.pos };
}
void UpdateEnemy()
{
    p2.destination = p1.position;
    p2.movement.x = p2.destination.x - p2.position.x;
    p2.movement.y = p2.destination.y - p2.position.y;
    p2.movement = v3_normalize(p2.movement);
    p2.movement *= p2.speed;

    p2.angle = (atan2f(p2.movement.y, p2.movement.x)) + 1.5708;

    if (p2.can_move)
    {
        if ((v2_distance({ p2.position.x, p2.position.y }, { p2.destination.x, p2.destination.y }) >= 10.0f))
        {
            p2.position += p2.movement;
            p2.moving = true;
            p2.in_range = false;
        }
        else
        {
            p2.moving = false;
            p2.in_range = true;
            p2.can_move = false;
        }
    }
    else if ((v2_distance({ p2.position.x, p2.position.y }, { p2.destination.x, p2.destination.y }) >= 12.0f))
    {
        p2.can_move = true;
    }

    if (!p2.autos.moving)
    {
        p2.autos.position = p2.position;
    }
    else
    {
        p2.autos.destination = p1.position;
        p2.autos.movement.x = p2.autos.destination.x - p2.autos.position.x;
        p2.autos.movement.y = p2.autos.destination.y - p2.autos.position.y;
        p2.autos.movement = v3_normalize(p2.autos.movement);
        p2.autos.movement *= p2.autos.speed;
        p2.autos.position += p2.autos.movement;

        if (v2_distance({ p2.autos.position.x, p2.autos.position.y }, { p1.position.x, p1.position.y }) <= 1.0f)
        {
            p2.autos.moving = false;
            p2.autos.position = p2.position;
            if (p1.hp != 0.0f)
            {
                p1.hp -= 10.0f;
            }
        }
    }
}
void Render()
{
    //texture view
    WGPUSurfaceTexture surface_tex = {};
    WGPUTextureView texture_view = AcquireNextFrameView(&surface_tex);

    if (texture_view == nullptr)
    {
        std::cerr << "failed to acquire surface texture." << std::endl;
        wgpuTextureViewRelease(texture_view);
        return;
    }

    //cmd encoder
    WGPUCommandEncoder encoder = BeginCMDbuffer();

    //render pass
    WGPURenderPassColorAttachment color_attachment = {};
    color_attachment.view = texture_view;
    color_attachment.resolveTarget = nullptr;
    color_attachment.loadOp = WGPULoadOp_Clear;
    color_attachment.storeOp = WGPUStoreOp_Store;
    color_attachment.clearValue = WGPUColor{ 0.0, 0.0, 0.0, 1.0 };
    color_attachment.depthSlice = WGPU_DEPTH_SLICE_UNDEFINED;

    WGPURenderPassDepthStencilAttachment depth_attachment = {};
    depth_attachment.view = vk.depth_image.view;
    depth_attachment.depthClearValue = 1.0f;
    depth_attachment.depthStoreOp = WGPUStoreOp_Store;
    depth_attachment.depthLoadOp = WGPULoadOp_Clear;

    depth_attachment.depthStoreOp = WGPUStoreOp_Store;
    WGPURenderPassDescriptor render_desc = {};
    render_desc.colorAttachmentCount = 1;
    render_desc.colorAttachments = &color_attachment;
    render_desc.depthStencilAttachment = &depth_attachment;
    render_desc.timestampWrites = nullptr;

    WGPURenderPassEncoder render_pass = wgpuCommandEncoderBeginRenderPass(encoder, &render_desc);

    if (vk.active_commands > 0)
    {
        wgpuRenderPassEncoderSetIndexBuffer(render_pass, vk.index_buffer.buffer, WGPUIndexFormat_Uint32, 0, WGPU_WHOLE_SIZE);
        wgpuRenderPassEncoderSetVertexBuffer(render_pass, 0, vk.vertex_buffer.buffer, 0, WGPU_WHOLE_SIZE);
        wgpuRenderPassEncoderSetBindGroup(render_pass, 0, vk.global_bg, 0, nullptr);

        wgpuRenderPassEncoderSetPipeline(render_pass, vk.plane_pipeline);
        wgpuRenderPassEncoderDrawIndexedIndirect(render_pass, vk.indirect_buffer.buffer, 0);

        if (vk.hovering) //currently only filling the first submesh in the hovered object, probably fine
        {
            wgpuRenderPassEncoderSetPipeline(render_pass, vk.outline_pipeline);
            wgpuRenderPassEncoderDrawIndexedIndirect(render_pass, vk.indirect_buffer.buffer, (vk.active_commands * sizeof(DrawIndexedIndirectCommand)));
        }

        wgpuRenderPassEncoderSetPipeline(render_pass, vk.object_pipeline);
        for (uint32_t i = 1; i < vk.active_commands; i++)
        {
            wgpuRenderPassEncoderDrawIndexedIndirect(render_pass, vk.indirect_buffer.buffer, (i * sizeof(DrawIndexedIndirectCommand)));
        }
    }

    wgpuRenderPassEncoderSetPipeline(render_pass, vk.wire_pipeline);
    wgpuRenderPassEncoderDraw(render_pass, 36, 2, 0, 0);

    if (vk.active_ui > 0)
    {
        wgpuRenderPassEncoderSetPipeline(render_pass, vk.ui_pipeline);
        wgpuRenderPassEncoderDraw(render_pass, 6, vk.active_ui, 0, 0);
    }

    wgpuRenderPassEncoderEnd(render_pass);
    wgpuRenderPassEncoderRelease(render_pass);

    WGPUCommandBuffer command = EndCMDBuffer(encoder);
    SubmitCMD(command);

#ifndef __EMSCRIPTEN__
    wgpuSurfacePresent(vk.surface);
#endif

    wgpuTextureViewRelease(texture_view);
    wgpuTextureRelease(surface_tex.texture);
}

void InitWorld()
{
    cam.model = mat4_diagonal(1.0f);
    cam.proj = mat4_perspective(DegToRad(40.0f), (float)win.extent_x / (float)win.extent_y, 1.0f);
    cam.pos = { 15.0f, 0.0f, 35.0f };
    cam.look_at = { 15.0f, 16.0f, 1.0f };
    cam.speed = 100.0f * vk.physics_dt;
    world_box.min = { 0.0f, 0.0f, 0.0f };
    world_box.max = { 50.0f, 50.0f, 0.0f };

    p1 = {};
    p1.position = { 15.0f, 15.0f, 0.0f };
    p1.speed = 5.0f * vk.physics_dt;
    p1.moving = false;
    p1.model_mesh = &vk.model_meshes[2];
    p1.instance_data.bone_offset = 0;
    p1.current_anim_id = 1;
    p1.autos = {};
    p1.autos.origin = p1.position;
    p1.autos.position = p1.position;;
    p1.autos.speed = 30.0f * vk.physics_dt;

    p2 = {};
    p2.position = { 35.0f, 35.0f, 0.0f };
    p2.speed = 4.5f * vk.physics_dt;
    p2.moving = false;
    p2.model_mesh = &vk.model_meshes[2];
    p2.instance_data.bone_offset = 33;
    p2.current_anim_id = 1;
    p2.autos = {};
    p2.autos.origin = p2.position;
    p2.autos.position = p2.position;
    p2.autos.speed = 30.0f * vk.physics_dt;

    gubo = {};
    gubo.window = { (float)win.extent_x, (float)win.extent_y };

    input.mouse_pos.x = WIDTH / 2.0f;
    input.mouse_pos.y = HEIGHT / 2.0f;
}
void InitVk()
{
    //window 
    if (!glfwInit())
    {
        std::cerr << "could not initialize GLFW." << std::endl;
        return;
    }

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

    win.handle = glfwCreateWindow(WIDTH, HEIGHT, "Earthworm Applications", nullptr, nullptr);
    if (win.handle == nullptr)
    {
        std::cerr << "could not open window." << std::endl;
        return;
    }
    glfwGetFramebufferSize(win.handle, &win.extent_x, &win.extent_y);
    win.x = WIDTH;
    win.y = HEIGHT;

    glfwSetKeyCallback(win.handle, KeyCallback);
    glfwSetMouseButtonCallback(win.handle, MouseButtonCallback);
    glfwSetCursorPosCallback(win.handle, CursorPosCallback);
    glfwSetWindowFocusCallback(win.handle, FocusCallback);

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
#ifdef __EMSCRIPTEN__
    WGPUEmscriptenSurfaceSourceCanvasHTMLSelector canvas_desc = {};
    canvas_desc.chain.sType = WGPUSType_EmscriptenSurfaceSourceCanvasHTMLSelector;
    canvas_desc.selector = WGPUStringView{ "#canvas", WGPU_STRLEN };
    WGPUSurfaceDescriptor surface_desc = {};
    surface_desc.nextInChain = &canvas_desc.chain;
    vk.surface = wgpuInstanceCreateSurface(vk.instance, &surface_desc);
#else
    WGPUSurfaceSourceWindowsHWND hwnd_desc = {};
    hwnd_desc.chain.sType = WGPUSType_SurfaceSourceWindowsHWND;
    hwnd_desc.hinstance = GetModuleHandle(NULL);
    hwnd_desc.hwnd = glfwGetWin32Window(win.handle);

    WGPUSurfaceDescriptor surface_desc = {};
    surface_desc.nextInChain = (WGPUChainedStruct*)&hwnd_desc; //maybe const WGPUChainedStruct* for web Idk
    vk.surface = wgpuInstanceCreateSurface(vk.instance, &surface_desc);
#endif

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
#ifdef __EMSCRIPTEN__
    while (!vk.adapter_request)
    {
        emscripten_sleep(10);
    }
#endif  
    if (vk.adapter == nullptr)
    {
        std::cerr << "could not request adapter." << std::endl;
        return;
    }
    std::cout << "adapter: ";

    /*
    WGPUAdapterInfo  adapter_info = {};
    if (wgpuAdapterGetInfo(vk.adapter, &adapter_info) == WGPUStatus_Success)
    {
        PrintString(adapter_info.vendor);
        std::cout << ", ";
        PrintString(adapter_info.architecture);
        std::cout << " ";
        wgpuAdapterInfoFreeMembers(adapter_info);
    }
    */
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
#ifdef __EMSCRIPTEN__   
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
    depthDesc.size = { (uint32_t)win.extent_x, (uint32_t)win.extent_y, 1 };
    depthDesc.format = WGPUTextureFormat_Depth32Float;
    depthDesc.mipLevelCount = 1;
    depthDesc.sampleCount = 1;
    vk.depth_image.tex = wgpuDeviceCreateTexture(vk.device, &depthDesc);
    vk.depth_image.view = wgpuTextureCreateView(vk.depth_image.tex, nullptr);

    //queue
    WGPUQueueWorkDoneCallbackInfo queue_callback_info = {};
    queue_callback_info.callback = QueueCallback;
    queue_callback_info.mode = WGPUCallbackMode_AllowSpontaneous;
    vk.queue = wgpuDeviceGetQueue(vk.device);

    ConfigSurface();

    //other stuff
    InitModels();
    vk.tex_array.tex = CreateTextureArray(512);
    LoadTextures();

    WGPUBindGroupLayout bg_layout = CreateBindGroupsLayout();
    WGPUPipelineLayoutDescriptor bg_layout_desc = {};
    bg_layout_desc.bindGroupLayoutCount = 1;
    bg_layout_desc.bindGroupLayouts = &bg_layout;
    vk.global_pipeline_layout = wgpuDeviceCreatePipelineLayout(vk.device, &bg_layout_desc);

    PipelineInfo op_info = CreatePipelineInfo(1, 1, WGPUOptionalBool_True, WGPUPrimitiveTopology_TriangleList,
        WGPUCompareFunction_Less, WGPUCullMode_None);

    PipelineInfo wire_info = CreatePipelineInfo(1, 0, WGPUOptionalBool_True, WGPUPrimitiveTopology_TriangleList,
        WGPUCompareFunction_Less, WGPUCullMode_None);

    PipelineInfo outline_info = CreatePipelineInfo(0, 1, WGPUOptionalBool_False, WGPUPrimitiveTopology_TriangleList,
        WGPUCompareFunction_LessEqual, WGPUCullMode_Front);

    PipelineInfo ui_info = CreatePipelineInfo(0, 0, WGPUOptionalBool_False, WGPUPrimitiveTopology_TriangleList,
        WGPUCompareFunction_Less, WGPUCullMode_None);

    vk.object_pipeline = CreatePipeline(object_shader, op_info);
    vk.plane_pipeline = CreatePipeline(plane_shader, op_info);
    vk.wire_pipeline = CreatePipeline(wire_shader, wire_info);
    vk.outline_pipeline = CreatePipeline(outline_shader, outline_info);
    vk.ui_pipeline = CreatePipeline(ui_shader, ui_info);

    InitializeBuffers();

    vk.global_bg = CreateBindGroups(bg_layout);

    UpdateStatic();

#ifdef __EMSCRIPTEN__
    JS_InitAudio();
    JS_LoadSound("fireball.mp3", "click");
    SetCursor("cursor.png");
#endif 
}
void UpdateGame(float delta)
{
    if (delta > 0.1f) delta = 0.1f;

    vk.accumulator += delta;

    while (vk.accumulator >= vk.physics_dt)
    {
        UpdatePositions();
        UpdateEnemy();
        vk.accumulator -= vk.physics_dt;
    }

    if (p1.moving)
    {
        SetEntityAnimation(&p1, 4, true);
    }
    else if (p1.current_anim_id != 4)
    {
        SetEntityAnimation(&p1, p1.current_anim_id, true);
    }
    else
    {
        SetEntityAnimation(&p1, 1, true);
    }

    if (p2.moving)
    {
        SetEntityAnimation(&p2, 4, true);
    }
    if (p2.in_range)
    {
        SetEntityAnimation(&p2, 3, true);
    }

    UpdateEntityAnimation(&p1, delta);
    UpdateEntityAnimation(&p2, delta);

    GetBoneMatrices(&p1);
    GetBoneMatrices(&p2);

    CheckAnimationEvents(&p1, delta);
    CheckAnimationEvents(&p2, delta);

    input.ray.direction = RayCast(input.mouse_pos.x, input.mouse_pos.y,
        cam.view, cam.proj, (float)win.extent_x, (float)win.extent_y);
    input.collision = GetRayCollisionBox(input.ray, p2.bb);
    if (!input.collision.hit)
    {
        ModelMeshes model = vk.model_meshes[2];
        if (vk.indirect_cmds[vk.active_commands].index_count != 0)
        {
            for (uint32_t i = 0; i < model.submesh_count; ++i)
            {
                DrawIndexedIndirectCommand cmd{};
                vk.indirect_cmds[vk.active_commands + i] = cmd;
            }

            uint64_t gpu_byte_offset = vk.active_commands * sizeof(DrawIndexedIndirectCommand);
            size_t instance_upload_size = model.submesh_count * sizeof(DrawIndexedIndirectCommand);
            wgpuQueueWriteBuffer(vk.queue, vk.indirect_buffer.buffer, gpu_byte_offset, &vk.indirect_cmds[vk.active_commands], instance_upload_size);

#ifdef __EMSCRIPTEN__
            SetCursor("cursor.png");
#endif
        }
    }
    else
    {
        vk.hovering = true;

        if (vk.indirect_cmds[vk.active_commands].index_count == 0)
        {
            ModelMeshes model = vk.model_meshes[2];
            for (uint32_t i = 0; i < model.submesh_count; ++i)
            {
                uint32_t index = model.submesh_index + i;

                DrawIndexedIndirectCommand cmd{};
                cmd.index_count = vk.meshes_alloc[index].index_count;
                cmd.instance_count = 1;
                cmd.first_index = vk.meshes_alloc[index].first_index;
                cmd.vertex_offset = vk.meshes_alloc[index].vertex_offset;
                cmd.first_instance = model.base_instance + 1;

                vk.indirect_cmds[vk.active_commands + i] = cmd;
            }

            uint64_t gpu_byte_offset = vk.active_commands * sizeof(DrawIndexedIndirectCommand);
            size_t instance_upload_size = model.submesh_count * sizeof(DrawIndexedIndirectCommand);
            wgpuQueueWriteBuffer(vk.queue, vk.indirect_buffer.buffer, gpu_byte_offset, &vk.indirect_cmds[vk.active_commands], instance_upload_size);

#ifdef __EMSCRIPTEN__
            SetCursor("cursor_red.png");
#endif
        }
    }

    UpdateGPU();
    Render();

    double work_end = glfwGetTime();
    double true_cpu_ms = (work_end - vk.last_time) * 1000.0;

    static double print_timer = 0.0;
    print_timer += delta;
    if (print_timer >= 1.0)
    {
        printf("%.2f ms\n", true_cpu_ms);
        print_timer -= 1.0;
    }
}