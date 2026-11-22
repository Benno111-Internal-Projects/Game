#version 330
in vec3 vertexPosition;
uniform mat4 mvp;
out vec3 direction;

void main() {
    direction = vertexPosition;
    vec4 projected = mvp * vec4(vertexPosition, 1.0);
    gl_Position = projected.xyww;
}
