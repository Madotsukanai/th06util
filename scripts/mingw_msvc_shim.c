// d3dx8.lib (MSVC) が参照するMSVCランタイム内部シンボルのMinGW向け代替。
//
// __alloca_probe: MSVCのスタックプローブ。呼び出し規約(eax=バイト数, espを下げて戻る)は
//                 libgcc の ___chkstk と同一なので、そのままjmpする。
// __except_list : fs:[_except_list] で使われる絶対シンボル(値0)。
__asm__(
    ".text\n"
    ".globl __alloca_probe\n"
    "__alloca_probe:\n"
    "    jmp ___chkstk\n"
    "\n"
    ".globl __except_list\n"
    ".set __except_list, 0\n"
);

// _fltused: MSVCの「浮動小数点を使用」マーカー(d3dx8.libが参照)。
// GNU ld ではライブラリから解決されるが、lld では未解決になるため定義する。
// (重複定義は --allow-multiple-definition で許容済み)
int _fltused = 0x9875;
