#!/usr/bin/env python3
"""MinGWビルド成果物が、静的リンクできないDLL以外に依存していないか検査する。

libwinpthread-1.dll / libgcc_s_*.dll / libstdc++-6.dll などがimport tableに
残っていたら失敗扱いにする（-static が効いていない、または libwinpthread.a が
無い環境でも黙って動的リンクに戻らないようにするため）。
"""
import re
import subprocess
import sys
from pathlib import Path

OBJDUMP = "i686-w64-mingw32-objdump"

# 静的リンクできないDLL（OS標準 + DirectX 8）。小文字で比較する。
ALLOWED_DLLS = {
    "kernel32.dll", "user32.dll", "gdi32.dll", "shell32.dll", "ole32.dll",
    "oleaut32.dll", "advapi32.dll", "imm32.dll", "ws2_32.dll", "iphlpapi.dll",
    "winmm.dll", "msvcrt.dll", "bcrypt.dll", "comctl32.dll", "comdlg32.dll",
    "d3d8.dll", "dinput8.dll", "dsound.dll", "dxerr8.dll",
    # OS-provided DLLs pulled in by the statically-linked SDL2 (Vulkan backend).
    "version.dll", "setupapi.dll", "cfgmgr32.dll",
}
# D3DX8 はバージョン違い（d3dx8d.dll など）があるので前方一致で許可する。
ALLOWED_PREFIXES = ("d3dx8",)


def list_imported_dlls(path: Path) -> list[str]:
    r = subprocess.run([OBJDUMP, "-p", str(path)], capture_output=True, text=True)
    if r.returncode != 0:
        print(f"{OBJDUMP} failed on {path}:\n{r.stderr[:1000]}")
        sys.exit(1)
    return re.findall(r"DLL Name:\s*(\S+)", r.stdout)


def check_static_deps(path: Path) -> None:
    dlls = list_imported_dlls(path)
    bad = [
        d for d in dlls
        if d.lower() not in ALLOWED_DLLS and not d.lower().startswith(ALLOWED_PREFIXES)
    ]
    if bad:
        print(f"Static link check FAILED for {path.name}: unexpected DLL imports:")
        for d in bad:
            print(f"  - {d}")
        sys.exit(1)
    print(f"    static check OK ({path.name}: {', '.join(sorted(set(dlls), key=str.lower))})")


if __name__ == "__main__":
    for p in sys.argv[1:]:
        check_static_deps(Path(p))
