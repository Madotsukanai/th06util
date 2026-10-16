#version 450

layout(location = 0) in vec2 fragTexCoord;
layout(location = 1) in vec4 fragColor;
layout(location = 2) in float fragDist;

layout(location = 0) out vec4 outColor;

layout(binding = 0) uniform sampler2D texSampler;

layout(push_constant) uniform PushConstants {
    mat4 viewProjMat;
    vec4 fogColor;
    vec2 fogDist; // x=near, y=far
    float fogEnable;
} pc;

void main() {
    vec4 texColor = texture(texSampler, fragTexCoord);
    vec4 col = texColor * fragColor;

    if (pc.fogEnable > 0.5 && pc.fogDist.y > pc.fogDist.x) {
        float fogFactor = clamp((pc.fogDist.y - fragDist) / (pc.fogDist.y - pc.fogDist.x), 0.0, 1.0);
        col.rgb = mix(pc.fogColor.rgb, col.rgb, fogFactor);
    }

    outColor = col;
}
