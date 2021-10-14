// SPDX-License-Identifier: Zlib

#version 140

in vec3 position;
in vec3 normal;
in vec3 color;

out vec4 vertColor;
out vec4 fragPos;

uniform mat4 mvp;
uniform mat4 modelview;

void main() {
  vec4 pos = vec4(position, 1.0);
  gl_Position = mvp * pos;
  vertColor = vec4(color, 1.0);

  // Position of vertex in camera coordinates interpolated to frag coords
  fragPos = modelview * pos;
}
