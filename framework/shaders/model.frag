#version 330
in vec2 uv;
in vec3 normal;

uniform sampler2D texture0;
uniform vec4 colDiffuse;
uniform vec3 lightDirection;

out vec4 fragment;

void main() {
    vec4 albedo = texture(texture0, uv) * colDiffuse;
    float diffuse = max(dot(normalize(normal), normalize(lightDirection)), 0.0);
    fragment = vec4(albedo.rgb * (0.35 + 0.65 * diffuse), albedo.a);
}
