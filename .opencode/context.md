# Context notes (chief continuation notes 10-21)

## Objective
TH06 Vulkanフォークのボス被弾エフェクト色異常(暗ティールblob)を修正する。ユーザーは手動でゲーム実行 смогли、結果ログは ~/Desktop/th/kouma/log_spell_practice.txt に追記される。報告は日本語。コミット禁止。/tmp/opencodeはセッション間で消える(重要資料はここに書き写す)。

## Key learnings (do not re-derive)
- LZSS: fork src/pbg3/Pbg3Archive.cpp ReadDecompressEntry は LZSS_DICTSIZE=0x2000(8192リング, mask 0x1fff), dictHead初期1 & (dh+1)&0x1fff 進行, match: `cnt=bits(4)+2; for i in 0..cnt(+1) => out dict[(off+i)&0x1fff]`, off=bits(13), off==0=end。フラグ bit: set=literal(8bit)。私の検証ツール(/tmp/opencode/pbg3_extract.py)はdictを4096にしていた=バグ。8192修正で全PNG完全デコード済。
- etama4.anm: header 64B (numSpr=25, numScr=21, nameOffset=332=data/etama/etama4.png, mipmapNameOffset=364=data/etama/etama4_a.png, width/height=256, format=5), spritePtrArray@64(25), scriptTable@164(21×(id,off))。命令 = {i16 time; u8 opcode; u8 argsByteSize; args; stride = 4+argsByteSize}。opcode表はforkのAnmVm.hpp enum (0=Delete...

— (transcription truncated) —   (0=Delete/Exit, 1=SetSprite, 3=Alpha, 4=Color, 11=ScaleSpeed, 12=AlphaInterp, 13=BlendAdditive, 14=BlendNormal, 16=SpriteSetRand, 17=MovePos, 18-20=MovePosInterp, 27/28=ScrollX/Y, 31=DisableZWrite)。
- etama4.png sprite内容(exact px): sprite0-3 = 64x64 quadrants with different soft tints (q0 rgb~(60,67,67), q1 (88,67,60), q2 (67,60,67), q3 (60,76,67); white cores 255); 4-6=(65/97/129,209) 30x30 glow (avg ~(97..111)); 23=(1,176) 63x63 big glow; 24=(161,225) 14x14; 7-22 = 16 stars @y241 x=1..241. Alphaは別ファイル etama4_a.png の R(=B) チャンネル。
- logに fork の TEXLOAD avg と私のPNG raw値が異なる (etama4: (50,48,45) vs (34,34,33); etama4_a: (69,69,69) vs (35,35,35); etama3: (127,113,112) vs (49,51,51))。dilation上書きでは説明不可。fork テクスチャパイプライン要調査(blob候補)。
- 22:16 表選択fixは有効(spi=690の0xffe0e0ffはそれ以降のセッションで消失)、blobには不十分。
- run-log: log_spell_practice.txt に15セッション亜追記; 最新セッションのみを解析の対象に(awk 'NR>=(最後のCFG opts行番号+1)')。
