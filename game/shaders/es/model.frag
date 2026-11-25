#version 100
precision mediump float;
varying vec2 uv;
varying vec3 normal;

uniform sampler2D texture0;
uniform vec4 colDiffuse;
uniform vec3 lightDirection;


void main() {
    vec4 albedo = texture2D(texture0, uv) * colDiffuse;
    float diffuse = max(dot(normalize(normal), normalize(lightDirection)), 0.0);
    gl_FragColor = vec4(albedo.rgb * (0.35 + 0.65 * diffuse), albedo.a);
}
