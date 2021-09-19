#version 460
#extension GL_KHR_vulkan_glsl: enable

layout (input_attachment_index = 0, set = 0, binding = 0) uniform subpassInput inDepth;
layout (input_attachment_index = 1, set = 0, binding = 1) uniform subpassInput inPosition;
layout (input_attachment_index = 2, set = 0, binding = 2) uniform subpassInput inNormal;
layout (input_attachment_index = 3, set = 0, binding = 3) uniform subpassInput inTangent;
layout (input_attachment_index = 4, set = 0, binding = 4) uniform subpassInput inUvCustomId;

layout (std430, binding = 0, set = 1) buffer MaterialData
{
    uvec4 data[];

} materialBuffer;

struct PackedLightData
{
    vec4 data0;
    vec4 data1;
    ivec4 data2;
};

layout (std430, binding = 1, set = 1) buffer AreaLights
{
    PackedLightData data[];

} areaLightBuffer;

layout (std430, binding = 2, set = 1) buffer DirectionalLights
{
    PackedLightData data[];

} directionalLightBuffer;

//Push data
layout( push_constant ) uniform PushData {
  vec4 cameraPosition;
  uvec4 lightCounts;
} pushData;

layout(location = 5) out vec4 outColor;         //In the framebuffer, the output is the 5th bound buffer.

//Calculate the BRDF.
vec3 calculateBRDF(vec3 toLightDir, vec3 toCameraDir, vec3 surfaceNormal, float metallic, float roughness, vec3 diffuse);
float DistributionGGX(vec3 surfaceNormal, vec3 h, float roughness);
float GeometrySchlickGGX(float sNormalToCamDot, float roughness);
float GeometrySmith(vec3 surfaceNormal, vec3 toCameraDir, vec3 toLightDir, float roughness);
vec3 FresnelSchlick(float cosTheta, vec3 f0);

void main() 
{
    //Temporary light and material values;
    const vec3 ambientLight = {0.07, 0.07, 0.07};

    //If no hit is present for this pixel, discard.
    float depth = subpassLoad(inDepth).r;
    if(depth == 1.0)
    {
        discard;
    }

    //Extract the data from the g buffer.
    vec4 position = subpassLoad(inPosition).rgba;
    vec4 normalRaw = subpassLoad(inNormal).rgba;
    vec4 tangentRaw = subpassLoad(inTangent).rgba;
    vec4 uvCustomId = subpassLoad(inUvCustomId).rgba;

    //Pack together the bits to get the uint IDs.
    uint customId = packHalf2x16(uvCustomId.zw);
    uint materialId = packHalf2x16(vec2(position.w, normalRaw.w));

    //Extract the packed material data.
    uvec4 packedMaterialData = materialBuffer.data[materialId];
    uint textureId = packedMaterialData.y;
    vec2 metallicRoughness = unpackUnorm2x16(packedMaterialData.x);
    vec3 albedo = vec3(unpackUnorm4x8(packedMaterialData.z));
    vec3 emissive = vec3(unpackUnorm4x8(packedMaterialData.w));

    //Normalize and calculate the bitangent.
    const vec3 normal = normalize(normalRaw.xyz);
    const vec3 tangent = normalize(tangentRaw.xyz);
    const vec3 biTangent = cross(normal, tangent) * tangentRaw.w;

    //Light vector that is appended to.
    vec3 finalLightColor = ambientLight;

    PackedLightData currentLight;

    //Loop over the area lights.
    for(uint i = 0; i < pushData.lightCounts.x; ++i)
    {
        currentLight = areaLightBuffer.data[i];

        #define lightPosition (currentLight.data0.xyz)
        #define lightRadius (currentLight.data0.w)
        #define lightRadiance (currentLight.data1.xyz)
        #define shadowIndex (currentLight.data2.x)
        const float lightRadiusSquared = lightRadius * lightRadius;
        const float lightArea = 3.1415926536 * lightRadiusSquared;     //Area is equal to the disk projected onto the pixel hemisphere (surface of the circle with the radius of the light).

        vec3 pixelToLightDir = lightPosition - position.xyz;
        const float lDistance = length(pixelToLightDir) - lightRadius;
        pixelToLightDir /= lDistance;
        const float cosI = max(dot(pixelToLightDir, normal), 0.0);
        const float cosO = 1.0;//max(0.0, dot(lightNormal, -pixelToLightDir));  //Since a sphere light always points at a surface.

        //When true, this light has a shadow map defined.
        bool shadowed = false;
        if(shadowIndex > -1)
        {
            //TODO check for shadow.
            //Do not append light if occluded.
            shadowed = false;
        }

        //Only shade when the light is visible.
        if (cosI > 0.f && lDistance > 0.01 && !shadowed)
        {
            const vec3 toCameraDir = normalize(pushData.cameraPosition.xyz - position.xyz);

            //Geometry term G(x). Solid angle is the light area projected onto the pixel hemisphere.
            const float solidAngle = (cosO * lightArea) / (lDistance * lDistance);
            const vec3 brdf = calculateBRDF(pixelToLightDir, toCameraDir, normal, metallicRoughness.x, metallicRoughness.y, albedo);

            //The final light transport value.
            //CosI converts from radiance to irradiance.
            //brdf is the light transport based on the microfacet normal.
            //SolidAngle is the surface of the light projected onto the hemisphere of the shaded pixel (scale according to distance and such).
            finalLightColor += brdf * solidAngle * cosI * lightRadiance;
        }
    }

    //Loop over the directional lights.
    for(uint i = 0; i < pushData.lightCounts.y; ++i)
    {
        currentLight = directionalLightBuffer.data[i];

        #define lightDirection (currentLight.data0.xyz)
        #define lightRadiance (currentLight.data1.xyz)
        #define shadowIndex (currentLight.data2.x)

        float cosI = dot(-lightDirection, normal);

        //When true, this light has a shadow map defined.
        bool shadowed = false;
        if(shadowIndex > -1)
        {
            //TODO check for shadow.
            //Do not append light if occluded.
            shadowed = false;
        }

        //Only shade when the light is visible.
        if (cosI > 0.f && !shadowed)
        {
            const vec3 toCameraDir = normalize(pushData.cameraPosition.xyz - position.xyz);

            //Geometry term G(x). Solid angle is the light area projected onto the pixel hemisphere.
            const vec3 brdf = calculateBRDF(-lightDirection, toCameraDir, normal, metallicRoughness.x, metallicRoughness.y, albedo);

            //The final light transport value.
            //CosI converts from radiance to irradiance.
            //brdf is the light transport based on the microfacet normal.
            //SolidAngle is the surface of the light projected onto the hemisphere of the shaded pixel (scale according to distance and such).
            finalLightColor += brdf * cosI * lightRadiance;
        }
    }

    //Finally write to the output buffer.
    outColor = vec4(finalLightColor, 1.0);
}

//BRDF below.

vec3 calculateBRDF(vec3 toLightDir, vec3 toCameraDir, vec3 surfaceNormal, float metallic, float roughness, vec3 diffuse)
{
        vec3 f0 = vec3(0.04); 
        f0 = mix(f0, diffuse, metallic);
        vec3 h = normalize(toCameraDir + toLightDir);      
        
        // cook-torrance brdf
        float ndf = DistributionGGX(surfaceNormal, h, roughness);        
        float g = GeometrySmith(surfaceNormal, toCameraDir, toLightDir, roughness);      
        vec3 f = FresnelSchlick(max(dot(h, toCameraDir), 0.0), f0);       
        
        vec3 kS = f;
        vec3 kD = vec3(1.0) - kS;
        kD *= 1.0 - metallic;	  
        
        vec3 numerator = ndf * g * f;
        float denominator = 4.0 * max(dot(surfaceNormal, toCameraDir), 0.0) * max(dot(surfaceNormal, toLightDir), 0.0);
        vec3 specular = numerator / max(denominator, 0.001);  
            
        // add to outgoing radiance Lo           
        return vec3(kD * diffuse / 3.1415926536 + specular); 
}

float DistributionGGX(vec3 surfaceNormal, vec3 h, float roughness)
{
    float a      = roughness*roughness;
    float a2     = a*a;
    float NdotH  = max(dot(surfaceNormal, h), 0.0);
    float NdotH2 = NdotH*NdotH;
	
    float num   = a2;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = 3.1415926536 * denom * denom;
	
    return num / denom;
}


float GeometrySchlickGGX(float sNormalToCamDot, float roughness)
{
    float r = (roughness + 1.0);
    float k = (r*r) / 8.0;

    float num   = sNormalToCamDot;
    float denom = sNormalToCamDot * (1.0 - k) + k;
	
    return num / denom;
}


float GeometrySmith(vec3 surfaceNormal, vec3 toCameraDir, vec3 toLightDir, float roughness)
{
    float NdotV = max(dot(surfaceNormal, toCameraDir), 0.0);
    float NdotL = max(dot(surfaceNormal, toLightDir), 0.0);
    float ggx2  = GeometrySchlickGGX(NdotV, roughness);
    float ggx1  = GeometrySchlickGGX(NdotL, roughness);
	
    return ggx1 * ggx2;
}


vec3 FresnelSchlick(float cosTheta, vec3 f0)
{
    return f0 + (1.0 - f0) * pow(max(1.0 - cosTheta, 0.0), 5.0);
}