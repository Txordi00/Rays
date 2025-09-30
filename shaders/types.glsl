struct Vertex {
    vec3 position;
    float uv_x;
    vec3 normal;
    float uv_y;
    vec4 color;
};

struct HitPayload
{
  vec3 hitValue;
  uint depth;
  float energyFactor;
};

#define RayPushConstants RayPushConstants \
    { \
        vec4 clearColor; \
        uint numObjects; \
        vec3 lightPosition; \
        float lightIntensity; \
        uint lightType; \
    }

struct Material
{
    // Color
    vec3 color;
    // specular reflectiveness
    float specularR;
    // Diffuse reflectiveness
    float diffuseR;
    // Ambient reflectiveness
    float ambientR;
    // Shininess factor N.
    // From the approximation in https://en.wikipedia.org/wiki/Phong_reflection_model#Concepts
    // with beta=1
    int shininessN;
    // Reflectiveness
    float reflectiveness;
    // Refractiveness
    float refractiveness;
    // Refractive index. n_2 in https://en.wikipedia.org/wiki/Snell's_law
    float refractiveIndex;
};
