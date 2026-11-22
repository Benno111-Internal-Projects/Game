#version 100
precision mediump float;
#ifdef GL_FRAGMENT_PRECISION_HIGH
#define WORLD_PRECISION highp
#else
#define WORLD_PRECISION mediump
#endif

varying WORLD_PRECISION vec3 worldPosition;
varying mediump vec3 surfaceNormal;
varying mediump vec3 biomeTint;

uniform sampler2D ground;
uniform sampler2D sediment;
uniform sampler2D rock;
uniform vec2 elevation;


void main() {
    vec3 normal = normalize(surfaceNormal);
    WORLD_PRECISION vec2 groundUV = worldPosition.xz * 0.19;
    float grassDetail = texture2D(ground, groundUV).r;
    float sedimentDetail = texture2D(sediment, worldPosition.xz * 0.13).r;
    // Fade high-frequency detail toward the horizon to reduce shimmer in motion.
    float detailAmount = smoothstep(0.012, 0.065, gl_FragCoord.w);
    grassDetail = mix(0.62, grassDetail, detailAmount);
    sedimentDetail = mix(0.67, sedimentDetail, detailAmount);
    float altitude = (worldPosition.y - elevation.x) * elevation.y;
    float weathering = smoothstep(-0.27, 0.37, altitude + 0.58 * (grassDetail - 0.62));
    float steepness = 1.0 - normal.y;
    float exposed = smoothstep(0.045, 0.23, steepness + 0.08 * (sedimentDetail - 0.65));

    // Rock is projected from three directions so cliffs keep readable detail.
    vec3 weights = abs(normal);
    weights *= weights;
    weights *= weights;
    weights /= weights.x + weights.y + weights.z;
    WORLD_PRECISION vec3 rockUV = worldPosition * 0.15;
    float rockDetail = dot(weights, vec3(texture2D(rock, rockUV.zy).r,
                                       texture2D(rock, rockUV.xz).r,
                                       texture2D(rock, rockUV.xy).r));

    rockDetail = mix(0.65, rockDetail, detailAmount);
    vec3 tint = biomeTint * 1.28 + vec3(0.045);
    vec3 weatheredTint = tint * vec3(1.14, 1.04, 0.90);
    float brightness = dot(tint, vec3(0.30, 0.59, 0.11));
    vec3 stoneTint = mix(tint, vec3(brightness) * vec3(1.08, 1.04, 1.00), 0.66);
    vec3 soil = mix(tint * (0.43 + 0.90 * grassDetail),
                    weatheredTint * (0.38 + 0.94 * sedimentDetail), weathering);
    gl_FragColor = vec4(mix(soil, stoneTint * (0.45 + 1.00 * rockDetail), exposed), 1.0);
}
