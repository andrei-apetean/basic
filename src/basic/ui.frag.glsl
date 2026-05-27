#version 450
#extension GL_EXT_nonuniform_qualifier : require

#define UI_FLAG_GLYPH 1u

layout(set = 0, binding = 0) uniform sampler2D textures[];

layout(location = 0) in vec4      in_color;
layout(location = 1) in vec2      in_uv;
layout(location = 2) in flat uint in_tex_index;
layout(location = 3) in flat uint in_flags;

layout(location = 0) out vec4 out_color;

void main()
{

    if (in_tex_index == 0xFFFFFFFFu) {
        // colored rect — no texture
        out_color = in_color;
    } else if ((in_flags & 1u) != 0) {
        float alpha = texture(textures[nonuniformEXT(in_tex_index)], in_uv).a;
        out_color = vec4(in_color.rgb, in_color.a * alpha);
    } else {
        out_color = texture(textures[nonuniformEXT(in_tex_index)], in_uv) * in_color;
    }

}
