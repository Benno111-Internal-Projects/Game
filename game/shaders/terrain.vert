#version 330
in vec3 vertexPosition;
in vec2 vertexTexCoord;
in vec3 vertexNormal;
in vec4 vertexColor;

uniform mat4 mvp;

out vec3 worldPosition;
out vec3 surfaceNormal;
out vec3 biomeTint;

void main() {
    worldPosition = vec3(vertexTexCoord.x, vertexPosition.y, vertexTexCoord.y);
    surfaceNormal = vertexNormal;
    biomeTint = vertexColor.rgb;
    gl_Position = mvp * vec4(vertexPosition, 1.0);
}
