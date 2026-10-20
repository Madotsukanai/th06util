#version 450

// Vertical (portrait) rotation composition. Mirrors the fork's D3D-direct CPU
// blit in GameWindow::Present pixel for pixel:
//   - in-game: the playfield plus HUD margin bands are transposed into a
//     rotW x rotH region of the backbuffer, which is then stretched into the
//     letterboxed destRect; the client area outside destRect gets the 32x32
//     pattern tile sampled from the vertical surface at (416,0).
//   - menus: the full 640x480 content is transposed and stretched into
//     destRect; outside area is black.
//   - vertical_mode == 2 applies a 180 degree flip on top.
layout(location = 0) out vec4 outColor;

layout(binding = 0) uniform sampler2D uTex;

layout(push_constant) uniform PC {
    vec4 rotRect;   // x: rotW, y: rotH, z: dstX (unused), w: dstY (unused)
    vec4 flags;     // x: isGame, y: flip180, z: transitionBlack, w: unused
    vec4 dest;      // x: destX, y: destY, z: destW, w: destH (client-space letters box)
} pc;

// Plays the role of the srcYTable[..] built from the margin bands in
// GameWindow::Present: dstX column d of the rotated region maps to this row
// of the (640 x 648) vertical surface.
int srcYForBand(int d) {
    int d2 = d;
    if (d2 < 16) return 480 + 0   + (16 - 1 - d2);
    d2 -= 16;
    if (d2 < 40) return 480 + 128 + (40 - 1 - d2);
    d2 -= 40;
    if (d2 < 16) return 480 + 16  + (16 - 1 - d2);
    d2 -= 16;
    if (d2 < 16) return 480 + 48  + (16 - 1 - d2);
    d2 -= 16;
    if (d2 < 16) return 480 + 64  + (16 - 1 - d2);
    d2 -= 16;
    if (d2 < 16) return 480 - 1 - d2;
    d2 -= 16;
    if (d2 < 448) return 464 - 1 - d2;
    d2 -= 448;
    if (d2 < 16) return 480 + 80  + (16 - 1 - d2);
    d2 -= 16;
    if (d2 < 16) return 480 + 96  + (16 - 1 - d2);
    d2 -= 16;
    if (d2 < 16) return 480 + 112 + (16 - 1 - d2);
    d2 -= 16;
    return 480 + 32 + (16 - 1 - d2);
}

void main() {
    if (pc.flags.z > 0.5) {
        outColor = vec4(0.0, 0.0, 0.0, 1.0);
        return;
    }

    int cliX = int(floor(gl_FragCoord.x));
    int cliY = int(floor(gl_FragCoord.y));

    float destX = pc.dest.x;
    float destY = pc.dest.y;
    float destW = pc.dest.z;
    float destH = pc.dest.w;

    int sampleX;
    int sampleY;
    bool flip = pc.flags.y > 0.5;

    // Destination pixel inside the vertically stretched render region?
    bool inDest = (gl_FragCoord.x >= destX && gl_FragCoord.x < destX + destW &&
                   gl_FragCoord.y >= destY && gl_FragCoord.y < destY + destH);

    if (pc.flags.x > 0.5) {
        if (inDest) {
            int vdx = int(floor((gl_FragCoord.x - destX) / destW * pc.rotRect.x));
            int vdy = int(floor((gl_FragCoord.y - destY) / destH * pc.rotRect.y));
            if (flip) {
                vdx = int(pc.rotRect.x) - 1 - vdx;
                vdy = int(pc.rotRect.y) - 1 - vdy;
            }
            sampleX = 32 + vdy;
            sampleY = srcYForBand(vdx);
        } else {
            int tileX = cliX - (cliX / 32) * 32;
            int tileY = cliY - (cliY / 32) * 32;
            if (flip) {
                tileX = 31 - tileX;
                tileY = 31 - tileY;
            }
            sampleX = 416 + tileX;
            sampleY = tileY;
        }
    } else {
        int vdx = int(floor((gl_FragCoord.x - destX) / destW * 640.0));
        int vdy = int(floor((gl_FragCoord.y - destY) / destH * 480.0));
        if (flip) {
            vdx = 639 - vdx;
            vdy = 479 - vdy;
        }
        sampleX = (vdy * 640) / 480;
        sampleY = 479 - (vdx * 480) / 640;
    }

    outColor = texture(uTex, vec2((float(sampleX) + 0.5) / 640.0, (float(sampleY) + 0.5) / 648.0));
}
