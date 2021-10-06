#version 140

in vec3 position;
in vec3 normal;
in vec4 color;

uniform mat4 mvp;

smooth out vec4 vertexColor;

void main() {
  gl_Position = mvp * vec4(position, 1.0);
  vertexColor = color;
}
