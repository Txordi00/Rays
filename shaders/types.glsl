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
};

#define RayPushConstants RayPushConstants \
    { \
        vec4 clearColor; \
        uint numObjects; \
        vec3 lightPosition; \
        float lightIntensity; \
        uint lightType; \
    }
