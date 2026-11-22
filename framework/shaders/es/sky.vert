#version 100
attribute vec3 vertexPosition;
uniform mat4 mvp;
varying mediump vec3 direction;

void main() {
    direction = vertexPosition;
    vec4 projected = mvp * vec4(vertexPosition, 1.0);
    gl_Position = projected.xyww;
}
