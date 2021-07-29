#pragma once

namespace egg
{
    /*
     * Data for a single instance of a mesh in the scene.
     */
    struct MeshInstance
    {
        /*
         * The transform to use for the mesh instance.
         */
        glm::mat4 m_Transform;

        /*
         * The index into the materials array to use.
         */
        uint32_t m_MaterialIndex = 0;

        /*
         * A custom pointer uint32_t can be set to anything.
         * Can be used to select objects in the scene with the mouse for example.
         * Each instance can be identified this way by coupling it to a lookup table.
         */
        uint32_t m_CustomData = 0;
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