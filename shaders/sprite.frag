#version 450

layout(location = 0) in vec2 fragTexCoord;
layout(location = 1) in vec4 fragColor;

layout(location = 0) out vec4 outColor;

layout(binding = 0) uniform sampler2D texSampler;

layout(push_constant) uniform PushConstants {
    mat4 projMat;
    vec4 colorKey;
    float colorKeyEnable;
} pc;

void main() {
    vec4 texColor = texture(texSampler, fragTexCoord);

    if (pc.colorKeyEnable > 0.5) {
        vec3 diff = abs(texColor.rgb - pc.colorKey.rgb);
        if (diff.r < 0.02 && diff.g < 0.02 && diff.b < 0.02) {
            discard;
        }
    }

    outColor = texColor * fragColor;
}
