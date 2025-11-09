#version 460 core
in vec2 vUV;
out vec4 FragColor;

uniform sampler2D uTex;
uniform int uUseTex; // 0 = solid color, 1 = sample texture

void main(){
    if (uUseTex == 1) {
        FragColor = texture(uTex, vUV);
    } else {
        FragColor = vec4(1.0, 0.5, 0.2, 1.0);
    }
}
