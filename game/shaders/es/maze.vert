#version 100
attribute vec3 vertexPosition;
attribute vec2 vertexTexCoord;
attribute vec3 vertexNormal;
attribute vec4 vertexColor;

uniform mat4 mvp;
uniform mediump vec2 mazeSize;

varying mediump vec2 lightGrid;
varying mediump float surfaceHeight;
varying mediump vec2 uv;
varying mediump vec4 tint;

void main() {
    // Sample just outside the stone face, on the passage side.
    lightGrid = (vertexPosition.xz + vertexNormal.xz * 0.40) / 2.5
              + vec2(0.5, 1.0) * mazeSize;
    surfaceHeight = vertexPosition.y;
    uv = vertexTexCoord;
    tint = vertexColor;
    gl_Position = mvp * vec4(vertexPosition, 1.0);
}
