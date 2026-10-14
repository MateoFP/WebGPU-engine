#include "include/renderer.h"
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#include <webgpu/webgpu_cpp.h>
#endif

void MainLoop()
{
#ifdef __EMSCRIPTEN__
    static double timer = 0.0;
    timer += vk.frame_delta;
    if ((timer >= 2.0) && (!web.Connected))
    {
        JS_ConnectToServer();
        timer -= 2.0;
    }

#endif
    if (!gc.paused)
    {
        double current_time = glfwGetTime();
        vk.frame_delta = current_time - vk.last_time;
        vk.last_time = current_time;
        gubo.time += vk.frame_delta;
        glfwPollEvents();
        UpdateGame(vk.frame_delta);
    }
}

int main()
{
    InitVk();
    printf("engine initialized.\n");
    InitWorld();
    printf("world initialized.\n");

    vk.last_time = glfwGetTime();
    UpdateGame(vk.physics_dt);
#ifdef __EMSCRIPTEN__
    FocusCanvas();
    emscripten_set_main_loop(MainLoop, 0, 1);
#else
    while (!glfwWindowShouldClose(win.handle))
    {
        MainLoop();
    }
#endif

    return 0;
}