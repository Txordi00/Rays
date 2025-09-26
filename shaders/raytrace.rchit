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
hitAttributeEXT vec3 attribs;
layout(binding = 0, set = 0) uniform accelerationStructureEXT topLevelAS;


layout(set = 1, binding = 0, scalar) uniform Ubo {
  mat4 worldMatrix;
} ubo[];

layout(buffer_reference, std430) readonly buffer VertexBuffer
{
  Vertex vertices[];
};

layout(buffer_reference, std430) readonly buffer IndexBuffer
{
  uint indices[];
};


layout(set = 1, binding = 1, scalar) readonly buffer ObjectStorage {
    VertexBuffer vertexBuffer;
    IndexBuffer indexBuffer;
    Material material;
} objStorage[];

//push constants block
layout(scalar, push_constant) uniform RayPushConstants push;

const float tMin = 0.001;
const float tMax = 10000.;

void main()
{
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

  const vec3 barycentrics = vec3(1.0 - attribs.x - attribs.y, attribs.x, attribs.y);

  // Computing the coordinates of the hit position
  const vec3 pos
  = vertPos0 * barycentrics.x + vertPos1 * barycentrics.y + vertPos2 * barycentrics.z;

//  const vec3 normal = normalize(cross(vertPos1 - vertPos0, vertPos2 - vertPos0));
  const vec3 normal = norm0 * barycentrics.x + norm1 * barycentrics.y + norm2 * barycentrics.z; // already normalized

  // Transforming the position to world space
  const vec3 worldPos = vec3(gl_ObjectToWorldEXT * vec4(pos, 1.0));

//  const vec3 colorIn =
//    vec3(v0.color * barycentrics.x + v1.color * barycentrics.y + v2.color * barycentrics.z);
  const vec3 colorIn = material.color;
//  const vec3 colorIn = vec3(1.);

  // Check if in shadow
  // Vector towards the light
  vec3 l = push.lightPosition - worldPos;
  float lightDistance = length(l);
  l /= lightDistance;
  float attenuation = push.lightIntensity / lightDistance;
  const vec3 viewDir = gl_WorldRayDirectionEXT; // already normalized

  vec3 outColor = vec3(0.);
  if(dot(l, normal) > 0. && dot(viewDir, normal) < 0.)
  {
    // Flags
    const uint shadowFlags =
        gl_RayFlagsTerminateOnFirstHitEXT | gl_RayFlagsOpaqueEXT | gl_RayFlagsSkipClosestHitShaderEXT;
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
                l,       // ray direction
                tMax,        // ray max range
                1            // payload (location = 1)
    );

    // Point light diffuse lighting
    vec3 diffuseC = vec3(0.);
    if(!isShadowed)
    {
      diffuseC = diffuse(material, l, normal) * colorIn;
      diffuseC *= attenuation;
    }

    // Point light specular lighting
    vec3 specularC = vec3(0.);
    if(!isShadowed)
    {
      printVal("%f ", length(viewDir), 0.99, 1.01);
      specularC = specular(material, viewDir, l, normal) * colorIn;
      specularC *= attenuation;
    }

    outColor = diffuseC + specularC;
    outColor /= (outColor + 1.);
}
  rayPayload.hitValue = outColor;
}
