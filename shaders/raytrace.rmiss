#version 460
#extension GL_EXT_ray_tracing : require
#extension GL_EXT_scalar_block_layout : enable

#include "types.glsl"

layout(location = 0) rayPayloadInEXT HitPayload rayPayload;

//push constants block
layout(scalar, push_constant) uniform RayPushConstants
{
    RayPush rayPush;
} push;

void main()
{
    rayPayload.hitValue = push.rayPush.clearColor.xyz * rayPayload.energyFactor * 0.8;
}
