#version 100
attribute vec3 vertexPosition;
attribute vec2 vertexTexCoord;
attribute vec3 vertexNormal;
attribute vec4 vertexColor;

uniform mat4 mvp;

#ifdef GL_FRAGMENT_PRECISION_HIGH
#define WORLD_PRECISION highp
#else
#define WORLD_PRECISION mediump
#endif

varying WORLD_PRECISION vec3 worldPosition;
varying mediump vec3 surfaceNormal;
varying mediump vec3 biomeTint;

void main() {
    worldPosition = vec3(vertexTexCoord.x, vertexPosition.y, vertexTexCoord.y);
    surfaceNormal = vertexNormal;
    biomeTint = vertexColor.rgb;
    gl_Position = mvp * vec4(vertexPosition, 1.0);
}
