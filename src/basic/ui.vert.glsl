#version 450

layout(location = 0) in vec4  in_rect;       // x, y, w, h
layout(location = 1) in vec4  in_color;      // r, g, b, a
layout(location = 2) in vec4  in_uv;         // u0, v0, u1, v1
layout(location = 3) in uint in_tex_index;  // raw bits reinterpreted as uint
layout(location = 4) in uint in_flags;

layout(push_constant) uniform PC {
    vec2 screen_size;
} pc;

layout(location = 0) out vec4  out_color;
layout(location = 1) out vec2  out_uv;
layout(location = 2) out flat uint out_tex_index;
layout(location = 3) out flat uint out_flags;

void main()
{
    vec2 offsets[4] = vec2[4](
        vec2(0.0, 0.0),
        vec2(0.0, 1.0),
        vec2(1.0, 0.0),
        vec2(1.0, 1.0)
    );

    vec2 uv_min = in_uv.xy;
    vec2 uv_max = in_uv.zw;
    vec2 off    = offsets[gl_VertexIndex];

    vec2 pos = in_rect.xy + in_rect.zw * off;
    pos = (pos / pc.screen_size) * 2.0 - 1.0;

    gl_Position   = vec4(pos, 0.0, 1.0);
    out_color     = in_color;
    out_uv        = mix(uv_min, uv_max, off);
    out_tex_index = in_tex_index;
    out_flags     = in_flags;
}

