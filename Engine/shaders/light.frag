#version 330 core

out vec4 lightColor;

uniform vec3 color;

void main(){
    lightColor = vec4(color, 1.0f);
}
