#include "Renderer.h"
#include <memory>
#include <glm/glm/glm.hpp>

/*
 * Program entry point.
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
    settings.clearColor = glm::vec4(0.f, 0.5f, 0.9f, 1.f);

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

        //Main loop
        bool run = true;
        while(run)
        {
            //Draw
            run = renderer->DrawFrame(drawData);

            //Update input
            auto input = renderer->QuerryInput();
            MouseEvent mEvent;
            KeyboardEvent kEvent;
            while(input.GetNextEvent(mEvent))
            {
                if(mEvent.action == MouseAction::SCROLL)
                {
                    
                }
                else if(mEvent.action == MouseAction::MOVE_X)
                {
                    
                }
                else if(mEvent.action == MouseAction::MOVE_Y)
                {
                    
                }
                else if(mEvent.action == MouseAction::CLICK)
                {
                    std::string mbutton = (mEvent.button == MouseButton::MMB ? "MMB" : mEvent.button == MouseButton::RMB ? "RMB" : "LMB");
                    printf("Mouse button clicked: %s.\n", mbutton.c_str());
                }
            }
            while(input.GetNextEvent(kEvent))
            {
                if(kEvent.action == KeyboardAction::KEY_PRESSED)
                {
                    printf("Key pressed: %u.\n", kEvent.keyCode);

                    //Stop the program when escape is pressed.
                    if(kEvent.keyCode == GLFW_KEY_ESCAPE)
                    {
                        run = false;
                    }

                    if(kEvent.keyCode == GLFW_KEY_ENTER)
                    {
                        printf("Resizing!\n");
                        renderer->Resize(!renderer->IsFullScreen(), 1280, 720);
                    }
                }
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