#version 330
in vec3 vertexPosition;
in vec3 vertexNormal;
in vec4 vertexColor;

uniform mat4 mvp;

out vec3 worldPosition;
out vec3 surfaceNormal;
out vec3 biomeTint;

void main() {
    worldPosition = vertexPosition;
    surfaceNormal = vertexNormal;
    biomeTint = vertexColor.rgb;
    gl_Position = mvp * vec4(vertexPosition, 1.0);
}
