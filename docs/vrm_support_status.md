# VRM 対応状況

krkrthreepp の VRM 実装が three-vrm / VRM 仕様のどこまで対応しているかの詳細一覧。
凡例: ✅ 対応 / ⚠️ 部分対応・近似 / ❌ 未対応。

- 実装: `vrm/VRM.cpp` `vrm/VRM.hpp`、TJS バインド: `main.cpp`、擬似コード API: `manual.tjs`
- 参照実装: three-vrm (`D:\test\three-vrm`)、VRM 仕様サンプル (vrm-c/vrm-specification)
- 検証: 実機 (krkrz64 + REPL) で確認済みのものは「実機✓」と記載

---

## 総括

VRM 0.x / 1.0 の**読込・humanoid・表情・視線・アニメ(VRMA)・揺れ物・制約・MToon**が
一通り動作する。同一モデルの 0.x / 1.0 両版がほぼ同一に描画されることを確認済み。
未対応の主なものは FirstPerson / VRMUtils / materialColorBind / scene ライティング。

---

## 読み込み (Loading)

| 項目 | 状況 | 備考 |
|---|---|---|
| VRM 0.x (`VRM` 拡張) | ✅ | 180°Y 回転で +Z 正面へ正規化 |
| VRM 1.0 (`VRMC_vrm`) | ✅ | |
| GLB / glTF | ✅ | マジックバイト "glTF" 判定(.vrm 拡張子対応, threepp fork) |
| メモリ/バイト列ロード | ✅ | `loadFromMemory` |
| 吉里吉里ストレージ/アーカイブ | ✅ | `loadStorage` (TVPCreateStream→memory) |
| sparse アクセサ (morph) | ✅ | threepp fork。表情 morph が sparse を使うため必須 |
| 全テクスチャ eager ロード | ✅ | 未参照テクスチャ(matcap 等)も解決(threepp fork) |

## Humanoid

| 項目 | 状況 | 備考 |
|---|---|---|
| raw ボーン名前引き | ✅ | `getBoneNode` |
| 正規化リグ + リタゲ | ✅ | rest world 回転を identity 化した正規化スケルトン |
| hips ルートモーション | ✅ | 正規化 hips の位置も raw へ転写 |
| 指ボーン | ⚠️ | rig 対象外(raw rest のまま。アニメの指は未転写) |
| pose get/set | ⚠️ | 最小(正規化ノードへの直接アクセスのみ) |

## 表情 (Expression / BlendShape)

| 項目 | 状況 | 備考 |
|---|---|---|
| morph target bind | ✅ | preset + custom。**MeshToonMaterial に morph mixin 追加で描画反映**(threepp fork) |
| isBinary | ✅ | 0.5 閾値 |
| override (block/blend) | ✅ | blink/mouth/lookAt グループ抑制。実機✓(block) |
| isBinary × override 相互作用 | ✅ | override 側=binary 値使用、被 override かつ isBinary=effect>0 で完全抑制(spec 準拠) |
| textureTransformBind | ✅ | 材テクスチャ UV offset/scale を weight で駆動(瞳移動)。実機✓(seed) |
| materialColorBind | ❌ | 検証アセット無し(手元 VRM は全て morph 表情)。VroidStudio 標準出力では付かない |

## 視線 (LookAt)

| 項目 | 状況 | 備考 |
|---|---|---|
| bone 方式 | ✅ | leftEye/rightEye を回転 |
| expression 方式 | ✅ | lookLeft/Right/Up/Down 表情を rangeMap で駆動→textureTransformBind で瞳移動。実機✓(seed) |
| rangeMap (Horizontal/Vertical) | ✅ | Outer レンジのみ(Inner は Outer で近似) |
| lookAt applier の world/head 空間 | ✅ | head ローカル空間で yaw/pitch 算出 |

## アニメーション (VRMA / VRMC_vrm_animation)

| 項目 | 状況 | 備考 |
|---|---|---|
| humanoid ボーン回転トラック | ✅ | nlerp 補間 |
| hips translation (ルートモーション) | ✅ | 高さスケール補正 + offset 方式。実機✓ |
| expression トラック | ⚠️ | パース・適用実装済。**顔トラック付き VRMA が手元に無く未検証** |
| lookAt トラック | ❌ | 未実装(rotation → 視線) |

## 揺れ物 (SpringBone)

| 項目 | 状況 | 備考 |
|---|---|---|
| 1.0 VRMC_springBone | ✅ | 慣性+剛性+重力→長さ拘束→衝突→回転(three-vrm 準拠) |
| 0.x secondaryAnimation | ✅ | boneGroups から joint チェーン再帰構築。実機✓ |
| 球 / カプセルコライダー | ✅ | |
| colliderGroups | ✅ | |
| center 空間 | ✅ | tail を center ローカルで保持、慣性を center 空間で計算(three-vrm 準拠) |
| コライダー半径スケール | ✅ | opt-in・標準外(`setColliderScale`)。VRoid の胴体側面手薄対策 |
| VRM0.x のメッシュ衝突 | ❌ | VRM 仕様に無し(コライダーは球/カプセルのみ) |

## 制約 (VRMC_node_constraint)

| 項目 | 状況 | 備考 |
|---|---|---|
| roll / aim / rotation | ✅ | three-vrm 式に準拠。依存順 topo ソート。実機✓(ツイストボーン) |

## MToon (VRMC_materials_mtoon)

独自の lights 対応 ShaderMaterial で実装(MeshToonMaterial 近似は廃止)。

| 項目 | 状況 | 備考 |
|---|---|---|
| lit / shade トゥーン陰影 | ✅ | shadingShift/shadingToony の linearstep ランプ(three-vrm 準拠) |
| 0.x _ShadeShift/_ShadeToony 変換 | ✅ | three-vrm VRMMaterialsV0CompatPlugin と同一式 |
| shadeMultiplyTexture | ✅ | baseColorTexture 共有(VRM 一般ケース) |
| normalMap | ✅ | tangent 無し向けに画面微分で接空間構築(perturbNormal) |
| emissive / emissiveMap | ✅ | |
| parametric rim | ✅ | フレネルリム(シェーダ統合) |
| matcap (_SphereAdd / matcapTexture) | ✅ | viewDir 相対の球 UV(three-vrm 準拠)。実機✓ |
| 輪郭線 outline | ✅ | 反転ハル。同一メッシュ 2 マテリアル+geometry group、オブジェクト空間オフセット、alpha discard。実機✓ |
| UV アニメーション (scroll/rotation) | ✅ | 回転→scroll 順(three-vrm 準拠)。実機✓(uvtest) |
| cutout / transparent (α) | ✅ | 0.x は `_BlendMode`、1.0 は glTF alphaMode。不透明材は α=1 固定 |
| rimMultiplyTexture | ❌ | 検証モデル未使用 |
| shadingShiftTexture | ❌ | 同上 |
| rimLightingMix / outlineLightingMix | ❌ | 多くのモデルで 0。未対応 |
| **シーンライティング** | ⚠️ | **固定カメラ相対キーライト**(scene の Directional/Ambient 非使用)。three-vrm 厳密一致には強度/π/色空間の整合が必要=高リスク。キャラビューアとしては妥当 |

## メタ情報 (Meta)

| 項目 | 状況 | 備考 |
|---|---|---|
| name / version / authors / licenseUrl | ⚠️ | 主要フィールドのみ(`getMetaName` 等) |
| thumbnail | ❌ | |
| ライセンス全項目 | ❌ | |

## その他

| 項目 | 状況 | 備考 |
|---|---|---|
| FirstPerson (一人称/三人称メッシュ注釈) | ❌ | 三人称ビューアでは頭を隠す程度で優先度低 |
| VRMUtils (dispose / skeleton 結合 等) | ❌ | 非視覚ユーティリティ |

## 描画基盤 (three-vrm には不要な吉里吉里/GLES 連携)

| 項目 | 状況 | 備考 |
|---|---|---|
| GLES(ANGLE) 上で threepp 描画 | ✅ | glad を GLES 化、捕捉 FBO へ描画、シェーダ 300 es |
| MSAA (縁 AA) | ✅ | `setSampleCount`。内部マルチサンプル FBO→resolve |
| premultiplied 縁補正 | ✅ | gles.dll `unpremultiply`(opt-in) |
| 吉里吉里レイヤ合成 | ✅ | dest.type=ltAlpha |

---

## 既知の非対応・今後の候補 (優先度順の目安)

1. **VRMA 顔トラック** — expression トラック検証(顔付き VRMA 入手待ち) + lookAt トラック実装
2. **materialColorBind** — 実装は three-vrm `VRMExpressionMaterialColorBind` 参照で可能。検証アセット待ち
3. **シーンライティング** — 固定ライト→scene ライト。高リスク大工事
4. **MToon 細部** — rimMultiplyTexture / shadingShiftTexture / rimLightingMix / outlineLightingMix
5. **FirstPerson / VRMUtils** — 低優先

## 関連ドキュメント

- 揺れ物コライダーと「髪が服に埋まる」現象: [`vrm_springbone_collider_notes.md`](vrm_springbone_collider_notes.md)
- 初期の実現性調査・Phase0 設計(歴史的資料): [`history/`](history/)
