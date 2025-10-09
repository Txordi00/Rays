#version 460
#extension GL_EXT_ray_tracing : require
#extension GL_EXT_nonuniform_qualifier : require
#extension GL_EXT_buffer_reference2 : require
#extension GL_EXT_scalar_block_layout : enable
#extension GL_EXT_debug_printf : enable

#include "types.glsl"
#include "functions.glsl"

layout(location = 0) rayPayloadInEXT HitPayload rayPayload;
layout(location = 1) rayPayloadEXT bool isShadowed;
layout(constant_id = 0) const uint MAX_RT_DEPTH = 3;
hitAttributeEXT vec3 attribs;
layout(binding = 0, set = 0) uniform accelerationStructureEXT topLevelAS;

layout(buffer_reference, std430) readonly buffer VertexBuffer
{
    Vertex vertices[];
};

layout(buffer_reference, std430) readonly buffer IndexBuffer
{
    uint indices[];
};

layout(set = 1, binding = 1, scalar) readonly buffer ObjectStorage
{
    VertexBuffer vertexBuffer;
    IndexBuffer indexBuffer;
    Material material;
}
objStorage[];

//push constants block
layout(scalar, push_constant) uniform RayPushConstants push;

const float tMin = 0.01;
const float tMax = 10000.;
const float ENERGY_LOSS = 0.8;
float ENERGY_MIN = 1. * pow(ENERGY_LOSS, MAX_RT_DEPTH);

void main()
{
    if (rayPayload.depth > MAX_RT_DEPTH)
        return;
    else {
        // Set depth +1
        rayPayload.depth++;

        const uint objId = gl_InstanceCustomIndexEXT;
        const uint primitiveIndex = gl_PrimitiveID * 3;

        VertexBuffer vBuffer = objStorage[nonuniformEXT(objId)].vertexBuffer;
        IndexBuffer iBuffer = objStorage[nonuniformEXT(objId)].indexBuffer;
        Material material = objStorage[nonuniformEXT(objId)].material;

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

        const vec3 barycentrics = vec3(1. - attribs.x - attribs.y, attribs.x, attribs.y);

        // Computing the coordinates of the hit position
        const vec3 pos = vertPos0 * barycentrics.x + vertPos1 * barycentrics.y
                         + vertPos2 * barycentrics.z;

        const vec3 normal = norm0 * barycentrics.x + norm1 * barycentrics.y
                            + norm2 * barycentrics.z; // already normalized

        // Transforming the position to world space
        const vec3 worldPos = vec3(gl_ObjectToWorldEXT * vec4(pos, 1.0));

        //  const vec3 colorIn =
        //    vec3(v0.color * barycentrics.x + v1.color * barycentrics.y + v2.color * barycentrics.z);
        const vec3 colorIn = material.color;

        // Check if in shadow
        // Vector towards the light
        vec3 l = push.lightPosition - worldPos;
        float lightDistance = length(l);
        l /= lightDistance;
        float attenuation = push.lightIntensity / lightDistance;
        const vec3 rayDir = gl_WorldRayDirectionEXT; // already normalized
        const bool facingToLight = (dot(l, normal) > 0.);

        vec3 outColor = vec3(0.);
        if (rayPayload.energyFactor > ENERGY_MIN) {
            // SHADOWS
            const uint shadowFlags = gl_RayFlagsTerminateOnFirstHitEXT | gl_RayFlagsOpaqueEXT
                                     | gl_RayFlagsSkipClosestHitShaderEXT;
            // We initialize to true, if the miss shader will be called it sets it to false
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

            vec3 diffuseC = vec3(0.);
            vec3 specularC = vec3(0.);
            vec3 reflectedC = vec3(0.);
            vec3 refractedC = vec3(0.);
            vec3 ambientC = vec3(0.);
            const float cos1 = -dot(rayDir, normal); // If positive, we are outside the object
            const bool entering = cos1 > 0.;

            if (entering) {
                // Ambient lighting
                ambientC = material.ambientR * colorIn;

                // Lighting depending on the point light
                if (!isShadowed && facingToLight) {
                    // Point light diffuse lighting
                    diffuseC = attenuation * diffuse(material, l, normal) * colorIn;

                    // Point light specular lighting
                    specularC = attenuation * specular(material, rayDir, l, normal) * colorIn;
                }

                // Reflections. Avoid entering the final recursion because it's useless
                if (rayPayload.depth < MAX_RT_DEPTH && material.reflectiveness > 0.01) {
                    const vec3 reflectedDir = reflect(rayDir, normal);
                    traceRayEXT(topLevelAS,             // acceleration structure
                                gl_IncomingRayFlagsEXT, // rayFlags
                                0xFF,                   // cullMask
                                0,                      // sbtRecordOffset
                                0,                      // sbtRecordStride
                                0,                      // missIndex
                                worldPos,               // ray origin
                                tMin,                   // ray min range
                                reflectedDir,           // ray direction
                                tMax,                   // ray max range
                                0                       // payload
                    );
                    reflectedC = material.reflectiveness * rayPayload.hitValue;
                }
            }
                // Refractions. Avoid entering the final recursion because it's useless
                // REFRACTION. Snell's law: https://en.wikipedia.org/wiki/Snell%27s_law#Vector_form
                if (rayPayload.depth < MAX_RT_DEPTH && material.refractiveness > 0.01) {
                    // Flip n1 and n2 depending on whether we are inside or outside the object
                    const float n = (entering) ? 1. / material.refractiveIndex : material.refractiveIndex;
                    const vec3 normalTmp = (entering) ? normal : -normal;
                    const vec3 refractedDir = refract(rayDir, normalTmp, n);
                    if(dot(refractedDir, refractedDir) > 0.)
                    {
                        traceRayEXT(topLevelAS,             // acceleration structure
                                    gl_IncomingRayFlagsEXT, // rayFlags
                                    0xFF,                   // cullMask
                                    0,                      // sbtRecordOffset
                                    0,                      // sbtRecordStride
                                    0,                      // missIndex
                                    worldPos,               // ray origin
                                    tMin,                   // ray min range
                                    refractedDir,           // ray direction
                                    tMax,                   // ray max range
                                    0                       // payload
                        );
                    } // If Snell's refraction fails it means that the ray is reflected and not refracted.
                    // Since we took that case into consideration already, only do it if the reflection was skipped.
                    else if (material.reflectiveness < 0.01)
                    {
                        const vec3 reflectedDir = reflect(rayDir, normalTmp);
                        traceRayEXT(topLevelAS,             // acceleration structure
                                    gl_IncomingRayFlagsEXT, // rayFlags
                                    0xFF,                   // cullMask
                                    0,                      // sbtRecordOffset
                                    0,                      // sbtRecordStride
                                    0,                      // missIndex
                                    worldPos,               // ray origin
                                    tMin,                   // ray min range
                                    reflectedDir,           // ray direction
                                    tMax,                   // ray max range
                                    0                       // payload
                        );
                    }
                    if(entering)
                        refractedC = material.refractiveness * rayPayload.hitValue;
                    else // We add the energy loss back because we don't want to lose energy twice on enter and exit
                        rayPayload.energyFactor /= ENERGY_LOSS;
                }
            // Compute the color contribution of this hit
            outColor = reflectedC + diffuseC + specularC + refractedC + ambientC;
            outColor /= (outColor + 1.);
        }
        // Add this particular contribution to the total ray payload
        rayPayload.hitValue += rayPayload.energyFactor * outColor;
        // After color transfer, lose energy
        rayPayload.energyFactor *= ENERGY_LOSS;
    }
}
