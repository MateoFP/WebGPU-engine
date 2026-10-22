#include "include/renderer.h"
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#include <webgpu/webgpu_cpp.h>
#endif

bool has_init = false;
double timer = 0.0;
bool connect_initiated = false;

void MainLoop()
{
#ifdef __EMSCRIPTEN__
    double current_time = glfwGetTime();
    vk.frame_delta = current_time - vk.last_time;
    vk.last_time = current_time;
    if (vk.frame_delta > 0.033) vk.frame_delta = 0.033;
    gubo.time += vk.frame_delta;

    if (!web.Connected)
    {
        if (!connect_initiated)
        {
            connect_initiated = true;
            JS_ConnectToServer();
        }
    }
    else 
    {
        timer += vk.frame_delta;
        if (timer >= 2.0)
        {
            JS_Ping(current_time);
            timer = 0.0;
        }
    }

    if (!has_init && web.Connected)
    {
        InitVk();
        printf("engine initialized.\n");
        InitWorld();
        printf("world initialized.\n");
        cam.view = mat4_look_at(cam.pos, cam.look_at, { 0.0f, 0.0f, 1.0f });
        gubo.projView = mat4_multiply(cam.proj, cam.view);
        wgpuQueueWriteBuffer(vk.queue, vk.gubo_buffer.buffer, 0, &gubo, sizeof(gubo));
        Render();
        has_init = true;
    }

    if (!gc.paused && has_init)
    {
        glfwPollEvents();
        UpdateGame(vk.frame_delta);
    }
#else

    double current_time = glfwGetTime();
    vk.frame_delta = current_time - vk.last_time;
    vk.last_time = current_time;
    gubo.time += vk.frame_delta;
    glfwPollEvents();
    UpdateGame(vk.frame_delta);
#endif
}

int main()
{
    vk.last_time = glfwGetTime();
#ifdef __EMSCRIPTEN__
    JS_InitPage();
    FocusCanvas();
    emscripten_set_main_loop(MainLoop, 0, 1);
#else
    InitVk();
    printf("engine initialized.\n");
    InitWorld();
    printf("world initialized.\n");
    UpdateGame(vk.physics_dt);

    while (!glfwWindowShouldClose(win.handle))
    {
        MainLoop();
    }
#endif

    return 0;
}