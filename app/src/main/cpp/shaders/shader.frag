#version 300 es
precision mediump float;

in vec2 fragTexCoord;
in vec4 fragColor;

uniform sampler2D uTexture;
uniform bool uUseTexture;

out vec4 outColor;

void main() {
    if (uUseTexture) {
        vec4 texElement = texture(uTexture, fragTexCoord);
        outColor = vec4(fragColor.rgb, fragColor.a * texElement.r);
    } else {
        outColor = fragColor;
    }
}

