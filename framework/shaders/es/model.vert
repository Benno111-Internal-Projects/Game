#version 100
attribute vec3 vertexPosition;
attribute vec2 vertexTexCoord;
attribute vec3 vertexNormal;

uniform mat4 mvp;
uniform mat4 matNormal;

varying mediump vec2 uv;
varying mediump vec3 normal;

void main() {
    uv = vertexTexCoord;
    normal = mat3(matNormal) * vertexNormal;
    gl_Position = mvp * vec4(vertexPosition, 1.0);
}
