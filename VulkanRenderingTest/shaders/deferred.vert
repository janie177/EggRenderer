#version 450
#extension GL_KHR_vulkan_glsl: enable
#extension GL_EXT_nonuniform_qualifier: enable

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec3 inTangent;
layout(location = 3) in vec2 inUvs;

layout(location = 0) out vec3 outPosition;
layout(location = 1) out vec3 outNormal;
layout(location = 2) out vec3 outTangent;
layout(location = 3) out vec2 outUvs;
layout(location = 4) out flat int outMaterialId;

layout( push_constant ) uniform PushData {
  mat4 viewProjectionMatrix;    //The view projection matrix.
  vec4 data1;                   //Some data that can be set to whatever.
  vec4 data2;
  vec4 data3;
  vec4 data4;
} pushData;

struct InstanceData
{
    mat4 transform;  
};

layout(std430, binding = 0, set = 0) buffer InstanceDataBuffer
{
  InstanceData instances[];

} instanceData;

void main() 
{
    uint offset = floatBitsToInt(pushData.data1.x) + gl_InstanceIndex;
    mat4 transform = instanceData.instances[offset].transform;

    vec4 pos = transform * vec4(inPosition, 1.0);
    outPosition = vec3(pos);
    outNormal = vec3(transform * vec4(inNormal, 0.0));
    outTangent = vec3(transform * vec4(inTangent, 0.0));
    outMaterialId = 1337;    //TODO

    gl_Position = pushData.viewProjectionMatrix * pos;
}