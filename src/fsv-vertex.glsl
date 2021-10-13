// SPDX-License-Identifier: Zlib

#version 140

in vec3 position;
in vec3 normal;

uniform mat4 mvp;

void main() {
  gl_Position = mvp * vec4(position, 1.0);
}
