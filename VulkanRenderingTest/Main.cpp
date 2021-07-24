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
        Vertex{glm::vec3{-1.f, -1.f, -1.f}, glm::vec3{1.f, 0.f, 0.f}, glm::vec3{1.f, 1.f, 1.f}, glm::vec2{0.f, 0.f}},
        Vertex{glm::vec3{1.f, -1.f, -1.f}, glm::vec3{0.f, 1.f, 0.f}, glm::vec3{1.f, 1.f, 1.f}, glm::vec2{0.f, 0.f}},
        Vertex{glm::vec3{1.f, 1.f, -1.f}, glm::vec3{0.f, 0.f, 1.f}, glm::vec3{1.f, 1.f, 1.f}, glm::vec2{0.f, 0.f}},
        Vertex{glm::vec3{-1.f, 1.f, -1.f}, glm::vec3{0.4f, 0.2f, 0.6f}, glm::vec3{1.f, 1.f, 1.f}, glm::vec2{0.f, 0.f}},
        Vertex{glm::vec3{-1.f, -1.f, 1.f}, glm::vec3{1.f, 0.f, 0.f}, glm::vec3{1.f, 1.f, 1.f}, glm::vec2{0.f, 0.f}},
        Vertex{glm::vec3{1.f, -1.f, 1.f}, glm::vec3{0.f, 1.f, 0.f}, glm::vec3{1.f, 1.f, 1.f}, glm::vec2{0.f, 0.f}},
        Vertex{glm::vec3{1.f, 1.f, 1.f}, glm::vec3{0.f, 0.f, 1.f}, glm::vec3{1.f, 1.f, 1.f}, glm::vec2{0.f, 0.f}},
        Vertex{glm::vec3{-1.f, 1.f, 1.f}, glm::vec3{0.4f, 0.2f, 0.6f}, glm::vec3{1.f, 1.f, 1.f}, glm::vec2{0.f, 0.f}}

    };
    std::vector<uint32_t> indices{
    0, 1, 3, 3, 1, 2,
    1, 5, 2, 2, 5, 6,
    5, 4, 6, 6, 4, 7,
    4, 0, 7, 7, 0, 3,
    3, 2, 7, 7, 2, 6,
    4, 5, 0, 0, 5, 1
    };

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
                constexpr float mouseDivider = 400.f;
                if(mEvent.action == MouseAction::SCROLL)
                {
                    
                }
                else if(mEvent.action == MouseAction::MOVE_X)
                {
                    camera.GetTransform().Rotate(Transform::GetWorldUp(), static_cast<float>(mEvent.value) / -mouseDivider);
                }
                else if(mEvent.action == MouseAction::MOVE_Y)
                {
                    camera.GetTransform().Rotate(Transform::GetWorldRight(), static_cast<float>(mEvent.value) / mouseDivider);
                }
                else if(mEvent.action == MouseAction::CLICK)
                {
                    std::string mbutton = (mEvent.button == MouseButton::MMB ? "MMB" : mEvent.button == MouseButton::RMB ? "RMB" : "LMB");
                    printf("Mouse button clicked: %s.\n", mbutton.c_str());
                }
            }

            constexpr float movementSpeed = 0.01f;
            const auto forwardState = input.GetKeyState(GLFW_KEY_W);
            const auto rightState = input.GetKeyState(GLFW_KEY_D);
            const auto leftState = input.GetKeyState(GLFW_KEY_A);
            const auto backwardState = input.GetKeyState(GLFW_KEY_S);
            if(forwardState != ButtonState::NOT_PRESSED) camera.GetTransform().Translate(camera.GetTransform().GetForward() * -movementSpeed);
            if (rightState != ButtonState::NOT_PRESSED) camera.GetTransform().Translate(camera.GetTransform().GetRight() * movementSpeed);
            if (leftState != ButtonState::NOT_PRESSED) camera.GetTransform().Translate(camera.GetTransform().GetLeft() * movementSpeed);
            if (backwardState != ButtonState::NOT_PRESSED) camera.GetTransform().Translate(camera.GetTransform().GetBack() * -movementSpeed);

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
                        renderer->Resize(!renderer->IsFullScreen(), 1280, 720);
                        const auto resolution = renderer->GetResolution();
                        camera.UpdateProjection(70.f, 0.1f, 1000.f, resolution.x / resolution.y);
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
}