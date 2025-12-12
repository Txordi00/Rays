#version 460
#extension GL_EXT_ray_tracing : require
#extension GL_EXT_scalar_block_layout : enable

#include "types.glsl"

layout(location = 0) rayPayloadInEXT HitPayload rayPayload;
layout(constant_id = 0) const float BKGR = 0.;
layout(constant_id = 1) const float BKGG = 0.;
layout(constant_id = 2) const float BKGB = 0.;

layout(scalar, push_constant) uniform RayPushConstants
{
    RayPush rayPush;
}
push;

void main()
{
    rayPayload.hitValue = push.rayPush.clearColor.xyz;
}
