#version 460
#extension GL_EXT_ray_tracing : require
#extension GL_EXT_scalar_block_layout : enable
// #extension GL_EXT_debug_printf : require

#include "types.glsl"
//#include "functions.glsl"

layout(location = 1) rayPayloadInEXT bool isShadowed;

//push constants block
layout(scalar, push_constant) uniform RayPushConstants
{
    RayPush rayPush;
}
push;

void main()
{
  isShadowed = false;
}
