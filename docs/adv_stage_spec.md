# ADV サンプル / 舞台メタ定義 仕様 (stage.tjs)

`data/advsample.tjs`（`-sample` 起動）の宣言層。`data/stage.tjs` の `global.stageDef`
（TJS 辞書、JSON プラグイン非依存）でキャラ・表情・モーション・シーンを宣言する。

## 用語体系（4層）

| 層 | 定義 | 実体 |
|---|---|---|
| **Expression（表情）** | 顔。名前付き or 合成 | VRM表情名 / `{表情名:weight,…}` の束 |
| **Pose（ポーズ）** | 全身の**静止姿勢** | 正規化ボーンの回転集合（データ/採取/VRMA1フレーム）※実装予定 |
| **Motion（モーション）** | **時間つき**の全身動き | VRMA(取込) / **Poseの列＋時間**(合成)※poseseq は実装予定 |
| **Scene（シーン）** | 宣言的な複数キャラ状態 | expr + pose/motion + gaze + 位置 |

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

### motions（VRMA モーション）✅実装済
```tjs
%[ "id"=>"idle", "name"=>"待機", "file"=>"anim.vrma", "loop"=>1 ]
```
- `file`: `data/<file>`（拡張子込み）
- `loop`: 1=ループ / 0=一度再生して最終フレームを保持（=ポーズ付け）
- 再生: 対象キャラへ `playMotion`。開始/切替/停止でクロスフェード。「停止」で素立ちへ。
- ※ VRMA によっては hips 回転等で崩れる素材があるので動作確認済みのみ登録。

### scenes（名前付きシーン）✅実装済
宣言的に各キャラの状態を一括指定する（TyranoScript タグ相当）。
```tjs
%[ "id"=>"talk", "name"=>"会話",
   "chars"=>%[ "left"  => %[ "expr"=>"happy", "motion"=>"idle", "gaze"=>"camera" ],
               "right" => %[ "expr"=>"relaxed", "motion"=>"left" ] ] ]
```
- `chars` のキーは characters の `id`。各キャラ状態のキー:
  - `expr`: 表情名（VRM単一 / 合成 id / "neutral"）
  - `motion`: モーション id（"none"=停止）
  - `gaze`: "camera"（カメラ目線ON） / "off"
  - `x`,`z`: 位置（左右 / 前後）※**空間モードでのみ適用**
  - 省略した項目は変更しない
- 適用: `applyScene`。現在状態は `captureState`→`saveStruct` でファイル保存、
  `Scripts.evalStorage`→`applyState` で復元（再起動を跨ぐ）。

---

## 実装状況

- ✅ characters / expressions（合成）/ motions（VRMA・クロスフェード）/ scenes（適用・保存/復元）
- 🔜 **Pose**（正規化ボーン回転集合の一次データ。データ定義／現在姿勢の採取・保存）
- 🔜 **poseseq モーション**（Pose 列＋時間 → 補間再生でモーション合成）

## 関連 API（advsample / tachie）
- 表情: `Tachie.setEmotionSet(id, [name,w,…])` / `setEmotion(name)`
- モーション: `Tachie.playMotion(anim, loop, id)` / `stopMotion()`（素立ちへクロスフェード）
- 配置: `Tachie.spaceSet(x, z)`（空間モード, rig ごと）
- 演出: `Tachie.fadeIn/fadeOut/slideIn`（dest レイヤのトゥイーン, 立ち絵モード）
- シーン: `ADVSample.applyScene` / `captureState` / `saveSlot` / `loadSlot`
- C++ 追加: `VRM.getExpressionName(i)` / `setOffset` / `applyAnimation(anim,time,blend)` /
  `captureRestPose` / `blendToRest`
