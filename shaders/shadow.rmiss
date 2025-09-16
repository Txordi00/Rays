#version 460
#extension GL_EXT_ray_tracing : require
#extension GL_EXT_scalar_block_layout : enable

#include "types.glsl"

layout(location = 1) rayPayloadInEXT bool isShadowed;

//push constants block
layout(scalar, push_constant) uniform RayPushConstants push;


void main()
{
  isShadowed = false;
}
