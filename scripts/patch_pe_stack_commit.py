#!/usr/bin/env python3
"""PE の SizeOfStackCommit フィールドを書き換えるスクリプト。
MinGW ld の --stack は reserve のみ変更できるため、commit はここで直接書き換える。

使用法: python3 scripts/patch_pe_stack_commit.py <exe> <commit_bytes>
例:     python3 scripts/patch_pe_stack_commit.py build_vulkan_windows/th06_vulkan.exe 0x100000

PE32 Optional Header レイアウト (ImageBase=DWORD 版):
  +0x00  Magic            (WORD)  0x10b
  +0x02  MajorLinkerVersion
  +0x03  MinorLinkerVersion
  +0x04  SizeOfCode
  +0x08  SizeOfInitializedData
  +0x0c  SizeOfUninitializedData
  +0x10  AddressOfEntryPoint
  +0x14  BaseOfCode
  +0x18  BaseOfData       (PE32 only)
  +0x1c  ImageBase        (DWORD, PE32)
  +0x20  SectionAlignment
  +0x24  FileAlignment
  +0x28  MajorOperatingSystemVersion
  +0x2a  MinorOperatingSystemVersion
  +0x2c  MajorImageVersion
  +0x2e  MinorImageVersion
  +0x30  MajorSubsystemVersion
  +0x32  MinorSubsystemVersion
  +0x34  Win32VersionValue
  +0x38  SizeOfImage
  +0x3c  SizeOfHeaders
  +0x40  CheckSum
  +0x44  Subsystem        (WORD)
  +0x46  DllCharacteristics (WORD)
  +0x48  SizeOfStackReserve (DWORD)
  +0x4c  SizeOfStackCommit  (DWORD)  ← ここ
"""
import struct
import sys

def patch_stack_commit(path: str, commit: int) -> None:
    with open(path, 'r+b') as f:
        data = bytearray(f.read())

    # DOS ヘッダの e_lfanew (PE シグネチャへのオフセット)
    e_lfanew = struct.unpack_from('<I', data, 0x3c)[0]

    # PE シグネチャ確認
    sig = bytes(data[e_lfanew:e_lfanew+4])
    assert sig == b'PE\x00\x00', f"Not a PE file (sig={sig!r})"

    # Optional Header の開始位置 = e_lfanew + 4 (sig) + 20 (COFF) = e_lfanew + 24
    opt_hdr = e_lfanew + 24

    # Magic: 0x10b = PE32
    magic = struct.unpack_from('<H', data, opt_hdr)[0]
    assert magic == 0x10b, f"Expected PE32 (0x10b), got 0x{magic:x}"

    reserve_offset = opt_hdr + 0x48
    commit_offset  = opt_hdr + 0x4c
    old_reserve = struct.unpack_from('<I', data, reserve_offset)[0]
    old_commit  = struct.unpack_from('<I', data, commit_offset)[0]

    struct.pack_into('<I', data, commit_offset, commit)

    with open(path, 'wb') as f:
        f.write(data)

    print(f"Patched {path}: SizeOfStackCommit 0x{old_commit:x} -> 0x{commit:x} "
          f"(SizeOfStackReserve=0x{old_reserve:x})")

if __name__ == '__main__':
    if len(sys.argv) != 3:
        print(f"Usage: {sys.argv[0]} <exe> <commit_hex_or_dec>")
        sys.exit(1)
    exe = sys.argv[1]
    val = int(sys.argv[2], 0)
    patch_stack_commit(exe, val)
