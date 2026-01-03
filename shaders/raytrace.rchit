#version 460
#extension GL_EXT_ray_tracing : enable
#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_EXT_buffer_reference2 : enable
#extension GL_EXT_scalar_block_layout : enable
#extension GL_EXT_debug_printf : require

#include "types.glsl"
#include "functions.glsl"

layout(location = 0) rayPayloadInEXT HitPayload rayPayload;
layout(location = 1) rayPayloadEXT HitPayload recursivePayload; // Separate payload for recursive shots
layout(location = 2) rayPayloadEXT bool isShadowed;
layout(constant_id = 0) const uint MAX_RT_DEPTH = 3;
layout(constant_id = 1) const uint BOUNCES = 8;
layout(constant_id = 2) const bool RANDOM = true;
layout(constant_id = 3) const bool PRESAMPLE = false;
hitAttributeEXT vec2 attribs;
layout(set = 0, binding = 0) uniform accelerationStructureEXT topLevelAS;
layout(set = 1, binding = 1) uniform sampler samplers[];
layout(set = 1, binding = 2) uniform texture2D textures[];

layout(binding = 3, set = 0) uniform sampler2D presamplingHemisphere;
layout(binding = 4, set = 0) uniform sampler3D presamplingGGX;

layout(set = 1, binding = 3, std430, scalar) readonly uniform LightsBuffer
{
    Light light;
}
lights[];

layout(buffer_reference, std430, scalar) readonly buffer VertexBuffer
{
    Vertex vertices[];
};

layout(buffer_reference, std140, scalar) readonly buffer IndexBuffer
{
    uint indices[];
};

layout(buffer_reference, scalar) readonly buffer MaterialConstantsBuffer
{
    MaterialConstants materialConstants;
};

struct SurfaceStorage
{
    IndexBuffer indexBuffer;
    VertexBuffer vertexBuffer;
    MaterialConstantsBuffer materialConstantsBuffer;
    int colorSamplerIndex;
    int colorImageIndex;
    int materialSamplerIndex;
    int materialImageIndex;
    int normalMapIndex;
    int normalSamplerIndex;
    uint startIndex;
    uint count;
};

layout(set = 1, binding = 0, scalar) readonly uniform SurfaceStorageBuffer
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
// const uint maxDepth = 3;
// const uint numSamples = 8;
uint rngState = gl_LaunchSizeEXT.x * gl_LaunchIDEXT.y + gl_LaunchIDEXT.x; // Initial seed

vec3 direct_lighting(const vec3 worldPos, const vec3 normal, const vec3 v, const vec3 diffuseColor, const vec3 f0, const float f90, const float a, const float NoV)
{
    vec3 directLuminance = vec3(0.);
    for (uint i = 0; i < push.rayPush.numLights; i++) {
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
        // Skip light if light or camera not looking to the hit point
        const float NoL = clamp(dot(normal, l), 0., 1.);
        if (NoL < 1e-5 || NoV < 1e-5)
            continue;
        // SHADOWS
        const uint shadowFlags = gl_RayFlagsTerminateOnFirstHitEXT | gl_RayFlagsOpaqueEXT
                | gl_RayFlagsSkipClosestHitShaderEXT;
        // We initialize to true, if the miss shader is called it sets it to false
        isShadowed = true;
        traceRayEXT(topLevelAS, // acceleration structure
            shadowFlags, // rayFlags
            0xFF, // cullMask
            0, // sbtRecordOffset
            0, // sbtRecordStride
            1, // missIndex
            worldPos, // ray origin
            tMin, // ray min range
            l, // ray direction
            tMax, // ray max range
            2 // payload (location = 1)
        );
        if (isShadowed)
            continue;

        const vec3 h = normalize(l + v);

        const float NoH = clamp(dot(normal, h), 0., 1.);
        const float LoH = clamp(dot(l, h), 0., 1.);

        const vec3 BSDF = BSDF(NoH, LoH, NoV, NoL,
                diffuseColor, f0, f90, a);

        // DIRECT LUMINANCE
        vec3 luminance = vec3(0.);
        if (light.type == 0) { // Point light
            luminance = evaluate_point_light(light, distanceSquared, BSDF);
        } else if (light.type == 1) { // Directional light
            luminance = evaluate_directional_light(light, BSDF);
        }
        directLuminance += luminance;
    }
    return directLuminance;
}

void cosine_sample_hemisphere_cached(in const mat3 S, in const vec2 u, out vec3 sampleDir, out float pdf, out float nDotL) {
    // Round to 2 decimals: 0.0132345 -> 0.01
    const ivec2 index = min(ivec2(round(u * 100.f)), ivec2(99));
    const vec3 sampleInNormalFrame = texelFetch(presamplingHemisphere, index, 0).xyz;
    // print_val("s %f ", presample.w, 2., 1.);
    sampleDir = S * sampleInNormalFrame;
    nDotL = sampleInNormalFrame.z;
    pdf = nDotL * ONEOVERPI;
}

void sample_microfacet_ggx_specular_cached(in const mat3 S, in const vec3 v, in const vec2 u, in const float a, out vec3 sampleDir, out vec3 h, out float nDotL, out float vDotH, out float pdf)
{
    const float a2 = a * a;
    // Round to 2 decimals: 0.0132345 -> 0.01
    const ivec3 index = min(ivec3(round(vec3(u, a) * 100.f)), ivec3(99));

    // Half vector in local frame
    // const vec3 hLocal = vec3(stheta * cos(phi), stheta * sin(phi), ctheta);
    const vec3 hLocal = texelFetch(presamplingGGX, index, 0).xyz;
    const float ctheta = hLocal.z;
    // Move to world frame
    h = S * hLocal;

    // Reflect view direction around half-vector to get light direction
    sampleDir = reflect(-v, h);

    nDotL = dot(sampleDir, S[2]);
    vDotH = dot(v, h);
    // ctheta = nDotH
    // if (nDotL < 1e-5 || vDotH < 1e-5 || ctheta < 1e-5)
    // {
    //     pdf = -1.; // If pdf negative, the sample will be skipped
    //     return;
    // }
    pdf = pdf_microfacet_ggx_specular(ctheta, a2, vDotH);
}

vec3 indirect_lighting(const vec3 worldPos, const vec3 normal, const vec3 v, const vec3 diffuseColor, const vec3 f0, const float f90, const float a, const float NoV)
{
    if (rayPayload.depth == MAX_RT_DEPTH)
        return vec3(0.);

    // Set a different seed for each recursion level
    rngState *= rayPayload.depth;

    // Local normal frame
    const mat3 S = normal_cob(normal);

    // Start sampling
    vec3 indirectLuminance = vec3(0.);
    const uint samplesPerStrategy = BOUNCES; // Split samples between hemisphere and microfacet ggx sampling
    uint samples = BOUNCES;

    // Sample hemisphere
    for (uint s = 0; s < samplesPerStrategy; s++)
    {
        vec2 u = (RANDOM) ? vec2(stepAndOutputRNGFloat(rngState), stepAndOutputRNGFloat(rngState)) : UV[s];
        vec3 l;
        float pdf_diffuse, NoL;
        (PRESAMPLE) ? cosine_sample_hemisphere_cached(S, u, l, pdf_diffuse, NoL) :
        cosine_sample_hemisphere(S, u, l, pdf_diffuse, NoL);

        if (pdf_diffuse < 1e-5) {
            samples--;
            continue;
        }
        const vec3 h = normalize(l + v);
        const float NoH = dot(normal, h);
        const float LoH = dot(l, h);
        const float VoH = dot(v, h);
        const float pdf_specular = pdf_microfacet_ggx_specular(NoH, a * a, VoH);

        // Balance heuristic MIS weight
        // const float weight = (pdf_diffuse * pdf_diffuse) /
        //         (pdf_diffuse * pdf_diffuse + pdf_specular * pdf_specular);
        const float weight = 1.;

        const vec3 BSDF = BSDF(NoH, LoH, NoV, NoL,
                diffuseColor, f0, f90, a);

        recursivePayload.hitValue = vec3(0.);
        recursivePayload.depth = rayPayload.depth;
        traceRayEXT(topLevelAS, // acceleration structure
            gl_IncomingRayFlagsEXT, // rayFlags
            0xFF, // cullMask
            0, // sbtRecordOffset
            0, // sbtRecordStride
            0, // missIndex
            worldPos, // ray origin
            tMin, // ray min range
            l, // ray direction
            tMax, // ray max range
            1 // payload
        );
        // Accumulate indirect lighting
        indirectLuminance += weight * BSDF * recursivePayload.hitValue / pdf_diffuse;
    }

    // Sample microfacet GGX specular
    // for (uint s = 0; s < samplesPerStrategy; s++)
    // {
    //     vec2 u = (RANDOM) ? vec2(stepAndOutputRNGFloat(rngState), stepAndOutputRNGFloat(rngState)) : UV[s];
    //     // vec2 u = vec2(stepAndOutputRNGFloat(rngState), stepAndOutputRNGFloat(rngState));
    //     // u = min(round(u * 100.) / 100., 0.99);
    //     // float aa = min(round(a * 100.) / 100., 0.99);
    //     vec3 l, h;
    //     float pdf_specular, NoL, VoH;
    //     (PRESAMPLE) ? sample_microfacet_ggx_specular_cached(S, v, u, a, l, h, NoL, VoH, pdf_specular) :
    //     sample_microfacet_ggx_specular(S, v, u, a, l, h, NoL, VoH, pdf_specular);

    //     if (pdf_specular < 1e-5) {
    //         samples--;
    //         continue;
    //     }
    //     const float pdf_diffuse = pdf_cosine_sample_hemisphere(NoL);

    //     // const vec3 h = normalize(l + v);
    //     const float NoH = dot(normal, h);
    //     const float LoH = dot(l, h);

    //     // Balance heuristic MIS weight
    //     const float weight = (pdf_specular * pdf_specular) /
    //             (pdf_diffuse * pdf_diffuse + pdf_specular * pdf_specular);
    //     // const float weight = 1.;

    //     const vec3 BSDF = BSDF(NoH, LoH, NoV, NoL,
    //             diffuseColor, f0, f90, a);

    //     recursivePayload.hitValue = vec3(0.);
    //     recursivePayload.depth = rayPayload.depth;
    //     traceRayEXT(topLevelAS, // acceleration structure
    //         gl_IncomingRayFlagsEXT, // rayFlags
    //         0xFF, // cullMask
    //         0, // sbtRecordOffset
    //         0, // sbtRecordStride
    //         0, // missIndex
    //         worldPos, // ray origin
    //         tMin, // ray min range
    //         l, // ray direction
    //         tMax, // ray max range
    //         1 // payload
    //     );
    //     // Accumulate indirect lighting
    //     indirectLuminance += weight * BSDF * recursivePayload.hitValue / pdf_specular;
    // }

    indirectLuminance /= float(BOUNCES);
    return indirectLuminance;
}

void main()
{
    // Set depth +1
    rayPayload.depth++;

    if (rayPayload.depth > MAX_RT_DEPTH) {
        return;
    }

    // -------- LOAD ALL THE DATA --------
    const uint instanceId = gl_InstanceCustomIndexEXT;
    const uint geometryId = gl_GeometryIndexEXT;
    const uint surfaceId = instanceId + geometryId;
    print_val("id %i ", surfaceId, 0, 6);

    RayPush rayPush = push.rayPush;
    SurfaceStorage surface = surfaceStorages[nonuniformEXT(surfaceId)].surface;

    const uint primitiveIndex = surface.startIndex + gl_PrimitiveID * 3;

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

    const vec4 colorVtx0 = v0.color;
    const vec4 colorVtx1 = v1.color;
    const vec4 colorVtx2 = v2.color;

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
    const vec3 normalVtx = gl_WorldToObjectEXT * vec4(normalVtxRaw, 0);
    // print_val("n %f ", length(normalVtxRaw), 2., 1.);

    const vec2 uv = uv0 * barycentrics.x + uv1 * barycentrics.y + uv2 * barycentrics.z;
    vec3 normal = normalVtx;
    if (normalMapIndex != -1)
    {
        const vec3 tangentRaw = v0.tangent.xyz * barycentrics.x + v1.tangent.xyz * barycentrics.y
                + v2.tangent.xyz * barycentrics.z; // range [-1, 1]
        const float handedness = v0.tangent.w; // All vi.tangent.w are the same
        const vec3 tangent = gl_WorldToObjectEXT * vec4(tangentRaw, 0);

        const vec3 bitangent = cross(normalVtx, tangent) * handedness;

        const mat3 TBN = mat3(tangent, bitangent, normalVtx);

        const vec4 normalTexRaw = 2.
                * texture(sampler2D(textures[nonuniformEXT(normalMapIndex)],
                        samplers[nonuniformEXT(normalSamplerIndex)]),
                    uv) - 1.; // range [0, 1] -> [-1, 1]

        normal = normalize(TBN * normalTexRaw.xyz);
        // print_val("n %f ", length(normal), 0.99, 1.);
    }
    const vec4 baseColor = (colorImageIndex != -1) ? texture(sampler2D(textures[nonuniformEXT(colorImageIndex)],
                samplers[nonuniformEXT(colorSamplerIndex)]),
            uv)
            * mConstants.baseColorFactor : mConstants.baseColorFactor; // range [0, 1]
    // print_val("c %f ", baseColor.x, 0.2, 0.9);

    // const vec4 baseColor = vec4(1.);

    const vec4 metallicRoughness = (materialImageIndex != -1) ? texture(sampler2D(textures[nonuniformEXT(materialImageIndex)],
                samplers[nonuniformEXT(materialSamplerIndex)]),
            uv)
            * vec4(0,
                mConstants.roughnessFactor,
                mConstants.metallicFactor,
                0) : vec4(0, mConstants.roughnessFactor, mConstants.metallicFactor, 0);
    const float perceptualRoughness = metallicRoughness.y;
    const float metallic = metallicRoughness.z;
    // Transforming the position to world space
    const vec3 worldPos = (gl_ObjectToWorldEXT * vec4(pos, 1.)).xyz;

    // -------------- BRDF --------------

    // Parametrization
    const vec3 diffuseColor = (1. - metallic) * baseColor.xyz;
    // const vec3 diffuseColor = vec3(1.);
    const float reflectance = 0.5;
    const vec3 f0 = mix(vec3(0.16 * reflectance * reflectance), baseColor.xyz, metallic);
    const float f90 = clamp(50.0 * f0.y, 0.0, 1.0);
    // perceptually linear roughness to roughness
    const float a = perceptualRoughness;
    // const float a = 0.001;
    // Ray directions
    const vec3 v = -gl_WorldRayDirectionEXT; // Inverse incoming (view) ray direction. Already normalized
    const float NoV = dot(normal, v);

    // INDIRECT LIGHTING
    const vec3 indirectLuminance = indirect_lighting(worldPos, normal, v, diffuseColor, f0, f90, a, NoV);

    // DIRECT LIGHTING
    // const vec3 directLuminance = direct_lighting(worldPos, normal, v, diffuseColor, f0, f90, a, NoV);
    const vec3 directLuminance = vec3(0.);

    rayPayload.hitValue = directLuminance + indirectLuminance;
    // rayPayload.hitValue = baseColor.xyz;
}
