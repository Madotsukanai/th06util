#version 450

layout(location = 0) in vec3 inPos;
layout(location = 1) in vec2 inTexCoord;
layout(location = 2) in vec4 inColor;

layout(location = 0) out vec2 fragTexCoord;
layout(location = 1) out vec4 fragColor;
layout(location = 2) out float fragDist;

layout(push_constant) uniform PushConstants {
    mat4 viewProjMat;
    vec4 fogColor;
    vec2 fogDist; // x=near, y=far
    float fogEnable;
} pc;

void main() {
    gl_Position = pc.viewProjMat * vec4(inPos, 1.0);
    fragTexCoord = inTexCoord;
    fragColor = inColor;
    fragDist = gl_Position.w; // W in clip space corresponds to eye-space Z depth
}
