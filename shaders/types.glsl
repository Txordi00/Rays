const float PI = 3.1415926535897932384626433832795;
const float TWOPI = 2. * PI;
const float ONEOVERPI = 1. / PI;
const float ONEOVERTWOPI = 1. / TWOPI;

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
    float energyFactor;
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
