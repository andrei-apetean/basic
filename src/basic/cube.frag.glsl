#version 450

layout(location = 0) in  vec3 in_normal;
layout(location = 0) out vec4 out_color;

void main()
{
    // vec3 light_dir = normalize(vec3(1.0, 2.0, 3.0));
    // float diffuse  = max(dot(normalize(in_normal), light_dir), 0.0);
    // vec3  color    = vec3(0.8, 0.4, 0.2) * (diffuse + 0.15);
    // out_color      = vec4(color, 1.0);
    out_color = vec4(in_normal * 0.5 + 0.5, 1.0);
    // out_color = vec4(1.0, 0.0, 0.0, 1.0); 
}
