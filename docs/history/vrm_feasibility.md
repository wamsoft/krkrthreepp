> **【歴史的資料】** 2026-08-01 に VRM 対応を始める前の実現性調査。実装が進み内容は
> 現状と乖離している。**現在の対応状況は [`../vrm_support_status.md`](../vrm_support_status.md) を参照**。
> 本ファイルは着手時の判断記録として残す。

# VRM on threepp 実現性調査レポート

**調査日:** 2026-08-01
**目的:** three-vrm（three.js 用 VRM ライブラリ, v3.5.1）相当のシステムを、threepp（three.js の C++ 移植, r129 ベース）上に **C++ で構築**し、吉里吉里 / C++ ゲームと統合できるか。

**背景:** 既存の JS 版エンジン `d:/test/jsengine` が ThreeJS(r176) + ThreeVRM(v3.5.1) を OpenGLES 上で動作させ済み。それをそのまま持ち込む案もあるが、C++ ベースなら実行性能と C++ ゲームとの親和性で有利になるため、本レポートで移植可否と計画を精査した。

---

## 結論

**実現可能。しかも当初想定よりリスクが低い。**

懸念だった2大障壁（① glTF ローダから VRM 拡張・オブジェクト対応へアクセスできるか、② MToon トゥーンシェーダを threepp で実装できるか）は、いずれも threepp 側で対応可能と判明。さらに **流用できる既存 C++ 資産（infosia/VRM.h）** が見つかり、ローダ工数を大きく削減できる見込み。

### 実現性を裏付ける5つの発見

| # | 発見 | 意味 |
|---|---|---|
| 1 | threepp の GLTFLoader は**生 glTF JSON も index→オブジェクト対応表も内部に完備**（`nodeObjects` / `materialCache` / `skinCache` / `textureCache`）。露出だけしていない | VRM の2大要件（生 extensions・associations）は「新規実装」でなく「露出」で足りる |
| 2 | threepp に three.js の **`#include` シェーダチャンク機構＋toon チャンク＋`meshtoon` シェーダ**が移植済み。SkinnedMesh + ShaderMaterial で skinning / morph が自動注入される | **MToon が現実的**（最大の懸念が大幅に低減） |
| 3 | 7サブシステム全てが threepp 既存機能で実装可能、**本体の破壊的改変は不要** | 追加は「利用側の派生・別モジュール」で収まる |
| 4 | **infosia/VRM.h**（MIT・**nlohmann 依存＝threepp と同一**・ヘッダオンリー）が VRM 0.x / 1.0 の全拡張を構造体化済み | VRM 拡張パース層を**丸ごと流用可能** |
| 5 | 目標 API が明確（`VRMActor` 高レベル＋three-vrm 相当ランタイム低レベル） | 「何を作ればゴールか」が確定済み |

---

## 1. 対象バージョンと目標 API

- **参照実装:** three.js r176 / **@pixiv/three-vrm v3.5.1**（core / materials-mtoon / springbone / node-constraint / animation / materials-v0compat）。MIT。
- **threepp:** three.js **r129** ベースの C++ 移植（fork: `wtnbgo/threepp` @ tag 2026-06-17）。
- **VRM 対応方針:** まず **VRM 1.0 のみ**先行（拡張が `VRMC_*` に分離・明文化されていて明快）。VRM 0.x は後半で軸反転・暗黙 tail 補正を追加（infosia/VRM.h が両対応なので追加は限定的）。

### 目標 API サーフェス（jsengine の vrmkit を雛形に）

**低レベル（three-vrm 相当ランタイム）**
- ローダ（VRM0/1 の glTF 解析 + `VRMUtils.rotateVRM0` / `removeUnnecessaryVertices` / `combineSkeletons` 相当）
- `vrm.humanoid`（`getNormalizedBoneNode` / `getRawBoneNode` / `resetNormalizedPose` / `update`、normalized↔raw リグ）
- `vrm.expressionManager`（`expressionMap` / `getValue` / `setValue`、preset 名、override 抑制）
- `vrm.lookAt`（`target` セット→毎フレーム追従、bone/expression 方式）
- `vrm.springBoneManager`（`reset()` + 毎フレーム物理）
- `vrm.update(delta)`（上記を束ねる総合更新）
- MToon マテリアル + VRM0 マテリアル互換
- VRMA（`VRMC_vrm_animation` 1.0 読み込み + リターゲット、AnimationMixer 相当）
- `firstPerson` は **jsengine 未使用のため優先度低**

**高レベル（`VRMActor` 相当・ゲーム側公開 API）**
`update(dtSec)` 一本で「procedural/VRMA ロコモーション → mixer → 表情フェード＋自動まばたき＋口パク → vrm.update」を回す薄いラッパ。位置/向き（`setPosition` / `faceTowards` / `teleportTo` / `settle`）、モーション（`addClip` / `playEmote` / `playLoop` / `setLocomotionClips`）、表情（`setExpression` / `setEmotion` / `setMouthOpen`）、視線（`lookAtTarget`）、ボーン参照（`bone` / `boneWorldPosition`）。

---

## 2. 土台の方式判断：**案A（threepp GLTFLoader を拡張）で確定**

### threepp GLTFLoader の実態
- `nlohmann/json` で生 glTF を**自前パース**（cgltf/tinygltf 不使用）。
- 生 JSON をパーサ内部に丸ごと保持し、`gltf["extensions"][...]` で**任意拡張へ既に生アクセス**（KHR_materials_variants / KHR_texture_transform / KHR_lights_punctual 等）。VRMC_* も同じ位置に存在。
- **index → 生成オブジェクトの対応表が既に内部にある:** `nodeObjects`（node→Object3D/Bone）/ `materialCache` / `skinCache` / `textureCache`。
- スキニング（IBM/joints/weights）、モーフ影響度、アニメーション sampler も処理済み。
- **唯一の障壁:** パーサ `GLTFParser` が匿名 namespace 内のローカル変数で、`GLTFResult{ scene, scenes, animations, variants }` を返した瞬間に破棄される。生 JSON も対応表も外に出ていない。

### 方式比較

| 観点 | 案A: threepp GLTFLoader を拡張し露出 | 案B: 別パーサ(cgltf/tinygltf/fastgltf)採用＋変換層新規実装 |
|---|---|---|
| 拡張アクセス | ◎ 生 `nlohmann::json` が既に内部にあり露出するだけ | ○ tinygltf/cgltf は容易、fastgltf は要コールバック |
| index↔threepp対応 | ◎ **既存**（nodeObjects 等）を出すだけ | ✕ パーサは自前構造体のみ。**対応表は結局自作** |
| 二重管理 | ◎ パース1回・シーングラフ1つ | ✕ **glTF を2回パース** or ローダ全面置換 |
| 実装量 | 小（露出＋VRM 拡張解釈層。拡張構造体化は VRM.h 流用可） | 大（accessor→BufferGeometry、skin→Skeleton 等を全再実装） |
| リスク | 内部実装依存（upstream 追従時に露出 API 保守） | 既存ローダと挙動差・機能欠落 |

**→ 案A が明確に優位。** 別パーサは必ず二重パース＋対応表自作になり、案Aの作業を包含した上で変換層まで再発明することになる。

### 露出改造の具体方向（中規模）
1. `GLTFParser` を破棄せず存続（Impl を hpp に前方宣言し pImpl 保持、または対応表を `GLTFResult` へ move/複製）。
2. `GLTFResult` に「生 glTF `nlohmann::json`」と「associations（node/mesh/material/skin/texture の index→threepp オブジェクト map）」を追加（three.js の `parser.associations` / `getDependency` 相当）。
3. mesh/primitive→Mesh 対応表を1本追加（現状 `userData["__gltfMeshIdx"]` タグ頼みで、variants 時に erase される欠点を解消）。
4. 追加で望ましい小物：morph target 名（`mesh.extras.targetNames`）の読取（Expression 名前引き用、現状欠落）、texture filter 適用（軽微）。

---

## 3. サブシステム別の実装可否（threepp 精査）

いずれも threepp 既存機能で実装可能。本体への破壊的改変は不要。

| サブシステム | 判定 | 根拠・要点 |
|---|---|---|
| **Humanoid** | 実装可能 | Bone/Skeleton/SkinnedMesh 完備。Object3D の階層・ワールド行列・quaternion（`applyQuaternion`/`worldToLocal`/`updateWorldMatrix` 等）完備。normalized/raw リグ生成に必要な行列操作が過不足なし |
| **Expression** | 実装可能 | `ObjectWithMorphTargetInfluences::morphTargetInfluences()` で実行時操作可。GLMorphTargets が毎描画反映。material 色/opacity/emissive も動的変更可。**同時 8 ターゲット上限**（r129 同）だが表情合成では通常問題なし。UV オフセット動的変更は MToon の自前 uniform で対応 |
| **LookAt** | 実装可能 | Quaternion/Euler/Matrix4/Vector3 が three.js 同等に完備（`setFromUnitVectors`/`rotateTowards`/`Matrix4::lookAt` 等） |
| **SpringBone** | 実装可能 | 数学＋world↔local 変換完備。衝突用 `math/Sphere.hpp` / `math/Capsule.hpp` あり。毎フレーム駆動は本体ループで `mixer.update → spring.update(dt) → render` の順に明示呼び出しが確実（`onBeforeRender` フックは補助） |
| **MToon** | 実装可能（一部軽微な拡張） | `ShaderMaterial`＋`#include` チャンク機構＋skinning/morph 自動注入。**要点は下記** |
| **FirstPerson** | 実装可能（優先度低） | `core/Layers.hpp`（マスク）で一人称/三人称カメラの可視分け。jsengine 未使用 |
| **アニメーション(VRMA)** | 実装可能 | AnimationMixer/Clip/各 KeyframeTrack（Linear/Cubic/Discrete、`makeAdditive`）。PropertyBinding が quaternion/position/scale/morphTargetInfluences[i]/material 値を全対応。`findNode` が `skeleton->getBoneByName` でボーン名解決 |

### MToon の実装ポイント（最重量ポイント・重点評価）
- **`ShaderMaterial`（RawShaderMaterial ではなく）** を使えばスキニング＋モーフ＋独自 GLSL を持つトゥーンマテリアルを追加できる。
- **利用可能な機構:**
  - `#include <chunk>` を解決する `GLProgram::resolveIncludes()`、`ShaderChunk` に skinning/morph/toon（`lights_toon_*`, `gradientmap_pars_fragment`）系が全て存在。`ShaderLib` に `meshtoon_vert/frag` の完成品あり（ベースに流用可）。
  - `ProgramParameters` が `SkinnedMesh` で `USE_SKINNING` を自動注入、ボーン数 64 上限は `BONE_TEXTURE` 経路（vertex texture）で実質回避。
  - uniform に `Texture*` を入れると `GLUniforms` が自動バインド（matcap/リムマスク等のカスタムテクスチャ自由）。UV アニメは uniform 更新＋`uniformsNeedUpdate=true`。
- **必要な小拡張（本体改変不要）:** MToon を `class MToonMaterial : public ShaderMaterial, public MaterialWithMorphTargets`（両者 `virtual Material`）として定義 → `USE_MORPHTARGETS` 経路が有効化。
- **アウトライン（背面法）:** geometry/skeleton 共有の第2 `SkinnedMesh`＋`side=Side::Back`＋`renderOrder` で構成可能。
- **唯一の欠落 `onBeforeCompile` は未実装**（コメントアウト）だが、MToon は独立した完全 ShaderMaterial 方式（three-vrm 自身も専用シェーダ）なので **不要**。
- **残る不確定:** threepp は r129 ベースなので、MToon が前提とする新しめ three.js のチャンクとの差分吸収の実量は Phase 2 で実際に通すまで正確には不明。

---

## 4. 参考にできる既存 C++ 資産

| プロジェクト | 概要 | 基盤 | License | 有用性 |
|---|---|---|---|---|
| **infosia/VRM.h** | C++ ヘッダオンリーの VRM シリアライズ/デシリアライズ。VRM 0.x/1.0 両対応。VRMC_vrm / springBone / node_constraint / materials_mtoon / extended_collider / vrm_animation を構造体化。**シーングラフは作らずデータ化のみ**。**nlohmann::json 依存（＝threepp と同一）** | fx-gltf/cgltf 系 + nlohmann | **MIT** | ◎ 拡張ブロック→構造体化を丸ごと再利用でき、threepp が露出する生 JSON をそのまま渡せる最有力ピース |
| **ruyo/VRM4U** | Unreal Engine 5 ランタイム VRM ローダ。MToon/SpringBone/Humanoid/ライセンスまで実装 | assimp fork + RapidJSON | MIT | ○ MToon/springbone/humanoid の**解釈の参考**。UE 密結合で直接流用不可 |
| **vrm-c/UniVRM** | VRM 標準実装。C#（Unity） | — | MIT | 仕様/挙動の**一次リファレンス**（コードは移植参考のみ） |

> glTF パーサ単体の代替を使う場合は **tinygltf**（threepp と同じ nlohmann、拡張を `Value` で直接保持、単一ヘッダ）が最も摩擦が少ない。assimp・yocto-gl は「独自シーングラフ構築＋VRM 拡張を取り出せない」ため本用途では不適。

---

## 5. 段階的移植計画

各フェーズで TJS バインドまで提供する前提。依存順は「index 解決基盤 → humanoid → expression/lookAt → springbone/mtoon」。MToon と SpringBone は他から依存されず後回し可能（Expression の material bind を除く）。

### Phase 0 — 最小表示（土台）　［難易度：低〜中］
1. **threepp GLTFLoader 露出改造**（§2 の1〜4）。
2. **infosia/VRM.h 組込み** — 露出した生 JSON を渡して `VRMC_vrm` 等を構造体化。
3. VRM を通常 glTF として読み、**MToon は標準 Lambert/PBR で代替**（lit color texture だけ拾えば見た目は成立）。
- **到達点:** VRM が静止ポーズで正しく立ち、スキンメッシュが動く。

### Phase 1 — 動かす基礎　［中］
4. **VRMHumanoid**（ボーン名マッピング＋normalized/raw 二重リグ）。
5. **VRMExpression**（morph bind のみ＝表情）。
6. **VRMLookAt**（視線、bone/expression 方式）。
7. **VRMA** — VRM.h でパース → `createVRMAnimationClip` 相当でリターゲット → threepp AnimationMixer で再生（clipAction / fadeIn/Out / LoopOnce/Repeat / clampWhenFinished / timeScale / finished）。
- **到達点:** アニメ＋表情＋視線が付いた実用アバター。

### Phase 2 — 揺れもの＆トゥーン　［高・最大の山］
8. **VRMSpringBone**（sphere/capsule collider、center 空間。毎フレーム `mixer.update → spring.update → render` 順で明示駆動）。
9. **MToon**（`ShaderMaterial`＋`MaterialWithMorphTargets` 多重継承、背面法アウトライン、rim/matcap/UV アニメは独自 uniform）。
10. Expression の **material color / texture transform bind**（MToon 完成後に有効化）。

### Phase 3 — 仕上げ・互換　［低〜中］
11. **VRMNodeConstraint**（roll/aim/rotation、クォータニオン演算で自己完結）。
12. **VRMFirstPerson**（`Layers` で可視制御。優先度低）。
13. **VRMMeta**（ライセンス表示）。
14. **VRM 0.x 読み込み**（X 軸反転・暗黙 tail 補正。VRM.h 両対応で追加限定的）。

### 高レベル層（全フェーズ横断）
`VRMActor` 相当（`update(dt)` 一本で歩行・モーション・表情・まばたき・口パク・視線）をゲーム側公開 API として被せる。目標は `d:/test/jsengine/samples/vrm_starter/vrmkit` の `VRMActor`。

---

## 6. 未確定リスク（着手前・各フェーズ頭で潰す）

1. **infosia/VRM.h の実カバレッジ** — 報告ベース。実際に組み込んで必要拡張を過不足なく構造体化するか要検証（Phase 0）。
2. **MToon の r129 チャンク差分の実量** — Phase 2 で shader を実際に通すまで正確な工数は不明。
3. **描画自体の目視確認は GLES 接続の実行時検証に依存** — 別件の 64bit GLES 課題（`../gles`）が前提。VRM 実装（ロジック）自体は並行して進められる。

---

## 出典

- pixiv/three-vrm — https://github.com/pixiv/three-vrm （DeepWiki: https://deepwiki.com/pixiv/three-vrm ）
- VRMC_vrm-1.0 / VRMC_springBone-1.0 spec — https://github.com/vrm-c/vrm-specification
- MToon shaders — https://github.com/pixiv/three-vrm/tree/dev/packages/three-vrm-materials-mtoon/src/shaders
- infosia/VRM.h — https://github.com/infosia/VRM.h
- ruyo/VRM4U — https://github.com/ruyo/VRM4U
- vrm-c/UniVRM — https://github.com/vrm-c/UniVRM
- glTF パーサ: tinygltf https://github.com/syoyo/tinygltf / cgltf https://github.com/jkuhlmann/cgltf / fastgltf https://github.com/spnda/fastgltf
- ローカル精査: `threepp/src/threepp/loaders/GLTFLoader.cpp`, `threepp/src/threepp/renderers/{gl,shaders}/`, `d:/test/jsengine/samples/vrm_starter/vrmkit/`
