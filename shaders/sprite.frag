#version 450

layout(location = 0) in vec2 fragTexCoord;
layout(location = 1) in vec4 fragColor;

layout(location = 0) out vec4 outColor;

layout(binding = 0) uniform sampler2D texSampler;

layout(push_constant) uniform PushConstants {
    mat4 projMat;
    vec4 colorKey;
    float colorKeyEnable;
    float colorOpAdd; // 1.0 = D3DTOP_ADD (flash), 0.0 = D3DTOP_MODULATE
    // D3D8 texel-center origin convention: a texture coordinate of k/size
    // addresses the CENTER of texel k, while Vulkan/OpenGL address the texel
    // EDGE. The renderer stores (0.5/width, 0.5/height) here so that
    // uv + halfTexel reproduces the historical Direct3D sampling positions.
    float halfTexelU;
    float halfTexelV;
} pc;

void main() {
    vec4 texColor = texture(texSampler, fragTexCoord + vec2(pc.halfTexelU, pc.halfTexelV));

    if (pc.colorKeyEnable > 0.5) {
        vec3 diff = abs(texColor.rgb - pc.colorKey.rgb);
        if (diff.r < 0.02 && diff.g < 0.02 && diff.b < 0.02) {
            discard;
        }
    }

    vec4 col;
    if (pc.colorOpAdd > 0.5) {
        col = vec4(clamp(texColor.rgb + fragColor.rgb, 0.0, 1.0), texColor.a * fragColor.a);
    } else {
        col = texColor * fragColor;
    }

    // D3DRS_ALPHATEST: ALPHAREF = 4, ALPHAFUNC = GREATEREQUAL
    if (col.a < 4.0 / 255.0) {
        discard;
    }
    outColor = col;
}
