#version 450

layout(location = 0) in vec3 in_pos;
layout(location = 1) in vec3 in_normal;

layout(push_constant) uniform PC {
    mat4 mvp;
} pc;

layout(location = 0) out vec3 out_normal;

void main()
{
    gl_Position = pc.mvp * vec4(in_pos, 1.0);
    out_normal  = in_normal;
}
