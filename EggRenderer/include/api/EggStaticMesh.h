#pragma once

namespace egg
{
    /*
     * The vertex format for meshes.
     */
    struct Vertex
    {
        glm::vec3 position;
        glm::vec3 normal;
        glm::vec4 tangent;
        glm::vec2 uv;
    };

    /*
     * Struct containing all the information needed to create a mesh.
     */
    struct StaticMeshCreateInfo
    {
        const Vertex* m_VertexBuffer = nullptr;
        const uint32_t* m_IndexBuffer = nullptr;
        uint32_t m_NumIndices = 0;
        uint32_t m_NumVertices = 0;
    };

    /*
     * API handle for a mesh on the GPU.
     */
    class EggStaticMesh
    {
    public:
        virtual ~EggStaticMesh() = default;
    };
}