# ADV サンプル / 舞台メタ定義 仕様 (stage.tjs)

`data/advsample.tjs`（`-sample` 起動）の宣言層。`data/stage.tjs` の `global.stageDef`
（TJS 辞書、JSON プラグイン非依存）でキャラ・表情・モーション・シーンを宣言する。

## 用語体系（4層）

| 層 | 定義 | 実体 |
|---|---|---|
| **Expression（表情）** | 顔。名前付き or 合成 | VRM表情名 / `{表情名:weight,…}` の束 |
| **Pose（ポーズ）** | 全身の**静止姿勢** | 正規化ボーンの回転集合（データ/採取/VRMA1フレーム） |
| **Motion（モーション）** | **時間つき**の全身動き | VRMA(取込) / **Poseの列＋時間**(合成=poseseq) |
| **Scene（シーン）** | 宣言的な複数キャラ状態＋演出＋カメラ | expr + motion + gaze + 位置 + fx + camera |

補間器（クロスフェード）は共通で効く:
- VRMA 内部: キーフレーム間 nlerp
- モーション開始/切替: 現ポーズ→新モーションへ slerp（`applyAnimationBlend`, ~0.3s）
- モーション停止: 現ポーズ→素立ちへ slerp（`blendToRest`）
- 表情: セット単位の weight 補間（~0.15s）

---

## stage.tjs スキーマ

```tjs
global.stageDef = %[
    "characters"  => [ … ],   // 左右キャラ
    "expressions" => [ … ],   // 合成表情
    "motions"     => [ … ],   // VRMA モーション
    "scenes"      => [ … ]    // 名前付きシーン
];
```

### characters
左右に並べるキャラ（現状2体固定）。
```tjs
%[ "id"=>"left", "name"=>"モデル", "vrm"=>"model", "frontZ"=>1 ]
```
- `vrm`: `data/<vrm>.vrm` の base 名（拡張子なし）
- `frontZ`: +1=VRM1.0 正面(+Z) / -1=VRM0.x

### expressions（合成表情）✅実装済
複数の VRM 表情を weight つきで束ね、1つの名前で一括指定する。
```tjs
%[ "id"=>"grin", "name"=>"にっこり", "set"=>[ "happy",1.0, "relaxed",0.5 ] ]
```
- `set`: `[表情名, weight, 表情名, weight, …]` のフラット配列
- 適用: `id` で呼ぶと set の全表情を weight つきで当てる。単一 VRM 表情との切替も
  同じセット補間でクロスフェード。パネルの「表情」行は 合成表情→VRM単一表情 の順に生成。

#### 生モーフ表情（個別ブレンドシェイプ直接指定）✅実装済
VRM 表情（名前付きモーフ束）ではなく、**個々のブレンドシェイプ（モーフ）を名前で直接**
weight 指定する最下層。作者が用意した表情に無い表情（例: 片目ウインク）を作れる。
```tjs
%[ "id"=>"wink", "name"=>"ウインク",
   "morphs"=>[ "Fcl_EYE_Close_L",1.0, "Fcl_MTH_Fun",0.7, "Fcl_BRW_Fun",1.0 ] ]
```
- `morphs`: `[モーフ名, weight, …]`。VRoid は `Fcl_*` 名（`glTF mesh.extras.targetNames`）。
- `set` と併記可（生モーフ＋VRM表情の合成）。適用は VRM 表情の**後**に override するので
  まばたき等にも上書き勝ちする。
- C++ API: `VRM.setMorph(name,weight)` / `clearMorphs()` / `getMorphCount()` / `getMorphName(i)`。
  列挙で全モーフ名（VRoid=57個）を取得できる。
- **同時モーフ数に上限なし**: threepp fork の GL レンダラを attribute 8 枠方式 →
  **morph テクスチャ方式**（全モーフを RGBA32F テクスチャに詰め `gl_VertexID`+`texelFetch` で
  走査）へ改修済み。旧来の「1メッシュ同時8モーフ」制限を撤廃した。

#### 表情エディタ（生モーフ簡易エディタ）＋ faces.json ✅実装済
advsample の**作業モード「表情編集」**（左パネルを丸ごと差し替え）で、curated モーフ
（眉/目/口/頬）をスライダ編集し、中央ビューにライブ反映しながらキーワード付きで保存する。
- **作業モード軸**: `app.workMode` = `"op"`(操作) / `"face"`(表情編集)。view の tachie/space とは
  直交。パネル最上部の「操作 / 表情編集」ラジオで切替（`buildLayout` が中身を差し替え）。
  face 進入時は対象を顔フレーミング＋idle/目線 off にする。
- **共有 JSON（`data/faces.json`）**: ブラウザ three-vrm エディタと相互運用する形式。
  ```json
  { "version": 1,
    "faces": {
      "wink": { "label": "ウインク",
                "morphs": { "Fcl_EYE_Close_L": 1.0, "Fcl_MTH_Fun": 0.7, "Fcl_BRW_Fun": 1.0 } }
    } }
  ```
  - `faces.<id>.morphs`: `Fcl名 => weight(0..1)` の辞書（stage.tjs の `[名,w,…]` 配列とは別表現。
    ブラウザ連携のため辞書型を採用）。0 のモーフは省いて保存。
  - 読み書きは **json.dll**（`Scripts.evalJSONStorage` / `saveJSON`。書込は bare 名で data/ 直下）。
- **API（advsample の ADVSample）**: `setWorkMode(m)` / `editSetMorph(name,w)` / `editClear()` /
  `saveCurrentFace()` / `loadFaceIntoEdit(id)` / `faceIdList()` / `loadFaceLib()` / `saveFaceLib()`。
  パネル id 規約: `mo:<Fcl名>`(スライダ) / `face_name`(名前) / `face_save` / `face_clear` /
  `lf:<id>`(読込) / `work_op` `work_face`(作業モード)。
- **TODO**: stage.tjs の合成表情から faces.json を参照（id 指定でロード）、詳細モード（全57モーフ）、
  ブラウザ側 three-vrm エディタ（同 JSON を読み書き）、②の次＝ポーズ簡易エディタ。

### motions（VRMA モーション）✅実装済
```tjs
%[ "id"=>"idle", "name"=>"待機", "file"=>"anim.vrma", "loop"=>1 ]
```
- `file`: `data/<file>`（拡張子込み）
- `loop`: 1=ループ / 0=一度再生して最終フレームを保持（=ポーズ付け）
- 再生: 対象キャラへ `playMotion`。開始/切替/停止でクロスフェード。「停止」で素立ちへ。
- ※ VRMA によっては hips 回転等で崩れる素材があるので動作確認済みのみ登録。

**poseseq（ポーズ列モーション）✅実装済**: VRMA でなく Pose を時間で繋いで合成する。
```tjs
%[ "id"=>"seqdemo", "name"=>"ポーズ列", "type"=>"poseseq", "loop"=>1,
   "keys"=>[ %[ "pose"=>"rest", "in"=>0.4, "hold"=>0.4 ],
             %[ "pose"=>"cap",  "in"=>0.5, "hold"=>0.7 ] ] ]
```
- `type`=="poseseq" で判定。`keys` は各キーフレーム:
  - `pose`: ポーズ id（poseSlots のキー。"rest"=素立ち。"cap"=採取スロット）
  - `in`: そのポーズへ寄せる遷移秒 / `hold`: 保持秒
- ランタイム(`Tachie.playPoseSeq`)が pose→(in秒 補間)→hold→次… を再生。loop 対応。
  → **ポーズ通しを繋いで自動でモーション化**。既存の Pose 補間器(applyPose/blendToRest)で駆動。

### poses（ポーズ）✅実装済
全身の静止姿勢＝正規化ヒューマノイドボーンの回転集合。データは
`[boneName, qx, qy, qz, qw, boneName, …]` のフラット配列（quaternion）。
```tjs
%[ "id"=>"peace", "name"=>"ピース", "data"=>[ "rightUpperArm", 0.04,-0.09,0.74,0.66, … ] ]
```
- **採取**: `Tachie.capturePose()` が現在の正規化姿勢を上記配列で返す（22ボーン）。
  → 現在姿勢/VRMA の1フレーム(playMotion 後)から採取できる。
- **適用**: `Tachie.applyPoseData(arr)`。現在姿勢からクロスフェードして保持（idle と競合しない）。
  「素立ち」で rest へクロスフェード。
- **保存/読込**: `(Array.saveStruct incontextof arr)(file)` / `Scripts.evalStorage(file)`。
- パネル「ポーズ」行: 採取 / 適用 / 素立ち / 保存 / 読込。メタ定義 `poses` があれば読込。

### scenes（名前付きシーン）✅実装済
宣言的に各キャラの状態＋演出＋カメラを一括指定する（TyranoScript タグ相当）。
```tjs
%[ "id"=>"talk", "name"=>"会話",
   "camera"=>%[ "frame"=>"bust" ],                 // 会話は胸上へ寄る
   "chars"=>%[ "left"  => %[ "expr"=>"happy", "motion"=>"idle", "gaze"=>"camera" ],
               "right" => %[ "expr"=>"relaxed", "motion"=>"left" ] ] ]
// イベント例: 空間モードへ切替＋望遠＋左右からスライド登場
%[ "id"=>"event", "name"=>"イベント",
   "camera"=>%[ "mode"=>"space", "reset"=>1, "fov"=>26 ],
   "chars"=>%[ "left" =>%[ "expr"=>"grin", "gaze"=>"camera", "fx"=>"slidein" ],
               "right"=>%[ "expr"=>"shock", "fx"=>"slidein" ] ] ]
```
- `chars` のキーは characters の `id`。各キャラ状態のキー:
  - `expr`: 表情名（VRM単一 / 合成 id / 生モーフ id / "neutral"）
  - `motion`: モーション id（"none"=停止）
  - `gaze`: "camera"（カメラ目線ON） / "off"
  - `x`,`z`: 位置（左右 / 前後）※**空間モードでのみ適用**
  - `fx`: 登場演出（"fadein" / "fadeout" / "slidein"）※dest レイヤのトゥイーン
  - 省略した項目は変更しない
- `camera`（シーンのカメラ指定。省略項目は変更しない）:
  - `mode`: "tachie" / "space"（モード切替）
  - `reset`: 1 でカメラを既定位置へ
  - `frame`: 立ち絵モードの構図プリセット（full/knee/waist/bust/face）を全キャラへ
  - `fov`: 空間モードの画角
- 適用: `applyScene`（chars → camera の順）。現在状態は `captureState`→`saveStruct` でファイル
  保存、`Scripts.evalStorage`→`applyState` で復元（再起動を跨ぐ。camera/fx は宣言専用で capture 対象外）。

---

## 実装状況

- ✅ characters / expressions（合成）/ motions（VRMA・クロスフェード）/ scenes（適用・保存/復元）
- ✅ **Pose**（採取／適用（クロスフェード）／保存・読込。データ = ボーン quaternion のフラット配列）
- ✅ **poseseq モーション**（Pose 列＋時間 → 補間再生でモーション合成、loop 対応）

用語 Expression/Pose/Motion/Scene の4層＋補間器が一通り実装済み。

### リップシンク（発話音声 → 口形）✅実装済
本体(krkrz) の `WaveSoundBuffer` 解析 API を VRM の visème 表情(aa/ih/ou/ee/oh)へ接続。
発話音声の再生カーソル付近を毎フレーム解析して口を駆動する（要 `useVisBuffer`）。
- 本体 API（要 `sound.useVisBuffer = true`。**`open()` の前に設定**。未設定だと空値）:
  - `getSoundLevel(ahead=0, window=0)` → `%[ rms, peak ]`（0..1）
  - `getSoundSpectrum(numbands, ahead=0)` → 対数バンド配列
  - `getVowel(ahead=0)` → `%[ a,i,u,e,o, voiced ]`（日本語5母音の重み・合計~1.0）
- 組み込み口: `Tachie.startLipSync(sound)` / `stopLipSync()`。ADV のボイス再生箇所で、
  再生する `WaveSoundBuffer`（同時発話人数分を共有する構成でも、そのキャラの1本）を渡す。
- 駆動: 毎フレーム `updateLipSync` が rms(開口量)×母音分布(口形)で visème を設定・平滑化。
  無音・停止時は visème に触れず表情へ委譲。
- 調整ノブ(Tachie): `lipGain`(開口ゲイン 既定3.0) / `lipMax`(開口上限 0.85) /
  `lipVowelMix`(口形ブレンド 0=音量のみ/aa・安定 ↔ 1=母音フル・表情豊か 既定0.4) /
  `lipAhead`(出力遅延の先読み補正 サンプル数) / 追従速度(`dt*16`)。
  ※ リアルタイム母音推定は暴れやすいので、既定は aa 開閉主体＋母音を少し混ぜる安定寄り。

## 関連 API（advsample / tachie）
- 表情: `Tachie.setEmotionSet(id, [name,w,…])` / `setEmotion(name)`
- モーション: `Tachie.playMotion(anim, loop, id)` / `stopMotion()`（素立ちへクロスフェード）
- 配置: `Tachie.spaceSet(x, z)`（空間モード, rig ごと）
- 演出: `Tachie.fadeIn/fadeOut/slideIn`（dest レイヤのトゥイーン, 立ち絵モード）
- シーン: `ADVSample.applyScene` / `captureState` / `saveSlot` / `loadSlot`
- ポーズ: `Tachie.capturePose()` / `applyPoseData(arr)` / `clearPose()`
- リップシンク: `Tachie.startLipSync(sound)` / `stopLipSync()`（本体 `WaveSoundBuffer` の
  `getSoundLevel`/`getVowel` を visème へ。`useVisBuffer` は open 前に有効化）
- C++ 追加: `VRM.getExpressionName(i)` / `setOffset` / `applyAnimation(anim,time,blend)` /
  `captureRestPose` / `blendToRest` / `applyPose([name,qx,qy,qz,qw,…], blend)`
