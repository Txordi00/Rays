#version 460
#extension GL_EXT_ray_tracing : require
#extension GL_EXT_scalar_block_layout : enable

struct hitPayload
{
  vec3 hitValue;
};

layout(location = 0) rayPayloadInEXT hitPayload rayPayload;

//push constants block
layout(scalar, push_constant) uniform constants
{
    vec4 clearColor;
    uint numObjects;
} push;


void main()
{
    rayPayload.hitValue = push.clearColor.xyz * 0.8;
}
