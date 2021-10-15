// SPDX-License-Identifier: Zlib

#version 140

in vec3 position;
in vec3 normal;
in vec3 color;

out vec3 vertColor;
out vec4 fragPos;
out vec3 fragNormal;
out vec4 lightPos;

uniform mat4 mvp;
uniform mat4 modelview;
uniform mat3 normal_matrix;
uniform vec4 light_pos;


void main() {
  vec4 pos = vec4(position, 1.0);
  gl_Position = mvp * pos;
  lightPos = modelview * light_pos;
  vertColor = color;

  // Position of vertex in camera coordinates interpolated to frag coords
  fragPos = modelview * pos;

  fragNormal = normal_matrix * normal;
}
