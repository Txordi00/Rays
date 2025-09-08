#version 460
#extension GL_EXT_ray_tracing : require
#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_EXT_buffer_reference2 : require
#extension GL_EXT_scalar_block_layout : enable

layout(location = 0) rayPayloadInEXT vec3 hitValue;
hitAttributeEXT vec3 attribs;

struct Vertex {
    vec3 position;
    float uv_x;
    vec3 normal;
    float uv_y;
    vec4 color;
};

layout(buffer_reference, std430) readonly buffer VertexBuffer{
    Vertex vertices[];
};

layout(buffer_reference, std430) readonly buffer IndexBuffer{
    uint indices[];
};

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


  // Vector towards the light
  vec3 l;
  float lightIntensity = push.lightIntensity;
  float lightDistance = 100000.;
  // Point light diffuse lighting
  if(push.lightType == 0)
  {
    vec3 l = push.lightPosition - worldPos;
    float lDotNorm = dot(l, worldNrm);
    lightDistance = length(l);
    lightIntensity = (lDotNorm > 0) ? lightIntensity * lDotNorm / lightDistance : 0.;
    l = normalize(l);
  }
  else // Directional light
  {
    l = normalize(push.lightPosition);
  }

  //const vec4 colorInterpolated =
    v0.color * barycentrics.x + v1.color * barycentrics.y + v2.color * barycentrics.z;

  const vec4 colorInterpolated = vec4(1.);

  vec4 outColor = lightIntensity * colorInterpolated;
  outColor /= (outColor + 1.);

  hitValue = vec3(outColor);

}
