#!/usr/bin/env python3
"""Verification helper for Touhou Koumakyou ver 1.02h original executable.
Ensures resources/game.exe exists, matches expected file size (512,000 bytes)
and SHA-256 hash before permitting any build.
"""
import hashlib
import sys
from pathlib import Path

EXPECTED_SIZE = 512000
EXPECTED_SHA256 = "9f76483c46256804792399296619c1274363c31cd8f1775fafb55106fb852245"


def verify_original_game_exe(root: Path) -> Path:
    game_exe = root / "resources" / "game.exe"
    if not game_exe.exists():
        print("=" * 70, file=sys.stderr)
        print("ERROR: Original game executable not found!", file=sys.stderr)
        print(f"Path: {game_exe}", file=sys.stderr)
        print("", file=sys.stderr)
        print("Building this project requires a legally owned copy of", file=sys.stderr)
        print("Touhou Koumakyou ~ the Embodiment of Scarlet Devil ver 1.02h.", file=sys.stderr)
        print("", file=sys.stderr)
        print("Please place 'kouma.exe' (ver 1.02h) at 'resources/game.exe' and retry.", file=sys.stderr)
        print("=" * 70, file=sys.stderr)
        sys.exit(1)

    size = game_exe.stat().st_size
    if size != EXPECTED_SIZE:
        print("=" * 70, file=sys.stderr)
        print(f"ERROR: File size mismatch for {game_exe}!", file=sys.stderr)
        print(f"  Expected: {EXPECTED_SIZE} bytes (ver 1.02h)", file=sys.stderr)
        print(f"  Actual:   {size} bytes", file=sys.stderr)
        print("Please make sure you are using Touhou Koumakyou ver 1.02h.", file=sys.stderr)
        print("=" * 70, file=sys.stderr)
        sys.exit(1)

    h = hashlib.sha256()
    with open(game_exe, "rb") as f:
        while chunk := f.read(65536):
            h.update(chunk)
    actual_hash = h.hexdigest().lower()

    if actual_hash != EXPECTED_SHA256.lower():
        print("=" * 70, file=sys.stderr)
        print(f"ERROR: SHA-256 hash mismatch for {game_exe}!", file=sys.stderr)
        print(f"  Expected: {EXPECTED_SHA256}", file=sys.stderr)
        print(f"  Actual:   {actual_hash}", file=sys.stderr)
        print("Please ensure you are using an unmodified ver 1.02h executable.", file=sys.stderr)
        print("=" * 70, file=sys.stderr)
        sys.exit(1)

    return game_exe


if __name__ == "__main__":
    verify_original_game_exe(Path(__file__).parent.parent)
    print("OK: resources/game.exe verified (ver 1.02h).")
