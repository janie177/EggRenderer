#version 450

layout (input_attachment_index = 0, set = 0, binding = 0) uniform subpassInput inDepth;
layout (input_attachment_index = 1, set = 0, binding = 1) uniform subpassInput inPosition;
layout (input_attachment_index = 2, set = 0, binding = 2) uniform subpassInput inNormal;
layout (input_attachment_index = 3, set = 0, binding = 3) uniform subpassInput inTangent;
layout (input_attachment_index = 4, set = 0, binding = 4) uniform subpassInput inUvMaterialId;

layout(location = 5) out vec4 outColor;         //In the framebuffer, the output is the 5th bound buffer.

void main() 
{
    float depth = subpassLoad(inDepth).r;
    vec3 color = subpassLoad(inNormal).rgb;

    if(depth < 1.0)
    {
        //For now make it red.
        outColor = vec4(color, 1.0);
    }
    else
    {
        discard;
    }
}