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

//struct InstanceData
//{
//    vec4 column1;
//    vec4 column2;
//    vec4 column3;
//    vec4 column4;
//};
//
//layout (std430, binding = 0) buffer InstanceDataBuffer
//{
//    InstanceData instances[];
//
//} instanceBuffer;
//
//vec4 Transform(InstanceData data, vec4 vector);
//
//void main() 
//{
//    uint offset = floatBitsToInt(pushData.data1.x) + gl_InstanceIndex;
//
//    //The material and mesh ID are stored in the matrix to save uploading bandwidth.
//    outMaterialId = floatBitsToUint(instanceBuffer.instances[offset].column1[3]);       
//    outCustomId = floatBitsToUint(instanceBuffer.instances[offset].column2[3]);
//
//    outNormal = vec3(Transform(instanceBuffer.instances[offset], vec4(inNormal, 0.0)));
//
//    vec4 pos = Transform(instanceBuffer.instances[offset], vec4(inPosition, 1.0));
//    outPosition = vec3(pos);
//    outTangent = vec4((Transform(instanceBuffer.instances[offset], vec4(inTangent.xyz, 0.f)).xyz), inTangent.w);
//
//    gl_Position = pushData.viewProjectionMatrix * pos;
//}
//
//vec4 Transform(InstanceData data, vec4 vector)
//{
//    const float dot1 = dot(vec4(data.column1.x, data.column2.x, data.column3.x, 0), vector);
//    const float dot2 = dot(vec4(data.column1.y, data.column2.y, data.column3.y, 0), vector);
//    const float dot3 = dot(vec4(data.column1.z, data.column2.z, data.column3.z, 0), vector);
//    return vec4(dot1, dot2, dot3, vector.w);
//}

struct InstanceData
{
    mat4 transform;
};

layout (std430, binding = 0) buffer InstanceDataBuffer
{
    InstanceData instances[];

} instanceBuffer;

void main() 
{
    uint offset = floatBitsToInt(pushData.data1.x) + gl_InstanceIndex;
    mat4 transform = instanceBuffer.instances[offset].transform;

    //The material and mesh ID are stored in the matrix to save uploading bandwidth.
    outMaterialId = floatBitsToUint(transform[0][3]);       
    outCustomId = floatBitsToUint(transform[1][3]);
    transform[0][3] = 0;
    transform[1][3] = 0;

    outNormal = vec3(transform * vec4(inNormal, 0.0));

    vec4 pos = transform * vec4(inPosition, 1.0);
    outPosition = vec3(pos);
    outTangent = vec4(((transform * vec4(inTangent.xyz, 0.f)).xyz), inTangent.w);

    gl_Position = pushData.viewProjectionMatrix * pos;
}