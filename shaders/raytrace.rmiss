#version 460
#extension GL_EXT_ray_tracing : require
#extension GL_EXT_scalar_block_layout : enable

#include "types.glsl"

struct hitPayload
{
  vec3 hitValue;
};

layout(location = 0) rayPayloadInEXT hitPayload rayPayload;

//push constants block
layout(scalar, push_constant) uniform RayPushConstants push;


void main()
{
    rayPayload.hitValue = push.clearColor.xyz;
}
