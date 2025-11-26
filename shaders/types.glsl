const float PI = 3.1415926535897932384626433832795f;
const float TWOPI = 2. * PI;
const float ONEOVERPI = 1. / PI;
const float ONEOVERTWOPI = 1. / TWOPI;
const vec2[32] UV = vec2[32](
        vec2(0.80918866, 0.73555427),
        vec2(0.31952403, 0.46541552),
        vec2(0.98199345, 0.05692386),
        vec2(0.93412786, 0.86172431),
        vec2(0.21233514, 0.51606522),
        vec2(0.46286697, 0.59213721),
        vec2(0.05000782, 0.82653978),
        vec2(0.73490317, 0.02261001),
        vec2(0.25135304, 0.41662461),
        vec2(0.57209909, 0.07445831),
        vec2(0.26934258, 0.90995006),
        vec2(0.8215488, 0.62998104),
        vec2(0.30568502, 0.00312605),
        vec2(0.16574971, 0.84374354),
        vec2(0.56525967, 0.38960361),
        vec2(0.8389401, 0.45214875),
        vec2(0.69068081, 0.23294052),
        vec2(0.22936854, 0.73513904),
        vec2(0.77063564, 0.91051739),
        vec2(0.89193338, 0.32682744),
        vec2(0.67525698, 0.4864062),
        vec2(0.10102081, 0.61900286),
        vec2(0.97006881, 0.72581968),
        vec2(0.35163832, 0.47681911),
        vec2(0.31438029, 0.59929662),
        vec2(0.85507134, 0.62248693),
        vec2(0.23788793, 0.30178636),
        vec2(0.10893564, 0.54384167),
        vec2(0.50782942, 0.21646965),
        vec2(0.71244726, 0.81729762),
        vec2(0.12920395, 0.21908925),
        vec2(0.44975938, 0.52771852));

struct Vertex {
    vec3 position;
    vec3 normal;
    vec2 uv;
    vec4 tangent;
    vec4 color;
};

struct HitPayload
{
    vec3 hitValue;
    uint depth;
    // float energyFactor;
};

struct Light
{
    vec3 positionOrDirection;
    vec3 color;
    float intensity;
    uint type; // 0 - point, 1 - directional
};

struct RayPush
{
    vec4 clearColor;
    uint numLights;
};

struct MaterialConstants
{
    vec4 baseColorFactor;
    float metallicFactor;
    float roughnessFactor;
};

// struct Material
// {
//     // Color
//     vec3 color;
//     // specular reflectiveness
//     float specularR;
//     // Diffuse reflectiveness
//     float diffuseR;
//     // Ambient reflectiveness
//     float ambientR;
//     // Shininess factor N.
//     // From the approximation in https://en.wikipedia.org/wiki/Phong_reflection_model#Concepts
//     // with beta=1
//     int shininessN;
//     // Reflectiveness
//     float reflectiveness;
//     // Refractiveness
//     float refractiveness;
//     // Refractive index. n_2 in https://en.wikipedia.org/wiki/Snell's_law
//     float refractiveIndex;
// };
