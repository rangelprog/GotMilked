#version 460 core
in vec3 vNormal;
in vec3 vFragPos;
in vec2 vUV;

out vec4 FragColor;

uniform vec3 uViewPos;
uniform vec3 uLightDir;   // Richtung aus der das Licht kommt (world space, normalized, z.B. ( -0.4, -1.0, -0.3 ))
uniform vec3 uLightColor; // z.B. (1.0, 1.0, 1.0)

uniform sampler2D uTex;   // Albedo
uniform int uUseTex;      // 1 = texture, 0 = solid

uniform vec3 uSolidColor; // wenn uUseTex==0

void main(){
    vec3 N = normalize(vNormal);
    vec3 L = normalize(-uLightDir);      // Licht-Richtung zum Fragment
    float NdotL = max(dot(N, L), 0.0);

    // Albedo
    vec3 albedo = (uUseTex == 1) ? texture(uTex, vUV).rgb : uSolidColor;

    // simple Blinn-Phong
    vec3 V = normalize(uViewPos - vFragPos);
    vec3 H = normalize(L + V);
    float spec = pow(max(dot(N, H), 0.0), 32.0);

    vec3 ambient  = 0.10 * albedo;
    vec3 diffuse  = NdotL * albedo * uLightColor;
    vec3 specular = 0.15 * spec * uLightColor;

    vec3 color = ambient + diffuse + specular;
    FragColor = vec4(color, 1.0);
}
