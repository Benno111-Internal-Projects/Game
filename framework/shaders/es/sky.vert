#version 100
attribute vec3 vertexPosition;
uniform mat4 mvp;
varying mediump vec3 direction;

void main() {
    direction = vertexPosition;
    gl_Position = mvp * vec4(vertexPosition, 1.0);
}
