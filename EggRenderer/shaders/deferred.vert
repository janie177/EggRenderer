#version 460 core
#extension GL_KHR_vulkan_glsl: enable
#extension GL_EXT_nonuniform_qualifier: enable

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec4 inTangent;
layout(location = 3) in vec2 inUvs;

layout(location = 0) out vec3 outPosition;
layout(location = 1) out vec3 outNormal;
layout(location = 2) out vec4 outTangent;
layout(location = 3) out vec2 outUvs;
layout(location = 4) out flat uint outMaterialId;
layout(location = 5) out flat uint outCustomId;

layout( push_constant ) uniform PushData {
  mat4 viewProjectionMatrix;    //The view projection matrix.
  vec4 data1;                   //Some data that can be set to whatever.
  vec4 data2;
  vec4 data3;
  vec4 data4;
} pushData;

struct InstanceData
{
    mat4x3 transform;   //The transform with 4 columns and 3 rows.
    uvec4 customData;   //Four uints containing: material id(0), custom id(1)
};

layout (std430, binding = 0) buffer IndirectionBuffer
{
    uint indices[];

} indirectionBuffer;

layout (std430, binding = 1) buffer InstanceDataBuffer
{
    InstanceData instances[];

} instanceBuffer;

void main() 
{
    //gl_InstanceIndex is equal to the index of the instance data indirection buffer thanks to the instance offset in the draw command.
    InstanceData instance = instanceBuffer.instances[indirectionBuffer.indices[gl_InstanceIndex]];

    //The material and mesh ID are stored in the matrix to save uploading bandwidth.
    outMaterialId = instance.customData[0];    
    outCustomId = instance.customData[1];

    outNormal = vec3(instance.transform * vec4(inNormal, 0.0));

    vec4 pos = vec4(instance.transform * vec4(inPosition, 1.0), 1.0);
    outPosition = vec3(pos);
    outTangent = vec4(((instance.transform * vec4(inTangent.xyz, 0.f)).xyz), inTangent.w);

    gl_Position = pushData.viewProjectionMatrix * pos;
}