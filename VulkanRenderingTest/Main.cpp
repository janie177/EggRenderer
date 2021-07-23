#include "Renderer.h"
#include <memory>

/*
 * Program entry point.
 * TODO: write everything
 */
int main()
{
    //A cube for testing.
    std::vector<Vertex> vertices{
        Vertex{glm::vec3{-1.f, -1.f, 0.f}, glm::vec3{1.f, 0.f, 0.f}, glm::vec3{1.f, 1.f, 1.f}, glm::vec2{0.f, 0.f}},
        Vertex{glm::vec3{0.f, -1.f, 1.f}, glm::vec3{0.f, 1.f, 0.f}, glm::vec3{1.f, 1.f, 1.f}, glm::vec2{0.f, 0.f}},
        Vertex{glm::vec3{1.f, -1.f, 0.f}, glm::vec3{0.f, 0.f, 1.f}, glm::vec3{1.f, 1.f, 1.f}, glm::vec2{0.f, 0.f}},
        Vertex{glm::vec3{0.f, 1.f, 0.f}, glm::vec3{0.4f, 0.2f, 0.6f}, glm::vec3{1.f, 1.f, 1.f}, glm::vec2{0.f, 0.f}}
    };
    std::vector<uint32_t> indices{ 0, 3, 1, 1, 3, 2, 2, 3, 0, 0, 1, 2 };

    RendererSettings settings;
    settings.debugFlags = DebugPrintFlags::ERROR;
    settings.vSync = true;

    auto renderer = std::make_unique<Renderer>();
    Camera camera;
    camera.UpdateProjection(70.f, 0.1f, 600.f, static_cast<float>(settings.resolutionX) / static_cast<float>(settings.resolutionY));

    if(renderer->Init(settings))  //This is a bit flag, can do DebugPrintFlags::ERROR | DebugPrintFlags::WARNING etc.
    {
        auto mesh = renderer->CreateMesh(vertices, indices);

    	//Drawing information.
        DrawData drawData;
        DrawCall drawCall;
        MeshInstance meshInstance;
        drawCall.m_Mesh = mesh;
        drawCall.m_NumInstances = 1;
        drawCall.m_Transparent = false;
        drawCall.m_pMeshInstances = &meshInstance;
        drawData.m_NumDrawCalls = 1;
        drawData.m_pDrawCalls = &drawCall;
        drawData.m_Camera = &camera;
    	
        static int frameId = 0;
        bool run = true;
        while(run)
        {
            run = renderer->DrawFrame(drawData);
            ++frameId;
            printf("Done rendering frame %i.\n", frameId);

        	//Try resizing the pipeline for test.
            if(frameId == 50)
            {
                renderer->Resize(1600, 800);
                camera.UpdateProjection(70.f, 0.1f, 600.f, static_cast<float>(settings.resolutionX) / static_cast<float>(settings.resolutionY));
            }
        	
        	//Stop to test pipeline cleanup after some frames.
            if(frameId > 6000)
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
        printf("Renderer successfully cleaned up!\n");
    }
    else
    {
        printf("Could not clean up renderer properly!\n");
    }

    printf("Program execution finished.\nPress any key to continue.\n");
    getchar();
}