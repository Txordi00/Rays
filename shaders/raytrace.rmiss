#version 460
#extension GL_EXT_ray_tracing : require
#extension GL_EXT_scalar_block_layout : enable

#include "types.glsl"

layout(location = 0) rayPayloadInEXT HitPayload rayPayload;
layout(binding = 5, set = 0) uniform sampler2D backgroundTexture;
layout(constant_id = 0) const bool USE_ENV_MAP = false;

layout(scalar, push_constant) uniform RayPushConstants
{
    RayPush rayPush;
}
push;

vec2 directionToSphericalEnvmap(vec3 dir) {
    float phi = atan(dir.z, dir.x);
    float theta = asin(dir.y);
    vec2 uv;
    uv.x = (phi + PI) * ONEOVERTWOPI;
    uv.y = theta * ONEOVERPI + 0.5;

    return uv;
}

void main()
{
    rayPayload.hitValue = (USE_ENV_MAP) ? texture(backgroundTexture, directionToSphericalEnvmap(gl_WorldRayDirectionEXT)).xyz : push.rayPush.clearColor.xyz;
}
