#version 460
#extension GL_EXT_ray_tracing : enable
#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_EXT_buffer_reference2 : enable
#extension GL_EXT_scalar_block_layout : enable
#extension GL_EXT_debug_printf : require

#include "types.glsl"
#include "functions.glsl"

layout(location = 0) rayPayloadInEXT HitPayload rayPayload;
layout(location = 1) rayPayloadEXT bool isShadowed;
layout(constant_id = 0) const uint MAX_RT_DEPTH = 3;
hitAttributeEXT vec2 attribs;
layout(set = 0, binding = 0) uniform accelerationStructureEXT topLevelAS;
layout(set = 1, binding = 1) uniform sampler samplers[];
layout(set = 1, binding = 2) uniform texture2D textures[];

layout(set = 1, binding = 3, scalar) uniform LightsBuffer
{
    Light light;
}
lights[];

layout(buffer_reference, std430, scalar) readonly buffer VertexBuffer
{
    Vertex vertices[];
};

layout(buffer_reference, std430, scalar) readonly buffer IndexBuffer
{
    uint indices[];
};

layout(buffer_reference, std430, scalar) readonly buffer MaterialConstantsBuffer
{
    MaterialConstants materialConstants;
};

struct SurfaceStorage
{
    IndexBuffer indexBuffer;
    VertexBuffer vertexBuffer;
    MaterialConstantsBuffer materialConstantsBuffer;
    uint colorSamplerIndex;
    uint colorImageIndex;
    uint materialSamplerIndex;
    uint materialImageIndex;
    uint normalMapIndex;
    uint normalSamplerIndex;
    uint startIndex;
    uint count;
};

layout(std430, set = 1, binding = 0, std430, scalar) readonly buffer SurfaceStorageBuffer
{
    SurfaceStorage surface;
}
surfaceStorages[];

//push constants block
layout(scalar, push_constant) uniform RayPushConstants
{
    RayPush rayPush;
}
push;

const float tMin = 0.01;
const float tMax = 10000.;
const float ENERGY_LOSS = 0.8;
float ENERGY_MIN = 1. * pow(ENERGY_LOSS, MAX_RT_DEPTH);

void main()
{
    if (rayPayload.depth > MAX_RT_DEPTH)
        return;
    // Set depth +1
    rayPayload.depth++;

    // -------- LOAD ALL THE DATA --------
    const uint surfaceId = gl_InstanceCustomIndexEXT;
    const uint primitiveIndex = gl_PrimitiveID * 3;

    RayPush rayPush = push.rayPush;
    SurfaceStorage surface = surfaceStorages[nonuniformEXT(surfaceId)].surface;

    IndexBuffer iBuffer = surface.indexBuffer;
    VertexBuffer vBuffer = surface.vertexBuffer;
    MaterialConstantsBuffer mBuffer = surface.materialConstantsBuffer;
    MaterialConstants mConstants = mBuffer.materialConstants;
    const uint colorSamplerIndex = surface.colorSamplerIndex;
    const uint colorImageIndex = surface.colorImageIndex;
    const uint materialSamplerIndex = surface.materialSamplerIndex;
    const uint materialImageIndex = surface.materialImageIndex;
    const uint normalMapIndex = surface.normalMapIndex;
    const uint normalSamplerIndex = surface.normalSamplerIndex;

    const uint i0 = iBuffer.indices[primitiveIndex];
    const uint i1 = iBuffer.indices[primitiveIndex + 1];
    const uint i2 = iBuffer.indices[primitiveIndex + 2];

    const Vertex v0 = vBuffer.vertices[i0];
    const Vertex v1 = vBuffer.vertices[i1];
    const Vertex v2 = vBuffer.vertices[i2];

    const vec3 vertPos0 = v0.position;
    const vec3 vertPos1 = v1.position;
    const vec3 vertPos2 = v2.position;

    const vec3 norm0 = v0.normal;
    const vec3 norm1 = v1.normal;
    const vec3 norm2 = v2.normal;

    const vec2 uv0 = v0.uv;
    const vec2 uv1 = v1.uv;
    const vec2 uv2 = v2.uv;

    const vec3 barycentrics = vec3(1. - attribs.x - attribs.y, attribs.x, attribs.y);

    // Computing the coordinates of the hit position
    const vec3 pos = vertPos0 * barycentrics.x + vertPos1 * barycentrics.y
                     + vertPos2 * barycentrics.z;

    const vec3 normalVtxRaw = norm0 * barycentrics.x + norm1 * barycentrics.y
                              + norm2 * barycentrics.z; // already normalized
    // Apply the transformation to the normals (not done in BLAS creation)
    const vec3 normalVtx = normalize((gl_WorldToObjectEXT * vec4(normalVtxRaw, 0)).xyz);

    const vec2 uv = uv0 * barycentrics.x + uv1 * barycentrics.y + uv2 * barycentrics.z;

    const vec3 tangentRaw = v0.tangent.xyz * barycentrics.x + v1.tangent.xyz * barycentrics.y
                            + v2.tangent.xyz * barycentrics.z; // range [-1, 1]
    const float handedness = v0.tangent.w;                     // All vi.tangent.w are the same
    const vec3 tangent = normalize((gl_WorldToObjectEXT * vec4(tangentRaw, 0)).xyz);

    const vec3 bitangent = cross(normalVtx, tangent) * handedness;

    const mat3 TBN = mat3(tangent, bitangent, normalVtx);

    const vec4 baseColor = texture(sampler2D(textures[nonuniformEXT(colorImageIndex)],
                                             samplers[nonuniformEXT(colorSamplerIndex)]),
                                   uv)
                           * mConstants.baseColorFactor; // range [0, 1]

    const vec4 metallicRoughness = texture(sampler2D(textures[nonuniformEXT(materialImageIndex)],
                                                     samplers[nonuniformEXT(materialSamplerIndex)]),
                                           uv)
                                   * vec4(0,
                                          mConstants.roughnessFactor,
                                          mConstants.metallicFactor,
                                          0);
    const float perceptualRoughness = metallicRoughness.y;
    const float metallic = metallicRoughness.z;

    const vec4 normalTexRaw = 2.
                                  * texture(sampler2D(textures[nonuniformEXT(normalMapIndex)],
                                                      samplers[nonuniformEXT(normalSamplerIndex)]),
                                            uv)
                              - 1.; // range [0, 1] -> [-1, 1]
    const vec3 normal = normalize(TBN * normalTexRaw.xyz);

    // Transforming the position to world space
    const vec3 worldPos = vec3(gl_ObjectToWorldEXT * vec4(pos, 0));

    // -------------- BRDF --------------

    // Parametrization
    const vec3 diffuseColor = (1. - metallic) * baseColor.xyz;
    const float f90 = 1.;
    const float reflectance = 0.5;
    const vec3 f0 = 0.16 * reflectance * reflectance * (1. - metallic) + baseColor.xyz * metallic;
    // perceptually linear roughness to roughness
    const float a = perceptualRoughness * perceptualRoughness;
    //        print_val("%f ", metallic, 0.5, 1.);

    // Ray directions
    const vec3 v = -gl_WorldRayDirectionEXT; // Inverse incoming (view) ray direction. Already normalized
    vec3 totalLuminance = vec3(0.);
    for (uint i = 0; i < rayPush.numLights; i++) {
        Light light = lights[nonuniformEXT(i)].light;
        vec3 l;
        float distanceSquared = 1.;
        // Vector to the light
        if (light.type == 0) { // Point
            l = light.positionOrDirection - worldPos;
            distanceSquared = dot(l, l);
            l /= sqrt(distanceSquared);
        } else if (light.type == 1) // Directional
        {
            l = -light.positionOrDirection; // Already normalized from Host
        }
        // SHADOWS
        const uint shadowFlags = gl_RayFlagsTerminateOnFirstHitEXT | gl_RayFlagsOpaqueEXT
                                 | gl_RayFlagsSkipClosestHitShaderEXT;
        // We initialize to true, if the miss shader is called it sets it to false
        isShadowed = true;
        traceRayEXT(topLevelAS,  // acceleration structure
                    shadowFlags, // rayFlags
                    0xFF,        // cullMask
                    0,           // sbtRecordOffset
                    0,           // sbtRecordStride
                    1,           // missIndex
                    worldPos,    // ray origin
                    tMin,        // ray min range
                    l,           // ray direction
                    tMax,        // ray max range
                    1            // payload (location = 1)
        );
        if (isShadowed)
            continue;

        const vec3 h = normalize(l + v);

        const float NoV = abs(dot(normal, v)) + 1e-5;
        const float NoL = clamp(dot(normal, l), 0., 1.);
        const float NoH = clamp(dot(normal, h), 0., 1.);
        const float LoH = clamp(dot(l, h), 0., 1.);

        const float D = D_GGX(NoH, a);
        const vec3 F = F_Schlick(LoH, f0, f90);
        const float V = V_SmithGGXCorrelated(NoV, NoL, a);

        // specular BRDF
        const vec3 Fr = D * V * F;

        // diffuse BRDF
        const vec3 Fd = diffuseColor * Fd_Lambert();

        const vec3 BSDF = Fr + Fd;

        vec3 luminance = vec3(0.);
        if (light.type == 0) { // Point light
            luminance = evaluate_point_light(light, distanceSquared, BSDF, NoL);
        } else if (light.type == 1) { // Directional light
            luminance = evaluate_directional_light(light, BSDF, NoL);
        }
        totalLuminance += luminance;
    }
    // Add this particular contribution to the total ray payload
    rayPayload.hitValue += rayPayload.energyFactor * totalLuminance;
    // After color transfer, lose energy
    //    rayPayload.energyFactor *= ENERGY_LOSS;
}
