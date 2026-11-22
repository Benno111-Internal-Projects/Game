#version 330
in vec2 lightGrid;
in float surfaceHeight;
in vec2 uv;
in vec4 tint;

uniform sampler2D texture0;
uniform sampler2D lightmap;
uniform vec2 mazeSize;
uniform float flicker;

out vec4 finalColor;

void main() {
    // Keep bilinear filtering within the sampled tile so corners cannot bleed.
    vec2 sampleGrid = floor(lightGrid) + clamp(fract(lightGrid), 0.125, 0.875);
    vec3 layers = texture(lightmap, sampleGrid / mazeSize).rgb;
    float fire = surfaceHeight < 1.38
               ? mix(layers.r, layers.g, clamp(surfaceHeight / 1.38, 0.0, 1.0))
               : mix(layers.g, layers.b, clamp((surfaceHeight - 1.38) / 2.22, 0.0, 1.0));
    vec3 light = vec3(0.20, 0.23, 0.29) + vec3(1.24, 0.86, 0.51) * (fire * flicker);
    vec4 surface = texture(texture0, uv) * tint;
    finalColor = vec4(surface.rgb * light, surface.a);
}
