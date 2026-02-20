#extension GL_EXT_debug_printf : require

#define print_val(message, val, valMin, valMax) if(val < valMin || val > valMax){ \
       debugPrintfEXT(message, val); \
     }

// Fast change of basis from https://backend.orbit.dtu.dk/ws/portalfiles/portal/126824972/onb_frisvad_jgt2012_v2.pdf
mat3 normal_cob(const vec3 normal) {
    const float nx = normal.x;
    const float ny = normal.y;
    const float nz = normal.z;
    const float nz1 = 1. / (1. + nz);
    const float nxony = -nx * ny;
    const float nxonynz1 = nxony * nz1;
    mat3 S = (nz > -0.999999) ? mat3(1. - nx * nx * nz1, nxonynz1, -nx,
            nxonynz1, 1. - ny * ny * nz1, -ny,
            nx, ny, nz) : mat3(0., -1., 0., -1., 0., 0., 0., 0., -1.);

    return S;
}

float D_GGX(const float NoH, const float a) {
    const float a2 = a * a;
    const float f = (NoH * a2 - NoH) * NoH + 1.0;
    return (a2 > 1e-5) ? a2 * ONEOVERPI / (f * f) : ONEOVERPI;
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
    const float first = 2. * NoL * NoV;
    const float second = NoL + NoV;
    return 0.5 / mix(first, second, a);
}

float Fd_Lambert(const float nDotL) {
    return nDotL * ONEOVERPI;
}

vec3 BSDF(const float nDotH, const float lDotH, const float nDotV, const float nDotL,
    const vec3 diffuseColor, const vec3 f0, const float f90, const float a) {
    const float a2 = a * a;
    const float D = D_GGX(nDotH, a);
    const vec3 F = F_Schlick(lDotH, f0, f90);
    const float V = V_SmithGGXCorrelatedFast(nDotV, nDotL, a);

    // specular BRDF
    const vec3 Fr = D * V * F * nDotL;

    // diffuse BRDF
    const vec3 Fd = diffuseColor * Fd_Lambert(nDotL);

    return Fd;
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

float D_specular_Disney_Epic(const float cosTheta, const float a2)
{
    const float denom = cosTheta * cosTheta * (a2 - 1.) + 1.;
    return (a2 > 1e-5) ? ONEOVERPI * a2 / (denom * denom) : ONEOVERPI;
}

float pdf_microfacet_ggx_specular(const float ctheta, const float a2, const float vDotH)
{
    // PDF transformation from half-vector to light direction
    const float D = D_specular_Disney_Epic(ctheta, a2);
    const float pdf_h = D * ctheta;

    // Jacobian: pdf_l = pdf_h / (4 * |v·h|)
    return pdf_h / (4. * vDotH);
}

void sample_microfacet_ggx_specular(in const mat3 S, in const vec3 v, in const vec2 u, in const float a, out vec3 sampleDir, out vec3 h, out float nDotL, out float vDotH, out float pdf)
{
    // Sample phi and theta in the local normal frame
    const float phi = TWOPI * u.x;
    const float a2 = a * a; // a in my case is already a = perceptualRoughness^2
    const float ctheta = (a2 > 1e-5) ? sqrt((1. - u.y) / (u.y * (a2 - 1.) + 1.)) : 1.;
    // print_val("ct: %f ", ctheta, -1., 1.);
    const float stheta = sqrt(1. - ctheta * ctheta);

    // Half vector in local frame
    const vec3 hLocal = vec3(stheta * cos(phi), stheta * sin(phi), ctheta);
    // Move to world frame
    h = S * hLocal;

    // Reflect view direction around half-vector to get light direction
    sampleDir = reflect(-v, h);

    nDotL = dot(sampleDir, S[2]);
    vDotH = dot(v, h);
    // ctheta = nDotH
    // if (nDotL < 1e-5 || vDotH < 1e-5 || ctheta < 1e-5)
    // {
    //     pdf = -1.; // If pdf negative, the sample will be skipped
    //     return;
    // }
    pdf = pdf_microfacet_ggx_specular(ctheta, a2, vDotH);
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

vec2 concentric_sample_disk_branchless(const vec2 u) {
    const vec2 uOffset = 2. * u - vec2(1.);

    // Check if we're at the origin
    const float isOrigin = step(dot(uOffset, uOffset), 1e-5);

    // Determine quadrant
    const float useX = step(abs(uOffset.y), abs(uOffset.x));

    // Calculate both possible r and theta values
    const float thetaX = ONEOVERFOURPI * (uOffset.y / (uOffset.x + 1e-5)); // add epsilon to avoid division by zero
    const float thetaY = ONEOVERFOURPI * (2. - uOffset.x / (uOffset.y + 1e-5));
    const float r = mix(uOffset.y, uOffset.x, useX);
    const float theta = mix(thetaY, thetaX, useX);

    // Return zero if at origin
    return mix(r * vec2(cos(theta), sin(theta)), vec2(0.), isOrigin);
}

float pdf_cosine_sample_hemisphere(const float nDotL) {
    return nDotL * ONEOVERPI;
}

void cosine_sample_hemisphere(in const mat3 S, in const vec2 u, out vec3 sampleDir, out float pdf, out float nDotL) {
    const vec2 d = concentric_sample_disk_branchless(u);
    const float d2 = dot(d, d);
    const float z = sqrt(max(0., 1. - d2));
    const vec3 sampleInNormalFrame = normalize(vec3(d.x, d.y, z));
    sampleDir = S * sampleInNormalFrame;
    nDotL = sampleInNormalFrame.z;
    // print_val("dc: %f ", abs(sampleInNormalFrame.z - nDotL), 0., 0.01);
    pdf = pdf_cosine_sample_hemisphere(nDotL);
}

// Steps the RNG and returns a floating-point value between 0 and 1 inclusive.
float stepAndOutputRNGFloat(inout uint rngState)
{
    // Condensed version of pcg_output_rxs_m_xs_32_32, with simple conversion to floating-point [0,1].
    rngState = rngState * 747796405 + 1;
    uint word = ((rngState >> ((rngState >> 28) + 4)) ^ rngState) * 277803737;
    word = (word >> 22) ^ word;
    return float(word) / 4294967295.0f;
}
