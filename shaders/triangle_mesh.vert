#version 460
#extension GL_EXT_buffer_reference : require
#extension GL_EXT_scalar_block_layout : enable
#extension GL_EXT_nonuniform_qualifier : enable


layout (location = 0) out vec3 outColor;
layout (location = 1) out vec2 outUV;

struct Vertex {
    vec3 position;
    float uv_x;
    vec3 normal;
    float uv_y;
    vec4 color;
};

layout(buffer_reference, std430) readonly buffer VertexBuffer{
    Vertex vertices[];
};

layout(buffer_reference, std430) readonly buffer IndexBuffer{
    uint indices[];
};


layout(scalar, binding = 0) uniform Ubo{
  mat4 worldMatrix;
} ubo[];

//push constants block
layout(scalar, push_constant) uniform constants
{
    uint objId;
    VertexBuffer vertexBuffer;
    IndexBuffer indexBuffer;
} push;

void main()
{
    // Before having the index buffer as an storage buffer and loading it through its device address
    // from the push constants, the index was just gl_VertexIndex:
    // uint index = gl_VertexIndex;

    // Load index&vertex data from device adress
    uint index = push.indexBuffer.indices[gl_VertexIndex];
    Vertex v = push.vertexBuffer.vertices[index];
    
    // output data
    gl_Position = ubo[push.objId].worldMatrix * vec4(v.position, 1.0f);
    outColor = v.color.xyz;
    outUV.x = v.uv_x;
    outUV.y = v.uv_y;
}
