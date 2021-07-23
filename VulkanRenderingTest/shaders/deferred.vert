#version 450

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

void main() 
{
    outPosition = inPosition + vec3(0.0, 0.0, -10.0);        //TODO transform.
    outNormal = inNormal;   //TODO transform
    outTangent = inTangent; //TODO transform
    outUvs = inUvs;
    outMaterialId = 1337;    //TODO

    gl_Position = pushData.viewProjectionMatrix * vec4(outPosition, 1.0);
}