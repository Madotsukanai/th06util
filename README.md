# th06UtilityTool

> [!NOTE]
> **English Documentation:** [English version is available here (README_en.md)](README_en.md)

東方紅魔郷 ver1.02h (SHA-256: 9f76483c46256804792399296619c1274363c31cd8f1775fafb55106fb852245)の実行ファイルに適用する動作改善及び機能拡張用パッチです。  

---

## 導入方法・使い方

1. [Releases](../../releases) より最新版の `th06UtilityTool.zip` をダウンロードして解凍します。
2. 解凍したファイル（`launcher.exe`, `kouma_patch.delta`, `th06_spell_practice.dll`, `custom_new.ini`）を、正規の**東方紅魔郷 ver1.02h**（`東方紅魔郷.exe` があるフォルダ）と同じディレクトリに配置します。
3. `launcher.exe` を起動します。自動的にパッチが適用され、拡張版が起動します。

---

## 主な機能

- **画面・描画**
  - フルスクリーンモードのボーダーレスウィンドウ化（マルチモニター環境でも快適に動作）
  - OBS等のキャプチャソフトによるゲーム画面出力・配信対応
  - フレームレートの安定化及び表示更新頻度の上昇
  - Alt + Enter でのウィンドウ／フルスクリーン即時切り替え
  - ウィンドウサイズの任意指定（`window_width`, `window_height`）
  - ウィンドウ位置の自動記憶・次回起動時の復元（`window_pos_x`, `window_pos_y`）
  - 垂直同期（VSync）の有効化／無効化切り替え
  - ボスの残り HP を視覚的に把握できる円形ゲージ表示（`boss_hp_circle`）
  - 画面下部にボスの位置を示すエネミーマーカー表示（`enemy_marker`）
  - 画面右上に難易度（Normal, Hard 等）を常時表示（`show_difficulty`）
  - 画面右上に経過フレーム数をリアルタイム表示（`show_frame_counter`）
  - 縦画面（TATE）モード対応（時計回り・反時計回り90度回転）
  - スリープ復帰後に描画が停止する問題の解消

- **操作・入力**
  - 入力遅延の大幅な改善
  - ゲームウィンドウフォーカス時の IME 半角／全角キー切り替え無効化（誤爆防止）
  - 起動直後のキー入力暴発抑制
  - タイトル画面で「Quit」にカーソルがある時に X キーでゲーム終了
  - リプレイメニューにてDeleteキーでリプレイファイルの削除が可能
  - 任意のキーコンフィグ対応（`custom_new.ini` で DirectInput キーコードを直接割り当て可能）
  - 会話スキップボタン（`skip`、デフォルト: LCtrl）のキー割り当て対応
  - アケコン配置向けの低速移動ボタン右端移動（`move_shift`）

- **ゲームプレイ・練習・通信**
  - 同梱 DLL によるスペルプラクティス機能（Practice 画面で Shift を押しながら決定）
  - オンラインマルチプレイ機能（Host/Guest P2P 通信による遠隔対戦）
  - thprac リプレイとの互換性確保
  - リプレイ再生時の倍速送り機能（最大5倍速）
  - ポーズ画面での R キー、および「左＋ショット＋低速移動」同時押しでの高速リトライ
  - ポーズ画面での Q キーによる即時ゲーム終了
  - 自機および敵弾の当たり判定表示（`show_collision`, `hitdisp`）
  - 入力キーのリアルタイム表示（`show_input`）

- **サウンド・システム**
  - **F1 キーによるクイック設定メニュー（ゲーム内オーバーレイ）の実装**
    - ゲームを止めることなく、描画・音声・システム・BGMタイプの各設定をリアルタイムに変更可能
  - BGM（WAV再生時）および SE の個別音量調節（0～100%）
  - MIDI 再生時の個別 BGM 音量調節（0～100%）
  - 各楽曲ごとの個別 MIDI / WAV 再生切り替え対応（全17曲を個別に指定可能）
  - 内蔵 SoundFont シンセサイザー（FluidSynth）対応（外部 MIDI 音源なしで高品質な演奏が可能）
  - SoundFont フォルダの任意指定、およびメニュー内 GUI ダイアログからのフォルダ／ファイル選択
  - ポーズ画面での BGM 一時停止及び解除時の途中再生
  - セーブデータ（`score.dat`）の自動世代バックアップ機能（世代数指定可能）

---

## 一部追加機能の詳述

### スペルプラクティス
ゲームディレクトリに `th06_spell_practice.dll` を置いた状態で、**Practice メニューで Shift キーを押しながらステージを決定**するとスペルプラクティスメニューが出現します。ここから任意のスペルカードを選択してプレイできます。

### オンラインプレイ（試験的機能）
同一LAN内でのマルチプレイに対応しています（異なるLAN環境での動作は検証中）。  
通常のゲームプレイ時、**Shift キーを押しながらショットタイプを決定**すると「Host」「Guest」の選択肢が出現し、マッチング設定を行えます。

---

## 設定ファイル（custom_new.ini）の解説

同梱されている `custom_new.ini` には本パッチ独自の設定項目が保存されています。テキストエディタで自由に編集可能です。基本的に `0` がオフ、`1` がオンです。

また、これらの設定値は**ゲーム中に F1 キーを押すと表示されるクイック設定メニュー**からも変更できます。「描画」「システム」「音声」「BGMタイプ」の各カテゴリから即座に切り替え可能です。

### [display]

| 項目名 | 説明 |
|---|---|
| `show_input` | 画面右下付近に入力を表示します（4方向、ショット、スペルカード、低速移動）。 |
| `move_shift` | 入力表示内の低速移動ボタンの位置を一番右に移動させます（アケコン向け）。 |
| `window_width` | ウィンドウの横幅をピクセル単位で指定します。 |
| `window_height` | ウィンドウの縦幅をピクセル単位で指定します。 |
| `window_pos_x` | ウィンドウの表示 X 座標を指定します（終了時にも自動更新）。 |
| `window_pos_y` | ウィンドウの表示 Y 座標を指定します（終了時にも自動更新）。 |
| `show_collision` | 当たり判定を表示します。※オンにするとスコア・リプレイの保存が無効化されます。 |
| `hitdisp` | 敵弾の当たり判定を赤い矩形で表示します。※オンにするとスコア・リプレイの保存が無効化されます。 |
| `vsync` | VSync（垂直同期）を有効にします。ティアリングは減少しますが遅延が増える場合があります。 |
| `boss_hp_circle` | ボスの残り HP を円状インジケーターで表示します。 |
| `enemy_marker` | 画面下部にエネミーマーカー（三角形）を表示します。※オンにするとスコア・リプレイ保存が無効化されます。 |
| `show_difficulty` | 画面右上に難易度（Normal, Hard 等）を常時表示します。 |
| `show_frame_counter` | 画面右上に経過フレーム数をリアルタイム表示します。 |
| `show_actual_deaths` | リザルト画面のミス数横に実ミス数（デスボム分を除いたミス数）を表示します（デフォルト: `1`）。 |
| `vertical_mode` | 縦画面モードを指定します（`1`: 時計回りに90度回転、`2`: 反時計回りに90度回転）。 |

### [system]

| 項目名 | 説明 |
|---|---|
| `backup` | セーブデータ（`score.dat`）の自動バックアップ機能（`0`: 無効、`1`: 有効）。 |
| `backup_keep_count` | 保持するバックアップ世代数の最大値（デフォルト: `30`）。 |

### [audio]

| 項目名 | 説明 |
|---|---|
| `bgm_volume` | BGM（WAV再生時）の音量を 0～100 で指定します。 |
| `se_volume` | SE（効果音）の音量を 0～100 で指定します。 |
| `internal_synth` | 内蔵 SoundFont シンセサイザー（FluidSynth）の使用設定（`1`: 内蔵 FluidSynth、`0`: 外部 MIDI 音源 / WinMM）。 |
| `bgm_volume_midi` | MIDI 再生時の BGM 音量を 0～100 で指定します。 |
| `soundfont_dir` | SoundFont（.sf2）を読み込むフォルダパス（デフォルト: `soundfonts`）。F1メニューの「MIDI設定」→「SF2フォルダ」からGUIでフォルダ選択も可能。 |
| `sf2` | 使用する SoundFont のファイル名（ファイル名のみ指定）。F1メニューの「MIDI設定」→「SoundFont」から左右キーまたは決定ボタン（GUIファイル選択）で指定可能。未指定またはファイル不在時は静的内蔵された `GeneralUser-GS.sf2` を自動使用。 |

### [bgm]

| 項目名 | 説明 |
|---|---|
| `track01` ～ `track17` | 各曲ごとの再生タイプを個別に指定します（`0`: MIDI、`1`: WAV）。F1メニューの「BGMタイプ」からも曲ごとに切り替え可能です。 |

### [input]

各操作のキー割り当てを DirectInput スキャンコードで変更できます（未指定時はデフォルト値）。

| 設定項目 | デフォルトキー | キーコード |
|---|---|---|
| `shoot` | Z | 44 |
| `bomb` | X | 45 |
| `focus` | LShift | 42 |
| `menu` | Escape | 1 |
| `up` | Up / W | 200 / 17 |
| `down` | Down / S | 208 / 31 |
| `left` | Left / A | 203 / 30 |
| `right` | Right / D | 205 / 32 |
| `skip` | LCtrl | 29 |

<details>
<summary>DirectInput キーコード早見表</summary>

| コード | キー | コード | キー | コード | キー | コード | キー |
|---|---|---|---|---|---|---|---|
| 1 | Escape | 2 | 1 | 3 | 2 | 4 | 3 |
| 5 | 4 | 6 | 5 | 7 | 6 | 8 | 7 |
| 9 | 8 | 10 | 9 | 11 | 0 | 12 | Minus |
| 13 | Equals | 14 | Back | 15 | Tab | 16 | Q |
| 17 | W | 18 | E | 19 | R | 20 | T |
| 21 | Y | 22 | U | 23 | I | 24 | O |
| 25 | P | 26 | LBrack | 27 | RBrack | 28 | Return |
| 29 | LCtrl | 30 | A | 31 | S | 32 | D |
| 33 | F | 34 | G | 35 | H | 36 | J |
| 37 | K | 38 | L | 39 | Semi | 40 | Apost |
| 41 | Grave | 42 | LShift | 43 | BackSl | 44 | Z |
| 45 | X | 46 | C | 47 | V | 48 | B |
| 49 | N | 50 | M | 51 | Comma | 52 | Period |
| 53 | Slash | 54 | RShift | 56 | LAlt | 57 | Space |
| 58 | Caps | 59 | F1 | 60 | F2 | 61 | F3 |
| 62 | F4 | 63 | F5 | 64 | F6 | 65 | F7 |
| 66 | F8 | 67 | F9 | 68 | F10 | 87 | F11 |
| 88 | F12 | 157 | RCtrl | 184 | RAlt | 71 | Num7 |
| 72 | Num8 | 73 | Num9 | 75 | Num4 | 76 | Num5 |
| 77 | Num6 | 79 | Num1 | 80 | Num2 | 81 | Num3 |
| 82 | Num0 | 200 | Up | 208 | Down | 203 | Left |
| 205 | Right | | | | | | |

</details>

---

## 注意点

> [!WARNING]
> - 本ソフトウェアは ZUN 氏及び上海アリス幻樂団様の関与しない非公式ファンメイド作品であり、予期せぬ不具合によるセーブデータ消失等が発生する可能性があります。
> - 本ソフトウェアを使用して発生した損害については、全て自己責任となる事を予めご了承下さい。
> - 原作の実行ファイル（バイナリ）およびリソースデータ（画像・音声・ステージデータ等の .DAT ファイル）は一切含んでおりません（プレイおよびビルドには製品版の原本が必要です）。
> - セーブデータの定期的なバックアップを取る事を強く推奨します。

## ビルド方法 (開発者向け)

本プロジェクトは Linux (Ubuntu / WSL2) および Windows ネイティブ環境の両方でビルド可能です。

---

### A. Linux / WSL2 環境でのビルド (GNU Make)

#### 必要ツール
- Ubuntu / Debian などの Linux 環境 (WSL2 可)
- `gcc-mingw-w64-i686`, `g++-mingw-w64-i686`, `binutils-mingw-w64-i686`
- `cmake` (3.13 以上)
- `python3`
- `lld` (推奨)
- `ccache` (推奨)
- `zip`

```bash
# 依存パッケージのインストール例 (Ubuntu / Debian)
sudo apt update
sudo apt install -y gcc-mingw-w64-i686 g++-mingw-w64-i686 binutils-mingw-w64-i686 cmake python3 lld ccache zip
```

#### ビルド手順
```bash
# サブモジュールを含めてクローン
git clone --recurse-submodules https://github.com/Madotsukanai/th06util
cd th06util

# 東方紅魔郷 ver 1.02h の原本 (東方紅魔郷.exe) を resources/game.exe として配置
# ※ 必須ゲームデータの抽出およびコンパイル・リンクに必須です。
cp /path/to/東方紅魔郷.exe resources/game.exe

# ビルド (データ抽出・コンパイルが並列実行されます)
make

# 配布用 zip (build/th06UtilityTool.zip) の作成
make package
```

---

### B. Windows ネイティブ環境でのビルド (NMAKE)

Windows 上のコマンドプロンプトや Visual Studio 開発者コマンドプロンプトから `Nmakefile` を用いてビルドできます。

#### 必要ツール
- **Python 3** (Windows 用インストーラーでインストールし、PATH を通してください)
- **MinGW-w64 (32-bit i686)** (MSYS2 の `mingw-w64-i686-gcc`、または [w64devkit](https://github.com/skeeto/w64devkit) などの 32-bit GCC)
- **CMake** (FluidSynth のビルド用)
- **NMAKE** (Visual Studio Developer Command Prompt または Windows SDK に付属)
- **PowerShell** (ZIP パッケージング用・Windows 標準)

#### ビルド手順
コマンドプロンプト（Developer Command Prompt for VS 等）を開き、以下を実行します：

```cmd
:: サブモジュールを含めてクローン
git clone --recurse-submodules https://github.com/Madotsukanai/th06util
cd th06util

:: 東方紅魔郷 ver 1.02h の原本 (東方紅魔郷.exe) を resources\game.exe として配置
:: ※ 必須ゲームデータの抽出およびコンパイル・リンクに必須です。
copy C:\path\to\東方紅魔郷.exe resources\game.exe

:: ビルド (th06util.exe, launcher.exe, patch, dll を一括ビルド)
nmake /f Nmakefile

:: 配布用 zip (build\th06UtilityTool.zip) の作成
nmake /f Nmakefile package
```

---

## ライセンス

本ソフトウェア（th06util）は、[Creative Commons CC0 1.0 Universal](LICENSE)（パブリックドメイン）の下で公開されています。

同梱・使用している各サードパーティ製ライブラリおよびアセットのライセンス詳細・全文については、[LICENSE](LICENSE) をご覧ください。

- **th06util**: CC0 1.0 Universal (パブリックドメイン提供)
- **th06 (GensokyoClub)**: CC0 1.0 Universal (パブリックドメイン)
- **FluidSynth**: GNU Lesser General Public License version 2.1 (LGPL 2.1)
- **xdelta3**: Apache License version 2.0
- **d3d8to9**: BSD 2-Clause License
- **GeneralUser GS SoundFont**: S. Christian Collins (ソフトウェア利用フリーライセンス)
