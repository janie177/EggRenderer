#version 460 core

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec4 inTangent;
layout(location = 3) in vec2 inUvs;
layout(location = 4) in flat uint inMaterialId;
layout(location = 5) in flat uint inCustomId;

layout(location = 0) out vec4 outPosition;
layout(location = 1) out vec4 outNormal;
layout(location = 2) out vec4 outTangent;
layout(location = 3) out vec4 outUvsCustomId;

void main() 
{
    //Pack the material ID into position and normal W components. Both need to be read when shading anyways, so it doesn't matter that it's two reads.
    vec2 materialIdAsVector = unpackHalf2x16(inMaterialId);
    outPosition = vec4(inPosition, materialIdAsVector.x);   //Use the W component to store the first half of the material ID.
    outNormal = vec4(inNormal, materialIdAsVector.y);       //Use the 
    outTangent = vec4(inTangent);

    //Split material ID in half for 32 bit precision. Store in UV so that a single read can retrieve it (for pixel picking).
    vec2 customIdAsVector = unpackHalf2x16(inCustomId);
    outUvsCustomId.xy = inUvs;   //UVs and mesh ID are combined.
    outUvsCustomId.zw = customIdAsVector; //Interpret the uint as two floats. Use packHalf2x16 to get the uint back.
}