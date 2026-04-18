#version 450

layout(location = 0) in vec4 rect;   // per instance: x, y, w, h (0-1)
layout(location = 1) in vec4 color;  // per instance: r, g, b, a

layout(push_constant) uniform PushConstants {
    float screen_width;
    float screen_height;
} pc;

layout(location = 0) out vec4 frag_color;

void main()
{
    // generate quad from vertex index (triangle strip: 0,1,2,3)
    vec2 pos = vec2(
        float(gl_VertexIndex & 1),
        float((gl_VertexIndex >> 1) & 1)
    );

    // scale and offset into instance rect
    pos = rect.xy + pos * rect.zw;

    // convert pixels to NDC
    pos.x = (pos.x / pc.screen_width)  * 2.0 - 1.0;
    pos.y = (pos.y / pc.screen_height) * 2.0 - 1.0;

    gl_Position = vec4(pos, 0.0, 1.0);
    frag_color  = color;
}
