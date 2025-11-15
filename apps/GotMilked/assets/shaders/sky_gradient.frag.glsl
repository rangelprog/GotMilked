#version 460 core

in vec2 vUV;
out vec4 FragColor;

uniform vec3 uTopColor;
uniform vec3 uBottomColor;

void main() {
    float t = clamp(vUV.y, 0.0, 1.0);
    vec3 color = mix(uBottomColor, uTopColor, t);
    FragColor = vec4(color, 1.0);
}

