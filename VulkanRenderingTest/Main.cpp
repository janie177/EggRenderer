#include "Renderer.h"
#include <memory>

/*
 * Program entry point.
 * TODO: write everything
 */
int main()
{
    RendererSettings settings;
    settings.debugFlags = DebugPrintFlags::ERROR;

    auto renderer = std::make_unique<Renderer>();

    if(renderer->Init(settings))  //This is a bit flag, can do DebugPrintFlags::ERROR | DebugPrintFlags::WARNING etc.
    {
        renderer->Run();
    }
    else
    {
        printf("Could not init renderer.\n");
    }

    printf("Program execution finished.\nPress any key to continue.\n");
    getchar();
}