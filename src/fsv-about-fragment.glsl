// SPDX-License-Identifier: Zlib

#version 140

in vec4 vertColor;
in vec4 fragPos;

out vec4 outputColor;

uniform vec4 fog_color;
uniform float fog_start;
uniform float fog_end;

void main() {
  float zdist = abs(fragPos.z / fragPos.w);
  float fog = (fog_end - zdist) / (fog_end - fog_start);
  outputColor = mix(fog_color, vertColor, fog);
}
