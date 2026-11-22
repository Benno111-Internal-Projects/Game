#version 330
in vec3 worldPosition;
in vec2 uv;
in vec4 tint;

uniform sampler2D texture0;
uniform vec4 torchLights[8];

out vec4 finalColor;

void main() {
    float fire = 0.0;
    for (int i = 0; i < 8; ++i) {
        vec3 delta = clamp(worldPosition - torchLights[i].xyz, -8.0, 8.0);
        float falloff = max(0.0, 1.0 - dot(delta, delta) / 28.0);
        fire += falloff * falloff * torchLights[i].w;
    }
    vec3 light = vec3(0.20, 0.23, 0.29) + vec3(1.24, 0.86, 0.51) * min(fire, 1.0);
    vec4 surface = texture(texture0, uv) * tint;
    finalColor = vec4(surface.rgb * light, surface.a);
}
