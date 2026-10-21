# Status (th06_new)

## Goal
Vulkan backend port of TH06 v1.02h. Build: `python3 scripts/build.py all -j"$(nproc)"`, deployables in build/ (th06util.exe etc.) → user copies to `~/Desktop/th/kouma` (wine). User runs manually; NO auto-testing of the game. Report in Japanese.

## New Bug: Fog effect renders black / wrong-colour
RESOLVED-WITH-REVERT (21:10): user said the fog/clear push-order change made backgrounds abnormal → REVERT BOTH (fogColor push back to RGBA order, Clear() back to RGBA order; deployed 21:10). NOTE: original fog order turned out to be what the user considers normal; the actual complaint was shot-hit EFFECT colours all along. Do not re-apply without user-confirmed evidence.

## CURRENT FOCUS: "stage 3 = emerald green / stage 6 = black" observation (21:3x user, after half-texel fix)
**UPDATE 22:16 — SECOND root cause found & fixed (table selection INVERTED), deployed 22:16:**
- The boss-hit/spell-declare FX colour = `g_EffectsColor[effectColorId]` (BulletManager.cpp:606-613 RegisterChain). The fork branched `bit==0 → WithTextureBlending / bit==1 → WithoutTextureBlending` — i.e. hwblend-bit SET picked the PALE table. User log proves the run state: `CFG opts=0x1 hwblend=1` (Supervisor.cpp:1408 default sets bit=1; vanilla default) AND the observed spell-declare spiral draws `draw3 tex=7 spi=690 (etama4 sprite 24, script 18) blend=2 color=0xffe0e0ff` ×774 = exactly g_EffectsColorWithoutTextureBlending[9..11] → the pale/without-table was in use.
- Vanilla semantics (consistent with Supervisor.cpp:1411 DIAG comment: COLOROP switches to ADD only when BOTH hwblend+nocomp are UNSET; i.e. bit=1 = MODULATE/TFACTOR HW texturing): bit=1 (default) must use **g_EffectsColorWithTextureBlending** (vivid per-FX colours, e.g. [5]=0xffff2020 red, [11]=0xff2020ff bright blue, [16]=0xff009000 green); the without-table only holds pale fallback tints ([9..11]=0xffe0e0ff, [16]=0xffe0ffe0…). Extra proof: stage-4 Patchouli spell ids {4,10,16,22,25} map to five DISTINCT vivid colours in the with-table vs near-identical pale tints in without — a colour-coded 5-element spell deck must be the vivid table.
- FIX: swapped the two branches in BulletManager::RegisterChain (bit!=0 → With table). Default init BulletManager.cpp:26 stays With (consistent). Built + deployed 22:16 (th06util.exe + kouma_patch.delta). Expected after fix: st3 spell FX red/dark-purple/dark-blue family (ids 5,6,9,10,11), st6 = bright blue (id 11). Note: ECL EFFECTPARTICLE colours (pastel 0xff80ff80 green / 0xff8080ff / 0xffff8080 …) are DIRECT ECL values in EclManager.cpp:819-822, NOT table-mediated — pastel hit-glow tints are vanilla data.
- STILL OPEN: identity of the tex=0 blend=1 black-rgb orbiting ring overlays at boss-hit windows (positions radius ~55, 22.5°/frame, alpha fade 0x10..0xeb — shape matches EffectUpdateCallback4 orbit but tex=0 suggests the white-fallback texture is being used, i.e. the script's sprite index maps into a texture that isn't loaded/valid, or these are a different FX family, not script 18); VK missing stage-sky-fill hypothesis (line 15) unverified; fog channel-order re-check note (line 14) still pending spawn a VK-spec check.
EVIDENCE FROM USER LOG (21:34 run of the 21:29 build — contains draw3/FOGW/FLASH diags):
- Hit sparkle draws healthy: tex=7 (etama4) sprites 666..690 (glow spi=670 rect 30x30, stars 673..680 rects 14x14), blend=2 BLEND_ADD, color white fading alpha (0xa5..0xff) — data path correct; slide to texture half-texel fix already deployed.
- Boss-damage flash window draws (sniffer armed=30 per hit): THREE overlays at tex=0 (text.anm white/generic): red c0=0xe0ff3030 (add-style red, EnemyManager boss colorOp=ADD path), bright white e0-0xf0ffffff, and a **16-spokes rotating ring of BLACK quads**: c0 = (0x10,0x32,0x40,0x50...)0000 — rgb=BLACK with alpha FADING 16 steps, positions orbit radius ~55 at 22.5°/draw → **this is the EclManager.cpp:556 SPELLCARDEFFECT FX: SpawnParticles(0xd, g_Effects[13]=SCRIPT_18 quaternion-orbit callback) with color = g_EffectsColor[effectColorId] (28-entry color table from the original exe: ids 0-5 black/dark-red family, 15-20 green family etc.)**. The Black RGB is *in the table* (id0=0xff000000). Stage-dependence (st3 green vs st6 black) plausibly = per-spell effectColorId from ECL data (Meiling=green-family ids, Flandre=dark ids) — NEEDS CONFIRMATION whether users' vanilla memory says these spells' rings are green/black too **or whether the table/byte-order got extracted wrong (ARGB vs BGRA misread swaps R/B) or the ECL colorId mapping is off**.
- NO FULLFOG lines at all and near all CLEAR → fog wasn't actively mixing in any sampled draw in that run; stage fog states in log: st3 fog=0xffc8a880 near/far=1000/2000, st6 fog=0xff6040b0 near/far=1000|2000 (and 600/1000 for some draws).
- BIG CORRECTION of an earlier assumption: VULKAN B8G8R8A8_UNORM sampling semantics = shader .r = byte2 = the RED byte (comp labels B,G,R,A map to byte order; .r = component "2"? no — .r = the component named "R" = byte 2 of layout)... **this needs re-verification**; under that model the current fogColor push order [R,G,B] is CORRECT and the earlier BGR-swap experiment was itself the anomaly (matches user's earlier "background abnormal" report). Treat PUSH-ORDER = settled CORRECT pending re-check of the VK spec indexing (VK_FORMAT_B8G8R8A8_UNORM: memory bytes [B,G,R,A]; shader swizzle .r = ?). If .r = byte0 (B) then push must be [B,G,R] and the current fog/clear are both channel-swapped (weinstein — the st3 tan→periwinkle symptom would follow).
- ScreenEffect::Clear(bool), GameWindow D3D skyFog fill at GameWindow.cpp:446 has **no Vulkan-equivalent stage-sky clear anywhere** (only black Clear(0,0,0) at BeginScene + mid-scene clears); GameManager.cpp:242 and GameWindow.cpp:446 D3D Clear(skyFog) calls are D3D-only → **stage sky fill may be MISSING in Vulkan!** (st6 "black" could be the missing sky fill; needs check of what fills the sky region in VK path today).
NEXT: ask user which visual is green at st3 / black at st6 (hit sparks? spell-declare ring FX? background/sky? whole screen?) and whether it appeared after the half-texel fix; then chase: (a) fog/clear channel-order + (b) missing stage-sky fill in VK path + (c) ECL effectColor table byte order.
USER SYMPTOM CONFIRMED (21:2x): "B"= dark/sunken (stars look black, glow gone). NOT hue inversion; boss-hit red flash not the complaint either (intended).
ROOT CAUSE FIXED (built+deployed 21:29, awaiting user verification):
- stage3d.frag (used by ALL Draw3/3D quads incl. hit sparks) was MISSING the D3D8 texel-centre correction that sprite.frag (2D) already had: `texture(sampler, fragTexCoord + vec2(pc.halfTexelU, pc.halfTexelV))`.
- Consequence: every Draw3 quad sampled exactly BETWEEN two texels (uv=k/size addresses inter-texel edge in Vulkan, whereas D3D8 addresses texel k at u=k/size). With the shared LINEAR sampler each 3D sample blended in the neighbouring texel row. Hit sparks are 14x14 star sprites surrounded by black texels (etama4.png rows y=241: white/red/green/blue stars verified per-pixel bright, alpha from etama4_a.png blue=255) → half-black neighbour mix = dark/blurry sparks (user symptom B). Also subtly darkened/blurred ALL 3D sprites (stage/boss/player).
- FIX: added halfTexelU/halfTexelV to PushConstants3D, filled in VulkanRenderer::DrawQuad3D (0.5/width, 0.5/height of textures_[tex], 0.0 for invalid/white textures, mirroring the 2D batch path), added uv+halfTexel sampling in stage3d.frag. Rebuilt (compile_shaders + build.py all OK) and deployed to ~/Desktop/th/kouma (th06util.exe + kouma_patch.delta 21:29).
- Blend facts verified along the way: etama4 sparkle scripts all BLEND_ADD(vm flag One) → mapped to BLEND_ADD → stagePipelineAdd(NoZ) = (SrcAlpha, One) ✓; white vm color 0x80FFFFFF; alpha interp interpolates all 4 RGBA comps between vm->color and alpha-changed copy (RGB constant) ✓; Rng::GetRandomU16InRange(4)=0..3 ✓; star texture pixels bright ✓. So the ONLY defect found on this path was the missing half-texel.
INVESTIGATION HISTORY (kept for context):
- Extracted & decoded data/etama4.anm (from 紅魔郷CM.DAT; PBG3+bitstream extractor /tmp/opencode/pbg3_extract.py; minimal PNG decoder /tmp/opencode/pngdec.py — note: PNG chunk length is big-endian '>I'). Hit sparkles = g_Effects[4..7] → etama4 scripts 9..16: SPRITE_RAND(7|11|15|19, 4) star dice rows of DIFFERENT colours (14x14 sprites y=241, rows x=1/65/129/193) + ZWRITE_DISABLE(1) + BLEND_ADD + ALPHA(0x80) + ALPHA_INTERP_LIN fade + SCALE 3 + rot.
- g_Effects[3] (hit glow) = script 6: sprite 6 (30x30) BLEND_ADD scale 3→ shrink -0.075/f, alpha fade 0→40f.
- On-hit sparkle spawn: Player.cpp 837/846/861 SpawnParticles(PARTICLE_EFFECT_UNK_5(any-of), COLOR_WHITE); UNK_3=GLOW_1 at 884.
- Boss-hit red flash draws: FLASH 3D tex=0 c0=0xe0ff3030 (additive red 0xFF3030 @88% alpha, EnemyManager.cpp:1732 colorOp ADD path) — intended vanilla behaviour.
UNKNOWN/TODO: user verification of the half-texel fix (ask: do hit sparks look bright/normal now; also check stage/boss sprites for sharpness change). If still dark, next suspects: functools of ALPHA_INTERP on color (unlikely now), additive alpha semantics.

## Current Bug 2: Effect colours change (boss-hit flash / invulnerability blink / lasers look wrong)
ROOT CAUSE FIXED (built 20:xx, awaiting user verification):
- AnmManager::DrawInner / Draw2 / Draw3 USE_VULKAN paths early-returned WITHOUT calling SetRenderStateForVm(vm) (the D3D8 path calls it before every draw).
- SetRenderStateForVm is what forwards vm->flags.colorOp (D3DTOP_ADD vs MODULATE, via dummy device COLOROP→SetColorOpAdd) and vm->flags.zWriteDisable (ZWRITEENABLE→SetDepthWrite).
- Result: every AnmVm drew with MODULATE regardless of script. Effects that the original renders ADDITIVE-bright (EnemyManager.cpp:1732 boss-hit flash, Player.cpp:733 invulnerability blink 0xff404040, BulletManager.cpp:1420 laser vm1, MainMenu/ResultScreen) rendered modulated → colours changed/dark. This is also the real answer to the earlier boss-hit-flash hunt (COLOROP DIAG in dummy_d3d8.cpp was in place but nothing on the Vulkan path ever reached it).
- FIX: added `this->SetRenderStateForVm(vm);` before the g_Renderer draw call in all three Vulkan paths (same placement as the D3D8 path). Config-bit gating (GCOS_USE_D3D_HW_TEXTURE_BLENDING / GCOS_NO_COLOR_COMP) is preserved because the original function performs it.
- Note: colorOp=ADD semantics in sprite.frag/stage3d.frag = tex.rgb+vert.rgb (alpha tex.a*vert.a) matches D3D8 COLORARG1=TEXTURE/COLORARG2=TFACTOR|DIFFUSE + ALPHAOP=MODULATE. Batch key already includes colorOpAdd so batching splits correctly.
- Known minor pre-existing gap (not this bug): dummy device maps COLOROP SELECTARG1 (NO_COLOR_COMP option) to MODULATE; only matters if that compat bit is set.
- User retests effects (boss damage flash, player blink during invulnerability, lasers, spell backgrounds).

## Previous Bug: Option screen BGM selector shows only "Off" (MIDI/Wav rows missing)
ROOT CAUSE IDENTIFIED & FIX PENDING USER VERIFICATION (deployed 13:47):
- title04.anm (ANM_FILE_TITLE04=26, texture data/title/title04.png 256x256) has sprites that INTENTIONALLY extend beyond the texture (rects up to x=496, e.g. id=14..16 at x 256..496). TH06原版 relies on the REPEAT sampler to wrap uv>1 back onto the texture (uv/256 mapping, dup of left-hand label art).
- The fork's Vulkan LoadAnm "clampToEdge" heuristic marked these as border-touching and disabled REPEAT -> uv 1.0+ clamps to the edge texel -> broken/garbage right-side labels on the option screen.
- FIX: LoadAnm now distinguishes anySpriteNeedsWrap (rect strictly beyond anm width/height + 0.25 epsilon) from mere border-touch; wrap required for needsWrap or UV-scroll textures.
- Verified data by extracting 紅魔郷TL.DAT with a python PBG3+LZSS parser (/tmp/opencode/pbg3_extract.py, mirrors src/pbg3/*). title04.anm sprite table matches runtime logs exactly; PNG IHDR=256x256.
- User retests option screen; if fixed, mark bug resolved and clean diagnostics later.

## Second Bug: No audio in Vulkan build — RESOLVED by user report (13:40-ish)
- User confirms sound plays now ("Vulkanで音が鳴る"). No code change attributed; treat as resolved but keep an eye on regressions.

## Files/deploy
- Deployed th06util.exe 13:47 (wrap fix + diagnostics).
- Logs: `~/Desktop/th/kouma/log_spell_practice.txt`, `log.txt`, `THPR log pr_*.log`.
- PBG3/LZSS parser: `/tmp/opencode/pbg3_extract.py <archive> <name>` (C++-mirror field order: unk2,unk1,checksum,dataOffset,uncompSize,then string). Archives: 紅魔郷TL.DAT etc. in the game dir. title04.anm extracted at /tmp/opencode/title04.anm.

## Conventions
- No manual/auto game testing by me; always ask user to run.
- Do not touch visual/font layout; label positions are user-owned.
- Commit only when user explicitly says OK; otherwise leave changes unstaged.

## UPDATE 10-21 morning (diag rebuild + PBG3/LZSS toolchain + anm decode breakthrough)
- DEPLOYED 08:19 diagnostic build to ~/Desktop/th/kouma (th06util.exe + kouma_patch.delta). Armed-window (boss-damage 30f) instrumentation upgraded:
  (a) EffectManager::OnDraw dumps EVERY live effect VM: "EFFECT[idx] sp vis rot scale color blend pos";
  (b) AnmManager Draw3 diag caps (file4<1600/file7,11<800/file8,9<800) are BYPASSED while armed → full per-draw log (prev caps were consumed by star draws so glow/spiral colors never appeared in log);
  (c) vulk SniffSuspectQuad adds SNIFFT teal detector (g,b>=0x60, r*3<=g,b) for the screenshot blob hue.
  → NEXT: user runs spell practice + lets boss hit once; analyze new log.
- LZSS BUG FOUND IN MY TOOLS (fork code was RIGHT): LZSS_DICTSIZE = 0x2000 (8192-byte ring, mask 0x1fff), NOT 4096. My python extractor used 4096 → matches with offset>=4096 read wrong history → PNG IDAT corrupt from ~16.7k bytes. With 8192: etama4.png (30224B), etama4_a.png (10318B), etama3.png (75323B), etama3_a.png (13956B) all decode: exact sizes, ALL chunk CRCs OK, full inflate to pixels. /tmp/opencode/*.pixels.bin + *.ref saved. Game-side textures therefore decode correctly in-engine; blobs NOT caused by LZSS.
- etama4.anm FULLY parsed (21 scripts; layout {i16 time; u8 opcode; u8 argsByteSize; argsØ; stride 4+argsByteSize}): sprite 0-3 = 64x64 quads (each quadrant has a distinct soft tint: q0 (60,67,67) cyan-ish, q1 (88,67,60) warm, q2 (67,60,67), q3 (60,76,67) greenish — RGB from etama4.png, shape/alpha from etama4_a.png mask); sprites 4-6 = (65/97/129,209) 30x30 glows; 23 = (1,176) 63x63 big glow; 24 = (161,225) 14x14 star; 7-22 = 16 stars 14x14 @y241. ALL glow/star/spiral scripts = ZWRITE_DISABLE(1) + BLEND_ADDITIVE + AlphaInterp fade; script18 = spiral (sprite24, RotationSpeed(0,0,-0.15), Alpha@60/interp@120/180, Jump 36 loop).
- OPEN ANOMALY (potentially blob-relevant): fork's in-game texture memory ≠ my CRC-verified raw PNG pixel data. Log "TEXLOAD avg": etama4=(50,48,45) vs mine (34,34,33); etama4_a=(69,69,69) vs mine (35,35,35) (~2x); etama3=(127,113,112) center=(67,54,54) vs mine (49,51,51) center=(0,0,0) (~2.6x + warmer hue). Fork alpha-bleed dilation simulated → (32,32,32), doesn't explain. Candidate areas: stb RGBA→BGRA conversion, LoadTextureAlphaChannel beta/alpha-source mapping, or which file actually gets loaded. To dig further after the armed-run log arrives.

## Diagnosis milestone (10-21, after SNIFFT run)
- SNIFFT(ティール検出) 50件すべて = スペルバーしきい値tickクアッドのみ:
  `SNIFFT 3D tex=0 blend=1 c0=c2=0xff30d0ff pos=(-29..-30,45..46,0) uv=(0,0)-(0,1)`
  (tick = 色D3DCOLOR_ARGB(0xFF,0x30,0xD0,0xFF)=シアン, 半径54-62, EnemyManager.cpp:2153)。
  POV: pos はリングローカル座標(半径≈54円上=ボスring)。tick は縦8px×円弧0.9°で本来ほぼ1px。
- 2Dパス・effect VM・draw3 にティール色は一切なし(draw3色は white glows 0xNNffffff + 0xff2020ff spirals のみ)。
  ⇒ blob(暗ティール)の唯一のティール発生源 = tick。ただし blob は大きくtickは本来小さい=描画側のバグの可能性。
- DrawQuad3D は quadごと 6 indices / 個別 vkCmdDrawIndexed(シームリークなし)。fog は w≈836 CLEAR。

### Blob一変量実験ビルド (10-21 09:05 deploy)
- tick色をマゼンタに変更 (EnemyManager.cpp DIAG(tickcolor)) + THRESH リストログ。
- ユーザー実行待ち:
  - blobがマゼンタに見えたら ⇒ tick=blobの正体 ⇒ tick描画のバグ探し(3 segment/scale/world)。
  - 変わらなければ ⇒ tickは無関係 ⇒ 次は insideColor(0xF0FFFFFF) ring か地点特定に着色実験。
- 次の仮説(未検証):鄂bossの threshold callback が多数登録され(THRESH n=で確認)、semi透明でなくα=FF で積み重なっている可能性。
