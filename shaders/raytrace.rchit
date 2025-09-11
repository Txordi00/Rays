#version 460
#extension GL_EXT_ray_tracing : require
#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_EXT_buffer_reference2 : require
#extension GL_EXT_scalar_block_layout : enable
#extension GL_GOOGLE_include_directive : enable
//#extension GL_EXT_debug_printf : enable

layout(location = 0) rayPayloadInEXT vec3 hitValue;
layout(location = 1) rayPayloadEXT bool isShadowed;
hitAttributeEXT vec3 attribs;
layout(binding = 0, set = 0) uniform accelerationStructureEXT topLevelAS;

#include "types.glsl"
#include "functions.glsl"

layout(set = 1, binding = 0, scalar) uniform Ubo{
  mat4 worldMatrix;
} ubo[];

layout(set = 1, binding = 1, scalar) readonly buffer ObjectStorage {
    VertexBuffer vertexBuffer;
    IndexBuffer indexBuffer;
} objStorage[];

//push constants block
layout(scalar, push_constant) uniform constants
{
    vec4 clearColor;
    uint numObjects;
    vec3 lightPosition;
    float lightIntensity;
    uint lightType;

} push;

const float tMin = 0.001;
const float tMax = 10000.;

void main()
{
  uint objId = gl_InstanceCustomIndexEXT;
  uint primitiveIndex = gl_PrimitiveID * 3;

  VertexBuffer vBuffer = objStorage[objId].vertexBuffer;
  IndexBuffer iBuffer = objStorage[objId].indexBuffer;

  uint i0 = iBuffer.indices[primitiveIndex];
  uint i1 = iBuffer.indices[primitiveIndex + 1];
  uint i2 = iBuffer.indices[primitiveIndex + 2];

  Vertex v0 = vBuffer.vertices[i0];
  Vertex v1 = vBuffer.vertices[i1];
  Vertex v2 = vBuffer.vertices[i2];


  const vec3 barycentrics = vec3(1.0 - attribs.x - attribs.y, attribs.x, attribs.y);

  // Computing the coordinates of the hit position
  const vec3 pos
  = v0.position * barycentrics.x + v1.position * barycentrics.y + v2.position * barycentrics.z;
  // Transforming the position to world space
  const vec3 worldPos = vec3(gl_ObjectToWorldEXT * vec4(pos, 1.0));

  // Computing the normal at hit position
  const vec3 nrm
  = v0.normal * barycentrics.x + v1.normal * barycentrics.y + v2.normal * barycentrics.z;
  // Transforming the normal to world space
  const vec3 worldNrm = normalize(vec3(nrm * gl_WorldToObjectEXT));


  //const vec4 colorIn =
    //v0.color * barycentrics.x + v1.color * barycentrics.y + v2.color * barycentrics.z;
  const vec3 colorIn = vec3(1.);

  // Check if in shadow
  // Vector towards the light
  vec3 l = push.lightPosition - worldPos;
  float lightDistance = length(l);
  vec3 lNorm = normalize(l);
  float attenuation = push.lightIntensity / lightDistance;

  // Flags
  uint  shadowFlags =
      gl_RayFlagsTerminateOnFirstHitEXT | gl_RayFlagsOpaqueEXT | gl_RayFlagsSkipClosestHitShaderEXT;
  // We initialize to true, if the miss shader will be called it sets it to false
  isShadowed = true;
  traceRayEXT(topLevelAS,  // acceleration structure
          shadowFlags,       // rayFlags
          0xFF,        // cullMask
          0,           // sbtRecordOffset
          0,           // sbtRecordStride
          1,           // missIndex
          worldPos,      // ray origin
          tMin,        // ray min range
          l,      // ray direction
          tMax,        // ray max range
          1            // payload (location = 1)
  );


  // Point light diffuse lighting
  vec3 diffuseC = vec3(0.);
  if(!isShadowed)
  {
    diffuseC = diffuse(1., lNorm, worldNrm) * colorIn;
    diffuseC *= attenuation;
  }

  // Point light specular lighting
  vec3 specularC = vec3(0.);
  if(!isShadowed)
  {
    vec3 viewDir = normalize(gl_WorldRayDirectionEXT);
    specularC = specular(4, viewDir, lNorm, worldNrm) * colorIn;
    specularC *= attenuation;
  }

  vec3 outColor = diffuseC + specularC;
  outColor /= (outColor + 1.);

  hitValue = vec3(outColor);

}
