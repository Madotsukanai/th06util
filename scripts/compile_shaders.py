#!/usr/bin/env python3
"""Compile GLSL shaders to SPIR-V and (optionally) re-embed them.

The compiled game binaries use the SPV blobs embedded in
src/render/shaders_embedded*.hpp, NOT the shaders/*.spv files (build.py never
invokes glslangValidator). The .spv files are intermediate build artifacts and
are intentionally git-ignored.

Usage:
    python3 scripts/compile_shaders.py            # compile shaders/*.vert|frag -> shaders/*.spv
    python3 scripts/compile_shaders.py --embed    # also rewrite the embedded arrays

Note: SPIR-V bytes are NOT stable across glslangValidator versions; the
committed embedded arrays were produced by an older toolchain and are kept as
the canonical build input. Re-running --embed regenerates semantically
equivalent (but not byte-identical) blobs - rebuild and smoke-test the game
afterwards.
"""

import argparse
import re
import struct
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent

# (glsl source, spv output, embedded header, array name)
SHADERS = [
    ("shaders/fullscreen.vert", "shaders/fullscreen.vert.spv", "src/render/shaders_embedded_fullscreen.hpp", "kFullscreenVertSpv"),
    ("shaders/vertical.frag",   "shaders/vertical.frag.spv",   "src/render/shaders_embedded_vertical.hpp",   "kVerticalFragSpv"),
    ("shaders/sprite.vert",     "shaders/sprite.vert.spv",     "src/render/shaders_embedded.hpp",            "kSpriteVertSpv"),
    ("shaders/sprite.frag",     "shaders/sprite.frag.spv",     "src/render/shaders_embedded_frag.hpp",       "kSpriteFrag2Spv"),
    ("shaders/stage3d.vert",    "shaders/stage3d.vert.spv",    "src/render/shaders_embedded.hpp",            "kStage3dVertSpv"),
    ("shaders/stage3d.frag",    "shaders/stage3d.frag.spv",    "src/render/shaders_embedded_frag.hpp",       "kStage3dFrag2Spv"),
]


def compile_shader(src: Path, dst: Path) -> bool:
    result = subprocess.run(
        ["glslangValidator", "-V", str(src), "-o", str(dst)],
        capture_output=True, text=True,
    )
    if result.returncode != 0:
        print(f"FAIL {src}:\n{result.stdout}{result.stderr}", file=sys.stderr)
        return False
    return True


def spv_to_c_array_words(spv: bytes) -> list[int]:
    assert len(spv) % 4 == 0, f"SPV size {len(spv)} is not 4-byte aligned"
    return list(struct.unpack(f"<{len(spv) // 4}I", spv))


def embed(header: Path, array: str, spv: bytes) -> bool:
    src = header.read_text()
    pattern = re.compile(re.escape(array) + r"\[\] = \{.*?\};", re.S)
    match = pattern.search(src)
    if not match:
        print(f"FAIL array {array} not found in {header}", file=sys.stderr)
        return False

    words = spv_to_c_array_words(spv)
    lines = []
    for i in range(0, len(words), 8):
        group = words[i:i + 8]
        line = "    " + ", ".join(f"0x{w:08x}" for w in group) + ","
        lines.append(line)  # trailing comma on the last line follows the repo style

    block = "{\n" + "\n".join(lines) + "\n};"
    src = src[:match.start()] + array + "[] = " + block + src[match.end():]
    header.write_text(src)
    return True


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--embed", action="store_true",
                        help="compile and rewrite the arrays in src/render/shaders_embedded*.hpp")
    parser.add_argument("--embed-only", action="store_true", dest="embed_only",
                        help="re-embed the existing shaders/*.spv files WITHOUT recompiling")
    args = parser.parse_args()

    if args.embed_only:
        args.embed = True

    ok = True
    for src, dst, header, array in SHADERS:
        src_p, dst_p, hdr_p = ROOT / src, ROOT / dst, ROOT / header
        if not args.embed_only:
            if not compile_shader(src_p, dst_p):
                ok = False
                continue
        elif not dst_p.is_file():
            print(f"FAIL missing {dst} (run without --embed-only first)", file=sys.stderr)
            ok = False
            continue
        spv = dst_p.read_bytes()
        print(f"{'USING' if args.embed_only else 'OK  '} {src} -> {dst} ({len(spv)} bytes)")
        if args.embed:
            if embed(hdr_p, array, spv):
                print(f"OK   {array} re-embedded into {header}")
            else:
                ok = False

    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
