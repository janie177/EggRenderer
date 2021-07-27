#include "Renderer.h"
#include <memory>
#include <glm/glm/glm.hpp>

/*
 * Program entry point.
 */
int main()
{
    using namespace egg;

    RendererSettings settings;
    settings.debugFlags = DebugPrintFlags::ERROR | DebugPrintFlags::WARNING;
    settings.vSync = true;
    settings.clearColor = glm::vec4(0.f, 0.5f, 0.9f, 1.f);
    settings.lockCursor = true;
    settings.m_SwapBufferCount = 3;

    auto renderer = std::make_unique<Renderer>();
    Camera camera;
    camera.UpdateProjection(70.f, 0.1f, 600.f, static_cast<float>(settings.resolutionX) / static_cast<float>(settings.resolutionY));

    if (renderer->Init(settings))  //This is a bit flag, can do DebugPrintFlags::ERROR | DebugPrintFlags::WARNING etc.
    {
        Transform meshTransform;
        //meshTransform.Scale({2.f, 1.f, 0.5f});
        ShapeCreateInfo shapeInfo;
        shapeInfo.m_Sphere.m_SectorCount = 20;
        shapeInfo.m_Sphere.m_StackCount = 20;
        shapeInfo.m_ShapeType = Shape::SPHERE;
        shapeInfo.m_InitialTransform = meshTransform.GetTransformation();
        auto mesh = renderer->CreateMesh(shapeInfo);

        constexpr auto NUM_CUBE_INSTANCES = 50000;

    	//Drawing information.
        DrawData drawData;
        DrawCall drawCall;
        std::vector<MeshInstance> meshInstances(NUM_CUBE_INSTANCES);

        Transform t;
        for(int i = 0; i < NUM_CUBE_INSTANCES; ++i)
        {
            t.Translate(t.GetForward() * 0.2f);
            t.Translate(t.GetUp() * 0.2f);
            t.RotateAround({ 0.f, 0.f, 0.f }, { 0.f, 1.f, 0.f }, 0.1f);
            meshInstances[i].m_Transform = t.GetTransformation();
        }

        drawCall.m_Mesh = mesh;
        drawCall.m_NumInstances = NUM_CUBE_INSTANCES;
        drawCall.m_Transparent = false;
        drawCall.m_pMeshInstances = meshInstances.data();
        drawData.m_NumDrawCalls = 1;
        drawData.m_pDrawCalls = &drawCall;
        drawData.m_Camera = &camera;

        //Main loop
        static int frameIndex = 0;
        bool run = true;
        while(run)
        {
            if (frameIndex % 100 == 0) printf("Frame #%i.\n", frameIndex);
            ++frameIndex;

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
                    camera.GetTransform().Rotate(camera.GetTransform().GetRight(), static_cast<float>(mEvent.value) / mouseDivider);
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
            const auto upState = input.GetKeyState(GLFW_KEY_E);
            const auto downState = input.GetKeyState(GLFW_KEY_Q);
            if(forwardState != ButtonState::NOT_PRESSED) camera.GetTransform().Translate(camera.GetTransform().GetForward() * -movementSpeed);
            if (rightState != ButtonState::NOT_PRESSED) camera.GetTransform().Translate(camera.GetTransform().GetRight() * movementSpeed);
            if (leftState != ButtonState::NOT_PRESSED) camera.GetTransform().Translate(camera.GetTransform().GetLeft() * movementSpeed);
            if (backwardState != ButtonState::NOT_PRESSED) camera.GetTransform().Translate(camera.GetTransform().GetBack() * -movementSpeed);
            if (upState != ButtonState::NOT_PRESSED) camera.GetTransform().Translate(camera.GetTransform().GetWorldUp() * -movementSpeed);
            if (downState != ButtonState::NOT_PRESSED) camera.GetTransform().Translate(camera.GetTransform().GetWorldDown() * -movementSpeed);

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