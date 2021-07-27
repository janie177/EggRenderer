#pragma once

namespace egg
{
    /*
     * Data for a single instance of a mesh in the scene.
     */
    struct MeshInstance
    {
        glm::mat4 m_Transform;	//The transform of the mesh. //TODO: material
    };

    /*
     * API handle for a mesh on the GPU.
     */
    class EggMesh
    {
    public:
        virtual ~EggMesh() = default;
    };
}