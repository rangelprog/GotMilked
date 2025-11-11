#version 460 core
in vec3 vNormal;
in vec3 vFragPos;
in vec2 vUV;

out vec4 FragColor;

uniform vec3 uViewPos;

// Material properties
uniform sampler2D uTex;
uniform int uUseTex;
uniform vec3 uSolidColor;

struct Material {
    vec3 diffuse;
    vec3 specular;
    float shininess;
    vec3 emission;
};

uniform Material uMaterial;

// Lighting
struct Light {
    int type;              // 0=Directional, 1=Point, 2=Spot, -1=Disabled
    vec3 color;
    vec3 position;         // For Point/Spot
    vec3 direction;        // For Directional/Spot
    vec3 attenuation;      // For Point/Spot (constant, linear, quadratic)
    float innerCone;       // For Spot
    float outerCone;       // For Spot
};

uniform int uNumLights;
uniform Light uLights[8];

// Calculate lighting contribution from a single light
vec3 CalculateLight(Light light, vec3 normal, vec3 fragPos, vec3 viewDir, vec3 albedo, vec3 specularColor, float shininess) {
    if (light.type == -1) return vec3(0.0); // Disabled light
    
    vec3 lightDir;
    float attenuation = 1.0;
    
    if (light.type == 0) {
        // Directional light
        lightDir = normalize(-light.direction);
    } else if (light.type == 1) {
        // Point light
        vec3 lightVec = light.position - fragPos;
        float distance = length(lightVec);
        lightDir = normalize(lightVec);
        attenuation = 1.0 / (light.attenuation.x + light.attenuation.y * distance + light.attenuation.z * distance * distance);
    } else if (light.type == 2) {
        // Spot light
        vec3 lightVec = light.position - fragPos;
        float distance = length(lightVec);
        lightDir = normalize(lightVec);
        
        // Attenuation
        attenuation = 1.0 / (light.attenuation.x + light.attenuation.y * distance + light.attenuation.z * distance * distance);
        
        // Spot angle attenuation
        float theta = dot(lightDir, normalize(-light.direction));
        float epsilon = light.innerCone - light.outerCone;
        float spotIntensity = clamp((theta - light.outerCone) / epsilon, 0.0, 1.0);
        attenuation *= spotIntensity;
    } else {
        return vec3(0.0);
    }
    
    // Diffuse
    float NdotL = max(dot(normal, lightDir), 0.0);
    vec3 diffuse = NdotL * albedo * light.color;
    
    // Specular (Blinn-Phong)
    vec3 halfDir = normalize(lightDir + viewDir);
    float spec = pow(max(dot(normal, halfDir), 0.0), shininess);
    vec3 specular = spec * specularColor * light.color;
    
    return (diffuse + specular) * attenuation;
}

void main() {
    vec3 N = normalize(vNormal);
    vec3 V = normalize(uViewPos - vFragPos);
    
    // Albedo from texture or solid color
    vec3 albedo = (uUseTex == 1) ? texture(uTex, vUV).rgb : uSolidColor;
    
    // Material properties (with fallback)
    vec3 materialDiffuse = uMaterial.diffuse;
    vec3 materialSpecular = uMaterial.specular;
    float materialShininess = uMaterial.shininess;
    
    // Combine texture with material diffuse color
    // If material diffuse is set, multiply texture by it; otherwise use texture directly
    if (length(materialDiffuse) > 0.001) {
        // Material diffuse color is set - multiply with texture
        if (uUseTex == 1) {
            materialDiffuse = albedo * materialDiffuse;
        } else {
            // No texture, use material diffuse directly
            materialDiffuse = materialDiffuse;
        }
    } else {
        // No material diffuse set, use albedo (texture or solid color)
        materialDiffuse = albedo;
    }
    
    // Use defaults for specular and shininess if not set
    if (length(materialSpecular) < 0.001) materialSpecular = vec3(0.5);
    if (materialShininess < 0.001) materialShininess = 32.0;
    
    // Ambient (increased for better visibility)
    vec3 ambient = 0.3 * materialDiffuse;
    
    // Accumulate lighting from all lights
    vec3 lighting = ambient;
    for (int i = 0; i < uNumLights && i < 8; ++i) {
        lighting += CalculateLight(uLights[i], N, vFragPos, V, materialDiffuse, materialSpecular, materialShininess);
    }
    
    // Add emission
    lighting += uMaterial.emission;
    
    FragColor = vec4(lighting, 1.0);
}
