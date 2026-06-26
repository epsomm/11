#version 300 es
layout(location = 0) in vec2 inPos;
layout(location = 1) in vec2 inTexCoord;
layout(location = 2) in vec4 inColor;

uniform vec2 uScreenSize;

out vec2 fragTexCoord;
out vec4 fragColor;

void main() {
    vec2 zeroToOne = inPos / uScreenSize;
    vec2 zeroToTwo = zeroToOne * 2.0;
    vec2 clipSpace = zeroToTwo - 1.0;
    gl_Position = vec4(clipSpace * vec2(1.0, -1.0), 0.0, 1.0);
    fragTexCoord = inTexCoord;
    fragColor = inColor;
}

