#version 460 core

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec3 inTangent;
layout(location = 3) in vec2 inUvs;
layout(location = 4) in flat int inMaterialId;

layout(location = 0) out vec3 outPosition;
layout(location = 1) out vec4 outNormalMaterialId;
layout(location = 2) out vec3 outTangent;
layout(location = 3) out vec3 outUvsMaterialId;

void main() 
{
    outPosition = inPosition;
    outNormalMaterialId = vec4(inNormal, 1.0);  //TODO material
    outTangent = inTangent;
    outUvsMaterialId = vec3(inUvs, inMaterialId);   //UVs and Material ID are combined because the Z channel wasn't used for anything.
}