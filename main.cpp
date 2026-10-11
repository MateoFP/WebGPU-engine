#pragma once
#include "include/renderer.h"

void UpdateGame()
{
}
void InitWorld()
{
}

void MainLoop()
{
    glfwPollEvents();
    UpdateGame();
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