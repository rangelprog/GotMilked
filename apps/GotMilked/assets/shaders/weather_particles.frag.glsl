#version 460 core

in VS_OUT {
    vec4 color;
    float alpha;
} fs_in;

layout(location = 0) out vec4 FragColor;

void main() {
    float opacity = fs_in.alpha;
    FragColor = vec4(fs_in.color.rgb, opacity);
}


