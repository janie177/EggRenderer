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
    settings.vSync = true;

    auto renderer = std::make_unique<Renderer>();

    if(renderer->Init(settings))  //This is a bit flag, can do DebugPrintFlags::ERROR | DebugPrintFlags::WARNING etc.
    {
        std::vector<Vertex> vertices{ Vertex{glm::vec3{0.f, 0.f, 0.f}, glm::vec3{1.f, 0.f, 0.f}, glm::vec3{1.f, 1.f, 1.f}}};
        std::vector<uint32_t> indices{1, 2, 3, 4, 5, 6, 22, 1};
        auto mesh = renderer->CreateMesh(vertices, indices);

        static int frameId = 0;
        bool run = true;
        while(run)
        {
            run = renderer->Run();
            ++frameId;
            printf("Done rendering frame %i.\n", frameId);
        	if(frameId > 50)
        	{
                run = false;
        	}
        }
    }
    else
    {
        printf("Could not init renderer.\n");
    }

    printf("Done running renderer.\n");

    //Delete all allocated objects.
    if(renderer->CleanUp())
    {
        printf("Renderer succesfully cleaned up!\n");
    }
    else
    {
        printf("Could not clean up renderer properly!\n");
    }

    printf("Program execution finished.\nPress any key to continue.\n");
    getchar();
}