#version 330
in vec3 direction;
uniform samplerCube skybox;
out vec4 fragment;

void main() {
    fragment = texture(skybox, direction);
}
