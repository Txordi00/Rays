#extension GL_EXT_debug_printf : require

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

float V_SmithGGXCorrelatedFast(const float NoV, const float NoL, const float a) {
    // const float a2 = a * a;
    const float first = 2. * NoL * NoV;
    const float second = NoL + NoV;
    return mix(first, second, a);
}

float Fd_Lambert(const float nDotL) {
    return nDotL * ONEOVERPI;
}

vec3 BSDF(const float nDotH, const float lDotH, const float nDotV, const float nDotL,
    const vec3 diffuseColor, const vec3 f0, const float f90, const float a) {
    const float D = D_GGX(nDotH, a);
    const vec3 F = F_Schlick(lDotH, f0, f90);
    const float V = V_SmithGGXCorrelated(nDotV, nDotL, a);

    // specular BRDF
    const vec3 Fr = D * V * F;

    // diffuse BRDF
    const vec3 Fd = diffuseColor * Fd_Lambert(nDotL);

    return Fr;
}

vec3 evaluate_directional_light(const Light light, const vec3 BSDF)
{
    const float illuminance = light.intensity;
    vec3 luminance = BSDF * illuminance * light.color;
    return luminance;
}

vec3 evaluate_point_light(const Light light, const float distanceSquared, const vec3 BSDF)
{
    const float attenuation = light.intensity / distanceSquared;
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
    vec3 tv = color / (1.f + color);
    return mix(color / (1.f + l), tv, tv);
}

void sample_hemisphere(in const mat3 S, in const float u, in const float v, out vec3 sampleDir, out float pdf)
{
    // Sample phi and theta
    const float phi = TWOPI * u;
    const float theta = acos(1. - v);
    // Sample in the local normal frame (b1, b2, n)
    const float stheta = sin(theta);
    const float b1 = stheta * cos(phi);
    const float b2 = stheta * sin(phi);
    const float n = cos(theta);

    // Return in world frame
    sampleDir = vec3(b1, b2, n) * S;
    pdf = ONEOVERTWOPI;
}

float D_specular_Disney_Epic(const float cosTheta, const float a2)
{
    const float denom = cosTheta * cosTheta * (a2 - 1.) + 1.;
    return ONEOVERPI * a2 / (denom * denom);
}

void sample_microfacet_ggx_specular(in const mat3 S, in const vec3 v, in const vec2 u, in const float a, out vec3 sampleDir, out float nDotL, out float pdf)
{
    // Sample phi and theta in the local normal frame
    const float phi = TWOPI * u[0];
    const float a2 = a * a; // a in my case is already a = perceptualRoughness^2
    const float ctheta = sqrt((1 - u[1]) / (u[1] * (a2 - 1.) + 1.));
    const float stheta = sqrt(1.0 - ctheta * ctheta);

    // Half vector in local frame
    const vec3 hLocal = vec3(stheta * cos(phi), stheta * sin(phi), ctheta);
    // Move to world frame
    const vec3 hWorld = normalize(S * hLocal);

    // Reflect view direction around half-vector to get light direction
    sampleDir = normalize(reflect(-v, hWorld));

    nDotL = dot(sampleDir, S[2]);
    float vDotH = dot(v, hWorld);
    // ctheta = nDotH
    if (nDotL < 1e-5 || vDotH < 1e-5 || ctheta < 1e-5)
    {
        pdf = -1.; // If pdf negative, the sample will be skipped
        return;
    }
    // PDF transformation from half-vector to light direction
    const float D = D_specular_Disney_Epic(ctheta, a2);
    const float pdf_h = D * ctheta;

    // Jacobian: pdf_l = pdf_h / (4 * |v·h|)
    pdf = pdf_h / (4. * vDotH);
}

vec2 concentric_sample_disk(const vec2 u) {
    vec2 uOffset = 2. * u - vec2(1.);
    if (abs(uOffset.x) < 0.001 && abs(uOffset.y) < 0.001) {
        return vec2(0.);
    }
    float theta, r;
    if (abs(uOffset.x) > abs(uOffset.y)) {
        r = uOffset.x;
        theta = PI / 4. * (uOffset.y / uOffset.x);
    } else {
        r = uOffset.y;
        theta = PI / 2. - PI / 4. * (uOffset.x / uOffset.y);
    }
    return r * vec2(cos(theta), sin(theta));
}

void cosine_sample_hemisphere(in const mat3 S, in const vec2 u, out vec3 sampleDir, out float pdf, out float nDotL) {
    const vec2 d = concentric_sample_disk(u);
    const float d2 = dot(d, d);
    const float z = sqrt(max(0., 1. - d2));
    const vec3 sampleInNormalFrame = vec3(d.x, d.y, z);
    sampleDir = normalize(S * sampleInNormalFrame);
    nDotL = clamp(dot(sampleDir, S[2]), 1e-5, 1.);
    pdf = nDotL * ONEOVERPI;
}

float rand(vec2 co)
{
    return fract(sin(dot(co.xy, vec2(12.9898, 78.233))) * 43758.5453);
}
// vec3 trowbridge_reitz_sample(const vec3 wo,
//     const Point2f & u ) const {
//     Vector3f wh;
// if ( ! sampleVisibleArea ) {
// Float cosTheta = 0, phi = (2 * Pi) * u[1];
// if ( alphax == alphay ) {
// Float tanTheta2 = alphax * alphax * u[0] / (1.0f - u[0]);
// cosTheta = 1 / std : : sqrt(1+tanTheta2);
// } else {
// phi =
// std : : atan(alphay/alphax*std: : tan(2*Pi*u[1]+.5f*Pi));
// if ( u[1] > .5f ) phi += Pi;
// Float sinPhi = std : : sin(phi), cosPhi = std : : cos(phi);
// const Float alphax2 = alphax * alphax, alphay2 = alphay * alphay;
// const Float alpha2 =
//     1 / (cosPhi * cosPhi / alphax2 + sinPhi * sinPhi / alphay2);
// Float tanTheta2 = alpha2 * u[0] / (1 - u[0]);
// cosTheta = 1 / std : : sqrt(1+tanTheta2);
// }
// Float sinTheta =
//     std : : sqrt(std: : max((Float)0., (Float)1.-cosTheta*cosTheta));
// wh = SphericalDirection(sinTheta, cosTheta, phi);
// if ( ! SameHemisphere(wo, wh)) wh = - wh;
// } else {
// bool flip = wo.z < 0;
// wh = TrowbridgeReitzSample(flip?-wo: wo, alphax, alphay, u[0], u[1]);
// if ( flip ) wh = - wh;
// }
// return wh;
// }
