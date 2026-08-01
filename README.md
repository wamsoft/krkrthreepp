# threepp 吉里吉里プラグイン

threepp (C++ port of three.js) を吉里吉里で使用するためのプラグインです。

## 概要

このプラグインは [threepp](https://github.com/markaren/threepp) ライブラリを ncbind を使って吉里吉里にバインディングし、TJS2 スクリプトから 3D グラフィックス機能を利用可能にします。

three.js 相当のシーングラフ/ジオメトリ/マテリアル/ライティングに加え、**VRM アバター (VRoid 等、0.x・1.0)** の読み込み・表情・視線・VRMA アニメ・揺れ物・MToon トゥーン描画に対応します。TJS2 側の擬似コード API は [`manual.tjs`](manual.tjs)、GLES 接続や描画の流れは [`CLAUDE.md`](CLAUDE.md) を参照してください。

## 実装済みクラス一覧

### Math クラス
| クラス名 | 説明 |
|---------|------|
| `Vector2` | 2次元ベクトル |
| `Vector3` | 3次元ベクトル |
| `Color` | RGB色 |
| `Euler` | オイラー角 |
| `Quaternion` | クォータニオン（四元数） |
| `Matrix3` | 3x3行列 |
| `Matrix4` | 4x4行列 |
| `Box2` | 2D軸平行境界ボックス |
| `Box3` | 3D軸平行境界ボックス |
| `Sphere` | 球 |
| `Ray` | レイ（光線） |
| `Plane` | 平面 |
| `Line3` | 3D線分 |
| `Triangle` | 三角形 |
| `Spherical` | 球面座標 |
| `Cylindrical` | 円柱座標 |

### Core クラス
| クラス名 | 説明 |
|---------|------|
| `Object3D` | 3Dオブジェクトの基底クラス |
| `BufferGeometry` | バッファジオメトリ |
| `Clock` | 時間計測 |
| `Raycaster` | レイキャスティング |

### Camera クラス
| クラス名 | 説明 |
|---------|------|
| `Camera` | カメラ基底クラス |
| `PerspectiveCamera` | 透視投影カメラ |
| `OrthographicCamera` | 正射影カメラ |

### Scene クラス
| クラス名 | 説明 |
|---------|------|
| `Scene` | シーン |
| `Fog` | 線形フォグ |
| `FogExp2` | 指数フォグ |

### Light クラス
| クラス名 | 説明 |
|---------|------|
| `Light` | ライト基底クラス |
| `AmbientLight` | 環境光 |
| `DirectionalLight` | 平行光源 |
| `PointLight` | 点光源 |
| `SpotLight` | スポットライト |
| `HemisphereLight` | 半球光源 |

### Material クラス
| クラス名 | 説明 |
|---------|------|
| `Material` | マテリアル基底クラス |
| `MeshBasicMaterial` | 基本マテリアル（ライティングなし） |
| `MeshLambertMaterial` | ランバートマテリアル |
| `MeshPhongMaterial` | フォンマテリアル |
| `MeshStandardMaterial` | PBRマテリアル |
| `MeshNormalMaterial` | 法線マテリアル |
| `MeshDepthMaterial` | 深度マテリアル |
| `MeshToonMaterial` | トゥーンマテリアル |
| `MeshMatcapMaterial` | Matcapマテリアル |
| `ShaderMaterial` | シェーダーマテリアル |
| `RawShaderMaterial` | 生シェーダーマテリアル |
| `LineBasicMaterial` | ライン用マテリアル |
| `PointsMaterial` | ポイント用マテリアル |
| `SpriteMaterial` | スプライト用マテリアル |

### Geometry クラス
| クラス名 | 説明 |
|---------|------|
| `BoxGeometry` | 直方体 |
| `SphereGeometry` | 球 |
| `PlaneGeometry` | 平面 |
| `CylinderGeometry` | 円柱 |
| `ConeGeometry` | 円錐 |
| `TorusGeometry` | トーラス |
| `TorusKnotGeometry` | トーラスノット |
| `CircleGeometry` | 円 |
| `RingGeometry` | リング |
| `IcosahedronGeometry` | 正二十面体 |
| `OctahedronGeometry` | 正八面体 |
| `CapsuleGeometry` | カプセル |
| `LatheGeometry` | 旋盤形状 |
| `EdgesGeometry` | エッジ抽出 |
| `WireframeGeometry` | ワイヤーフレーム |
| `ShapeGeometry` | 2D形状ジオメトリ |
| `ExtrudeGeometry` | 押し出しジオメトリ |

### Object クラス
| クラス名 | 説明 |
|---------|------|
| `Group` | グループ |
| `Mesh` | メッシュ |
| `Line` | ライン |
| `LineSegments` | 線分 |
| `Points` | ポイントクラウド |
| `Sprite` | スプライト |
| `InstancedMesh` | インスタンスメッシュ |
| `LOD` | Level of Detail |
| `Bone` | ボーン |
| `Skeleton` | スケルトン |
| `SkinnedMesh` | スキニングメッシュ |
| `Sky` | 空（大気散乱） |
| `Water` | 水面 |
| `Reflector` | 反射面 |

### Helper クラス
| クラス名 | 説明 |
|---------|------|
| `AxesHelper` | 座標軸ヘルパー |
| `GridHelper` | グリッドヘルパー |
| `ArrowHelper` | 矢印ヘルパー |
| `BoxHelper` | ボックスヘルパー |
| `CameraHelper` | カメラヘルパー |
| `DirectionalLightHelper` | 平行光源ヘルパー |
| `PointLightHelper` | 点光源ヘルパー |
| `SpotLightHelper` | スポットライトヘルパー |
| `PolarGridHelper` | 極座標グリッドヘルパー |
| `PlaneHelper` | 平面ヘルパー |

### Texture クラス
| クラス名 | 説明 |
|---------|------|
| `Texture` | テクスチャ |
| `DataTexture` | データテクスチャ |

### Loader クラス
| クラス名 | 説明 |
|---------|------|
| `TextureLoader` | テクスチャローダー |

### Curve クラス
| クラス名 | 説明 |
|---------|------|
| `LineCurve` | 2D直線カーブ |
| `LineCurve3` | 3D直線カーブ |
| `QuadraticBezierCurve` | 2D二次ベジェカーブ |
| `QuadraticBezierCurve3` | 3D二次ベジェカーブ |
| `CubicBezierCurve` | 2D三次ベジェカーブ |
| `CubicBezierCurve3` | 3D三次ベジェカーブ |
| `SplineCurve` | 2Dスプラインカーブ |
| `EllipseCurve` | 楕円カーブ |
| `CatmullRomCurve3` | CatmullRomスプライン曲線 |
| `Path` | 2Dパス（複数カーブの結合） |
| `Shape` | 2D形状（穴付きパス、ExtrudeGeometry用） |

### Renderer クラス
| クラス名 | 説明 |
|---------|------|
| `GLRenderer` | OpenGLレンダラー (MSAA `setSampleCount`、`onBeginScene`/`onEndScene` でホスト GL 連携) |

### VRM クラス
VRoid/VRM アバター (0.x・1.0 両対応)。読み込み・humanoid リグ・表情・視線・VRMA アニメ・
揺れ物(springBone)・制約(node_constraint)・MToon(トゥーン/輪郭線/matcap/normalMap) を実装。

| クラス名 | 説明 |
|---------|------|
| `VRMLoader` | .vrm ローダ (`load` / `loadStorage`) |
| `VRM` | アバター本体 (`getScene`/`update`/`setExpression`/`applyAnimation`/`setLookAtTarget`/`setColliderScale` 等) |
| `VRMAnimationLoader` | .vrma ローダ |
| `VRMAnimation` | VRM アニメーション (ボーン回転 + hips ルートモーション) |

グローバル関数 `textureFromLayer(layer)` — 吉里吉里 Layer を threepp テクスチャへ変換。

詳細な API は [`manual.tjs`](manual.tjs)、**VRM の機能ごとの対応状況**は
[`docs/vrm_support_status.md`](docs/vrm_support_status.md) を参照。

---

## 保留中クラス一覧

以下のクラスは依存関係や実装の複雑さから保留しています。

### Controls（操作系）
| クラス名 | 保留理由 |
|---------|---------|
| `OrbitControls` | `PeripheralsEventSource` への依存。吉里吉里のイベントシステムとの統合が必要 |
| `FlyControls` | 同上 |
| `DragControls` | 同上 |

### Geometry（複雑な依存関係）
| クラス名 | 保留理由 |
|---------|---------|
| `TubeGeometry` | `Curve3` パスを必要とする複雑な依存関係 |
| `TextGeometry` | フォントデータへの依存 |
| `ExtrudeTextGeometry` | フォントデータへの依存 |
| `ConvexGeometry` | 凸包計算の複雑さ |
| `DecalGeometry` | 複雑なUV計算 |
| `PolyhedronGeometry` | 頂点・面配列の複雑なパラメータ |
| `TetrahedronGeometry` | 未実装（需要に応じて追加可能） |
| `DodecahedronGeometry` | 未実装（需要に応じて追加可能） |

### Extras/Core（曲線・パス系）
| クラス名 | 保留理由 |
|---------|---------|
| `Curve` | 基底クラス（テンプレート）、具体的なカーブクラスを使用 |
| `CurvePath` | 基底クラス（Pathを使用） |
| `Font` | フォントデータ管理 |

### Extras/Curves（2Dカーブ）
| クラス名 | 保留理由 |
|---------|---------|
| `ArcCurve` | EllipseCurveのエイリアス（必要であれば追加可能） |

### Math（追加）
| クラス名 | 保留理由 |
|---------|---------|
| `Vector4` | 需要に応じて追加可能 |
| `Frustum` | ビューフラストラム（内部使用が主） |

### Core（内部向け）
| クラス名 | 保留理由 |
|---------|---------|
| `BufferAttribute` | 低レベルAPI、直接操作は複雑 |
| `InterleavedBuffer` | 低レベルAPI |
| `InterleavedBufferAttribute` | 低レベルAPI |
| `Uniform` | シェーダー用、複雑な型対応が必要 |
| `Shader` | シェーダー定義 |
| `Layers` | レイヤー管理 |

### Loader（追加）
| クラス名 | 保留理由 |
|---------|---------|
| `OBJLoader` | ファイルI/Oの統合が必要 |
| `MTLLoader` | 同上 |
| `STLLoader` | 同上 |
| `AssimpLoader` | Assimp依存 |
| `URDFLoader` | URDF形式特有 |
| `ImageLoader` | 吉里吉里の画像システムとの統合が有効 |
| `FontLoader` | フォントシステム |

### Helper（追加）
| クラス名 | 保留理由 |
|---------|---------|
| `Box3Helper` | 需要に応じて追加可能 |
| `HemisphereLightHelper` | 需要に応じて追加可能 |
| `SkeletonHelper` | スケルトン可視化 |

### Objects（特殊）
| クラス名 | 保留理由 |
|---------|---------|
| `Text2D` | TextGeometry依存 |
| `Text3D` | ExtrudeTextGeometry依存 |

### Animation
| クラス名 | 保留理由 |
|---------|---------|
| `AnimationClip` | アニメーションシステム全体の実装が必要 |
| `AnimationMixer` | 同上 |
| `AnimationAction` | 同上 |
| `KeyframeTrack` | 同上 |

### Audio
| クラス名 | 保留理由 |
|---------|---------|
| `Audio` | オーディオシステム（吉里吉里に既存の機能あり） |
| `AudioListener` | 同上 |
| `PositionalAudio` | 同上 |

---

## 使用例

```tjs
// シーンとカメラの作成
var scene = new Threepp.Scene();
var camera = new Threepp.PerspectiveCamera(75, 16/9, 0.1, 1000);
camera.position.z = 5;

// ジオメトリとマテリアルの作成
var geometry = new Threepp.BoxGeometry(1, 1, 1);
var material = new Threepp.MeshPhongMaterial();
material.color = 0x00ff00;

// メッシュを作成してシーンに追加
var mesh = new Threepp.Mesh(geometry, material);
scene.add(mesh);

// ライト追加
var light = new Threepp.DirectionalLight(0xffffff, 1.0);
light.position.set(1, 1, 1);
scene.add(light);

var ambient = new Threepp.AmbientLight(0x404040);
scene.add(ambient);

// レンダラー作成と描画
var renderer = new Threepp.GLRenderer(800, 600);
renderer.setClearColor(0x000000);
renderer.render(scene, camera);
```

## ビルド要件

- C++17 以上
- threepp ライブラリ
- 吉里吉里 SDK (tp_stub, ncbind)

## ライセンス

threepp は MIT ライセンスで提供されています。
