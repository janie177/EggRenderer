#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec3 inTangent;
layout(location = 3) in vec2 inUvs;

layout(location = 0) out vec3 outPosition;
layout(location = 1) out vec4 outNormalMaterialId;
layout(location = 2) out vec3 outTangent;
layout(location = 3) out vec2 outUvs;

void main() 
{
    outPosition = inPosition;
    outNormalMaterialId = vec4(inNormal, 1.0);  //TODO material
    outTangent = inTangent;
    outUvs = inUvs;
}