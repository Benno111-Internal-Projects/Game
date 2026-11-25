#version 330
in vec3 vertexPosition;
in vec2 vertexTexCoord;
in vec3 vertexNormal;
in vec4 vertexColor;

uniform mat4 mvp;
uniform vec2 mazeSize;

out vec2 lightGrid;
out float surfaceHeight;
out vec2 uv;
out vec4 tint;

void main() {
    // Sample just outside the stone face, on the passage side.
    lightGrid = (vertexPosition.xz + vertexNormal.xz * 0.40) / 2.5
              + vec2(0.5, 1.0) * mazeSize;
    surfaceHeight = vertexPosition.y;
    uv = vertexTexCoord;
    tint = vertexColor;
    gl_Position = mvp * vec4(vertexPosition, 1.0);
}
