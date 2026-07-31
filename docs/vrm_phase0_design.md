# VRM Phase 0 詳細設計 — 最小表示（土台）

> **状況 (2026-08-01): 実装完了・ロード検証 PASS（VRM 0.x / 1.0 両方）。**
> スタンドアロン検証 `test/vrm_smoke.cpp` で:
> - `AvatarSample_A/B.vrm`（VRM 0.x）: humanoid 54 ボーン解決、9/9 主要ボーン、scene 199 ノード。
> - `VRM1_Constraint_Twist_Sample.vrm`（VRM 1.0, three-vrm 公式サンプル）: humanoid 54、
>   meta（name/version=v1.0.1/author=pixiv Inc./licenseUrl）を infosia/VRM.h で取得、scene 185 ノード。
>
> 実装差分:
> - **threepp fork**（work_sdl3）: GLTFLoader 露出改造（生JSON＋associations＋meshPrimitives）、
>   **GLB をマジックバイト "glTF" で判定**（`.vrm` 拡張子対応）、**sparse アクセサ対応**を
>   `readFloats` に追加（VRM モーフターゲット＝表情が sparse を使うため必須。これが無いと 1.0 は
>   morph accessor でクラッシュしていた）。
> - **plugin**: `vrm/` モジュール（VRMLoader/VRM/VRMHumanoid/VRMMeta、0.x/1.0 両対応）、
>   `main.cpp` の ncbind 登録、`external/VRM`(infosia/VRM.h submodule)、`test/vrm_smoke.cpp`。
> - 検証のため VRM 0.x の humanoid/meta 読み取りを前倒し実装（Phase3 予定だったが実サンプルが 0.x）。
>
> **未検証は描画（GLES 接続の実行時）のみ。ロードは 0.x/1.0 とも動作確認済み。**

**前提レポート:** [`vrm_feasibility.md`](./vrm_feasibility.md)
**このフェーズのゴール:** VRM(1.0) を glTF として読み込み、静止ポーズで正しく立ち、スキンメッシュが動く状態。MToon は標準マテリアルで代替。humanoid ボーンへ index 経由でアクセスできる土台を作る。

Phase 0 は **ローダ側のロジック**であり、描画の目視確認（GLES 接続）に依存せず実装・単体確認まで進められる。目視は 32bit + effekseer テスト構成、または 64bit GLES 解消後に行う。

---

## 全体構成

```
[threepp fork]  GLTFLoader を「拡張露出」改造（生JSON + associations）   ← 唯一の threepp 本体改変
        │  GLTFResult に生glTF JSON文字列と index→オブジェクト対応表を追加
        ▼
[plugin]  external/VRM.h(infosia, MIT, header-only, nlohmann依存) を vendor
        │
        ├─ vrm/VRMLoader  : GLTFLoader を回し、生JSONを VRM.h でパースして VRM を組む
        ├─ vrm/VRM        : scene(Group) + humanoid + meta を保持する実体
        ├─ vrm/VRMHumanoid: humanBoneName → threepp::Bone（Phase0 は raw のみ）
        └─ vrm/VRMMeta    : ライセンス等メタ
        ▼
[TJS]  var vrm = new Threepp.VRMLoader().load("model.vrm");
       scene.add(vrm.scene);  renderer.render(scene, camera);
```

---

## 1. threepp GLTFLoader 露出改造（案A）

唯一の threepp 本体（fork）改変。内部に既にあるデータを `GLTFResult` へ載せるだけ。**公開ヘッダに nlohmann を漏らさない**ため、生 glTF は `std::string`（JSON テキスト）で渡し、VRM レイヤ側で自前 nlohmann で再パースする（ロード時 1 回だけなのでコスト無視できる）。

### 1-1. `GLTFResult` 拡張（`include/threepp/loaders/GLTFLoader.hpp`）

```cpp
#include <map>
#include <unordered_map>
#include <string>

struct GLTFResult {
    std::shared_ptr<Group> scene;
    std::vector<std::shared_ptr<Group>> scenes;
    std::vector<std::shared_ptr<AnimationClip>> animations;
    MaterialVariants variants;

    // --- 拡張レイヤ(VRM 等)向けの露出。標準 glTF 利用には影響しない ---

    /// 生 glTF JSON テキスト（extensions / extras 含む）。
    /// nlohmann を公開ヘッダに出さないため文字列で渡す。空なら未設定。
    std::string json;

    /// glTF index → 生成された threepp オブジェクト（three.js parser.associations 相当）
    std::unordered_map<int, std::shared_ptr<Object3D>> nodes;      ///< node index
    std::unordered_map<int, std::shared_ptr<Material>>  materials; ///< material index
    std::unordered_map<int, std::shared_ptr<Texture>>   textures;  ///< texture index
    std::unordered_map<int, std::shared_ptr<Skeleton>>  skins;     ///< skin index
    std::map<std::pair<int,int>, std::shared_ptr<Mesh>> meshPrimitives; ///< (mesh,prim) index → Mesh
};
```

> `nodes` は VRM の humanoid / springBone joint / lookAt eye / constraint の node 参照解決に、`materials`/`textures` は Expression の material bind と MToon(Phase2) に、`skins` は骨格に、`meshPrimitives` は Expression の morph bind に使う。Phase 0 では `nodes` と `json` があれば足りる。

### 1-2. パーサ側の対応（`src/threepp/loaders/GLTFLoader.cpp`）

- `struct GLTFParser` に生 JSON テキストと mesh/prim 対応を追加:
  ```cpp
  std::string rawJsonText;                                            // parseGLTF/parseGLB で設定
  std::map<std::pair<int,int>, std::shared_ptr<Mesh>> meshPrimitives; // loadMesh で設定
  ```
- 既存の `loadMesh` で Mesh を生成している箇所（現状 `userData["__gltfMeshIdx"]`/`["__gltfPrimIdx"]` タグを付けている 899-900 行付近）に、`meshPrimitives[{meshIdx, primIdx}] = mesh;` を 1 行追加。
- `parseGLTF`（1227）/`parseGLB`（1270）で、パース元テキストを `rawJsonText` に退避（`parseGLTF` は引数 `jsonText` をそのまま、`parseGLB` は抽出した JSON チャンク文字列）。
- `resolveVariants(GLTFResult&)`（1551）と同様に **finalize ヘルパ**を追加し、両 parse の `return result;` 直前で呼ぶ:
  ```cpp
  void populateAssociations(GLTFResult& result) {
      result.json           = rawJsonText;
      result.nodes          = nodeObjects;   // shared_ptr のコピー（安価）
      result.materials      = materialCache;
      result.textures       = textureCache;
      result.skins          = skinCache;
      result.meshPrimitives = meshPrimitives;
  }
  ```
  注意: `materialCache` は遅延生成でメッシュ参照分のみ、`nodeObjects` の mesh ノードは collapse 後も index キーは残る（humanoid bone は `Bone` のまま残る）— VRM 用途に好適。variants による userData タグ erase の影響を受けない独立経路になる。

### 1-3. 破壊的影響

- 既存の `GLTFResult` 利用者は追加フィールドを無視するだけ。API 後方互換。
- パーサ破棄前にコピーするので「パーサ存続化(pImpl)」は不要 → 改変は局所的。
- **fork 差分が 1 つ増える**（本家 GLTFLoader への追従時に要保守）。コミットは threepp fork(work_sdl3) 側に載せる。

---

## 2. infosia/VRM.h 組み込み

- **形態:** ヘッダオンリー `<VRMC/VRM.h>`、`#define USE_VRMC_VRM_1_0`（Phase3 で `USE_VRMC_VRM_0_0` 追加）。**nlohmann::json 依存**、シーンは作らず拡張ブロックのみ構造体化。
- **使い方:**
  ```cpp
  #define USE_VRMC_VRM_1_0
  #include <VRMC/VRM.h>
  nlohmann::json gltf = nlohmann::json::parse(result.json);
  const auto& ext = gltf.at("extensions");
  VRMC_VRM_1_0::Vrm v;
  VRMC_VRM_1_0::from_json(ext.at("VRMC_vrm"), v);   // v.meta / v.humanoid / v.expressions / v.lookAt / v.firstPerson
  ```
- **vendoring:** `plugins/threepp/external/VRM/`（サブモジュール or ソース直置き, MIT）。ライセンス表記を README に追記。
- **nlohmann 供給:** プラグインにも必要（生 JSON 再パース＋VRM.h）。threepp が vendor 済みの `threepp/src/external/nlohmann` を include に足すか、vcpkg（`nlohmann-json`）で入れる。プラグインには現状 `vcpkg.json` が無いので、シンプルには vendor 済みパスを再利用。

---

## 3. VRM モジュール（プラグイン側・新規）

ファイル構成（siblings の `_common`/`_method` 分割は Phase が進んでから。Phase 0 は最小）:

```
plugins/threepp/
  vrm/
    VRM.hpp        VRM / VRMHumanoid / VRMMeta の宣言
    VRM.cpp        実装
    VRMLoader.hpp  VRMLoader の宣言
    VRMLoader.cpp  GLTFLoader → VRM.h → VRM 構築
    VRM_bind.cpp   ncbind 登録（または main.cpp に追記）
```

### 3-1. クラス設計（Phase 0 スコープ）

```cpp
namespace krkrvrm {

struct VRMMeta {                 // v.meta から詰める（表示・法的表示用）
    std::string name, version, authors, licenseUrl;
    // Phase0 は主要フィールドのみ。詳細は Phase3。
};

class VRMHumanoid {
public:
    // VRM humanBone 名（"hips","spine",...,"leftUpperArm",...）→ 実ボーン。
    // Phase0 は raw のみ（normalized/raw 二重リグは Phase1）。
    std::shared_ptr<threepp::Bone> getRawBoneNode(const std::string& humanBoneName) const;
    bool has(const std::string& humanBoneName) const;
private:
    std::unordered_map<std::string, std::shared_ptr<threepp::Bone>> rawBones_;
    friend class VRMLoader;
};

class VRM {
public:
    std::shared_ptr<threepp::Group> scene;   // GLTFResult.scene をそのまま
    VRMHumanoid humanoid;
    VRMMeta     meta;
    // Phase1+: expressionManager / lookAt / springBoneManager / update(dt)
};

class VRMLoader {
public:
    std::shared_ptr<VRM> load(const std::filesystem::path& path);   // 失敗時 nullptr
};

} // namespace krkrvrm
```

### 3-2. `VRMLoader::load` フロー

1. `threepp::GLTFLoader().load(path)` → `GLTFResult`（`json` + `nodes` 付き）。失敗なら nullptr。
2. `nlohmann::json gltf = nlohmann::json::parse(result->json);`
3. `extensions` に `VRMC_vrm` が無ければ「VRM ではない普通の glTF」として scene だけ返す（or nullptr）。
4. `VRMC_VRM_1_0::from_json(ext["VRMC_vrm"], v);`
5. **Humanoid 構築:** `v.humanoid.humanBones` の各 (boneName → node index) について
   `auto it = result->nodes.find(nodeIdx); auto bone = std::dynamic_pointer_cast<threepp::Bone>(it->second);`
   を `rawBones_[boneName] = bone;` に格納。
6. **Meta 構築:** `v.meta` を `VRMMeta` へ写す。
7. `vrm->scene = result->scene;` を返す。
8. （Phase3: `ext` に `VRM`(0.x) があれば `VRMC_VRM_0_0` 経路 + 軸/tail 補正。）

### 3-3. MToon → 標準マテリアル

**追加実装ほぼ不要**。VRM 1.0 のマテリアルは MToon 拡張と併せて標準 `pbrMetallicRoughness` を必ず持つ（非対応ビューア用フォールバック）。threepp の GLTFLoader はこの pbrMetallicRoughness から `MeshStandardMaterial` を生成するので、`VRMC_materials_mtoon` は無視されてそのまま標準表示になる。
→ **Phase 0 リスク項目:** 対象 VRM が本当に pbr フォールバックを持つか実ファイル（`AvatarSample_A.vrm`）で確認する。無い/真っ黒なら暫定で lit color texture を拾う簡易処理を足す。

---

## 4. TJS バインド（Phase 0）

ncbind で最小を公開:

```
class VRMLoader {
    function VRMLoader();
    function load(path);        // → VRM オブジェクト（失敗で void）
}
class VRM {
    property scene;             // Threepp.Group（scene.add() で Threepp.Scene に足す）
    property meta;              // 辞書（name/version/authors/licenseUrl）
    function getBoneNode(name); // Threepp.Object3D（Bone）— カメラフレーミング等に
}
```

TJS 使用例:
```javascript
var loader = new Threepp.VRMLoader();
var vrm = loader.load("models/AvatarSample_A.vrm");

var scene = new Threepp.Scene();
scene.add(vrm.scene);                     // VRM の Group を Scene に追加
var camera = new Threepp.PerspectiveCamera(30, w/h, 0.1, 20);
camera.position.set(0, 1.3, -2);          // VRM1 は +Z 正面
camera.lookAt(0, 1.3, 0);

// 描画（GLES 接続の renderer 上で）
renderer.onBeginScene(w, h);
renderer.render(scene, camera);
renderer.onEndScene();
```

> `vrm.scene` は Group なので `Threepp.Scene` に add して既存 `GLRenderer.render(scene, camera)` に流す（three.js と同じ流儀）。Group を返す property バインドは既存の Object3D ラッパ返却作法（`ThreeWrapper<Group>`）を流用。

---

## 5. ビルド構成変更

### 5-1. threepp fork（submodule）
- GLTFLoader.hpp/.cpp を §1 のとおり改修（work_sdl3 に新規コミット）。

### 5-2. プラグイン `CMakeLists.txt`
```cmake
krkrz_plugin(krkr_threepp
    NCBIND
    SOURCES
        main.cpp
        vrm/VRM.cpp
        vrm/VRMLoader.cpp
        vrm/VRM_bind.cpp        # or main.cpp に統合
    LIBRARIES
        threepp
)
# VRM.h と nlohmann の include
target_include_directories(krkr_threepp PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/external/VRM/include
    ${CMAKE_CURRENT_SOURCE_DIR}/threepp/src/external/nlohmann)  # threepp vendor を再利用
target_compile_definitions(krkr_threepp PRIVATE USE_VRMC_VRM_1_0)
```
（`krkrz_plugin` マクロがターゲット名で `target_*` を許すか要確認。不可ならマクロ引数側で include/define を渡す。）

---

## 6. 作業順序（実装チェックリスト）

1. [ ] threepp fork: `GLTFResult` 拡張フィールド追加（hpp）
2. [ ] threepp fork: `GLTFParser` に `rawJsonText`/`meshPrimitives` 追加、`loadMesh` に 1 行、`populateAssociations` 追加、両 parse で呼ぶ
3. [ ] threepp fork: threepp 単体でビルド確認（既存 glTF 読み込みが壊れないこと）
4. [ ] external/VRM 配置（infosia/VRM.h）、nlohmann include 疎通、`USE_VRMC_VRM_1_0`
5. [ ] `vrm/VRMLoader` 実装（GLTFLoader→json 再パース→VRM.h→Humanoid/Meta 構築）
6. [ ] `vrm/VRM` `VRMHumanoid` `VRMMeta` 実装
7. [ ] ncbind 登録（VRMLoader / VRM）
8. [ ] Release ビルド
9. [ ] `AvatarSample_A.vrm` を load して、humanoid ボーン数・meta・scene ノード数をログ確認（描画前の単体検証）
10. [ ] 描画確認（32bit+effekseer 構成 or 64bit GLES 解消後）

---

## 7. Phase 0 完了条件・リスク

**完了条件**
- `AvatarSample_A.vrm` を load でき、`vrm.humanoid.getBoneNode("hips")` 等が有効なボーンを返す。
- `scene.add(vrm.scene)` して標準マテリアルで静止ポーズ表示（描画は環境が整い次第）。

**リスク / 着手時に潰す**
1. **VRM.h の実カバレッジ**（humanBones の index の持ち方、meta フィールド名）— 実 API を実物で確認して 3-2 の詳細を確定。
2. **pbr フォールバックの有無** — 3-3 のとおり実ファイルで確認。無ければ簡易 lit color 処理を暫定追加。
3. **座標系** — VRM1 は +Z 正面・右手系・メートル。カメラ配置で確認。VRM0.x は Phase3。
4. **threepp GLTFLoader の既存挙動非破壊** — 3 の単体ビルドで担保。
5. **描画は GLES 接続の実行時検証に依存**（別件 64bit GLES）。ロジックは並行実装可。
