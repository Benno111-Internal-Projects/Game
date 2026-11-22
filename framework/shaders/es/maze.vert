#version 100
attribute vec3 vertexPosition;
attribute vec2 vertexTexCoord;
attribute vec4 vertexColor;

uniform mat4 mvp;

varying mediump vec3 worldPosition;
varying mediump vec2 uv;
varying mediump vec4 tint;

void main() {
    worldPosition = vertexPosition;
    uv = vertexTexCoord;
    tint = vertexColor;
    gl_Position = mvp * vec4(vertexPosition, 1.0);
}
