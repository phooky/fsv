// SPDX-License-Identifier: Zlib

#version 140

in vec3 position;
in vec3 normal;
in vec3 color;

out vec4 vertColor;
uniform mat4 mvp;

void main() {
  gl_Position = mvp * vec4(position, 1.0);
  vertColor = vec4(color, 1.0);
}
