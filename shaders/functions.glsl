#define print_val(message, val, valMin, valMax) if(val < valMin || val > valMax){ \
       debugPrintfEXT(message, val); \
     }

float D_GGX(const float NoH, const float a) {
    const float a2 = a * a;
    const float f = (NoH * a2 - NoH) * NoH + 1.0;
    return a2 / (PI * f * f);
}

vec3 F_Schlick(const float u, const vec3 f0, const float f90) {
    const float u11 = 1. - u;
    const float u12 = u11 * u11; // u1^2
    const float u14 = u12 * u12;
    const float u15 = u14 * u11;
    return f0 + (vec3(f90) - f0) * u15;
}

float V_SmithGGXCorrelated(const float NoV, const float NoL, const float a) {
    const float a2 = a * a;
    const float GGXL = NoV * sqrt((-NoL * a2 + NoL) * NoL + a2);
    const float GGXV = NoL * sqrt((-NoV * a2 + NoV) * NoV + a2);
    return 0.5 / (GGXV + GGXL);
}

float V_SmithGGXCorrelatedFast(float NoV, float NoL, float roughness) {
    const float a = roughness;
    const float first = 2. * NoL * NoV;
    const float second = NoL + NoV;
    return mix(first, second, a);
}

float Fd_Lambert() {
    return 1.0 / PI;
}

vec3 evaluate_directional_light(const Light light, const vec3 BSDF, const float NoL)
{
    const float illuminance = light.intensity * NoL;
    vec3 luminance = BSDF * illuminance * light.color;
    return luminance;
}

vec3 evaluate_point_light(const Light light, const float distanceSquared, const vec3 BSDF, const float NoL)
{
    const float attenuation = light.intensity * NoL / distanceSquared;
    vec3 luminance = BSDF * attenuation * light.color;
    return luminance;
}

float luminance(const vec3 v)
{
    return dot(v, vec3(0.2126f, 0.7152f, 0.0722f));
}

vec3 reinhard_jodie(const vec3 color)
{
    float l = luminance(color);
    vec3 tv = color / (1.0f + color);
    return mix(color / (1.0f + l), tv, tv);
}

vec3 sample_hemisphere(const mat3 S, const float u, const float v)
{
    // Sample phi and theta in the local normal frame
    const float phi = 2. * PI * u;
    const float theta = acos(1. - v);
    const float stheta = sin(theta);
    const float b1 = stheta * cos(phi);
    const float b2 = stheta * sin(phi);
    const float n = cos(theta);

    // Return in world frame
    return vec3(b1, b2, n) * S;
}
