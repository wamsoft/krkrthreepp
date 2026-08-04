# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

**krkrthreepp** is a Kirikiri Z plugin that exposes the [threepp](https://github.com/markaren/threepp) (C++ port of three.js) 3D graphics library to TJS2 scripting via the ncbind binding framework. The project is written in C++ and documentation is in Japanese.

## Build Commands

Requires `VCPKG_ROOT` environment variable to be set. **VSLANG=1033 を付けること（エラーメッセージを英語にする）。**

```bash
# Configure (run once or when CMake changes)
VSLANG=1033 cmake --preset x64-windows

# Build (Debug, default)
VSLANG=1033 cmake --build build/x64-windows/

# Build (Release)
VSLANG=1033 cmake --build build/x64-windows/ --config Release
```

Available presets: `x86-windows`, `x64-windows`, `x64-linux`, `x64-osx`, `arm64-osx`, `arm64-android`, `x64-android`.

No test suite is built for the plugin itself (threepp's tests are disabled via CMake options).

## Architecture

The plugin has four layers:

1. **threepp** (git submodule, `threepp/`): Pure C++ 3D library (fork at `wtnbgo/threepp`, branch `work_sdl3`). Uses `THREEPP_WITH_GENERIC` mode for custom rendering integration. GLES 化・GLTFLoader 露出・MToon 用の追加あり。
2. **ncbind** (sibling directory `../ncbind`): C++ template library that bridges native C++ classes to Kirikiri's TJS2 scripting engine.
3. **vrm** (`vrm/VRM.cpp` `vrm/VRM.hpp`): threepp の上に構築した VRM ランタイム (0.x/1.0 読込・humanoid・表情・視線・VRMA・springBone・node_constraint・MToon)。ncbind 非依存の純 C++。詳細は [`docs/vrm_support_status.md`](docs/vrm_support_status.md)。
4. **main.cpp**: The binding layer that registers 100+ threepp/VRM classes with ncbind macros for TJS2 access.

### Key Binding Patterns in main.cpp

- `NCB_REGISTER_SUBCLASS()` — value-type classes with Boxing converter (Euler, Box3, etc.)
- `NCB_REGISTER_SUBCLASS_DELAY()` — value-type classes with custom converters (Vector2, Vector3, Color のみ — カスタム DST コンバータを持つため)
- `NCB_REGISTER_THREE_SUBCLASS()` — wrapper classes for threepp objects with shared_ptr lifecycle via `ThreeWrapper<T>`
- `NCB_METHOD_PROXY()` — static wrapper functions for complex operations that need type conversion
- `NCB_METHOD_VOID*()` / `NCB_THREE_METHOD_VOID*()` — T& を返すチェーンメソッドを void として登録するマクロ群
- Type converters: `std::string`, `Vector2`, `Vector3`, `Color` にカスタムコンバータ、各種 enum に INTEGER コンバータ

### ncbind の制限と対策パターン

- **`const T&` 戻り値**: proxy 関数で値コピーを返す（Box3::min(), Line3::start() 等）
- **`T&` チェーン戻り値**: `NCB_METHOD_VOID*` マクロで void として登録
- **`std::optional<T>` 引数**: proxy 関数で optional なしのオーバーロードを呼ぶ
- **`std::pair`, `std::wstring`**: proxy 関数で ncbind 対応型に変換
- **Bridge + `(int)0` の曖昧性**: `NCB_THREE_PROPERTY_RO` / `NCB_THREE_MEMBER_PROPERTY_RO` は `getBridgeType()` を使用して解消

### Other Key Files

- `layerExBase.hpp` — Base class for integrating threepp rendering into Kirikiri's layer system (image buffer access)
- `manual.tjs` — Pseudo-code API documentation for TJS2 script developers
- `README.md` — Japanese documentation with class listing and usage examples

## Current Status

64bit 実機 (krkrz64 + `../gles` の GLESAdaptor) で threepp 描画と VRM が一通り動作する状態。
ビルドは Release / Debug 成功。

### 描画基盤 (実機検証済み)
- **threepp を GLES 化済み** (fork): glad を desktop GL → GLES(glad2) へ差替え、シェーダを
  `300 es` 化 (`THREEPP_GLES`)。当初の「ANGLE GLES 上で desktop glad が解決しない」問題は解消。
- **捕捉 FBO へ描画**: threepp に `setDefaultFramebuffer*` を追加し、`adaptor.capture()` が bind した
  中間 FBO へ描く。`GLRenderer` は `onBeginScene(w,h)` → `render(scene,camera)` → `onEndScene()`。
- **MSAA** (`setSampleCount`) + gles.dll の `unpremultiply` で縁の AA と premultiplied 補正。
- 合成は `dest.type = ltAlpha`。実装は `main.cpp` の `GLRenderer*` 群。

### VRM (`vrm/VRM.cpp` `vrm/VRM.hpp`)
VRM 0.x / 1.0 の読込・humanoid・表情(morph/override/textureTransformBind)・視線(bone/expression)・
VRMA アニメ(+hips ルートモーション)・揺れ物(springBone, center 空間)・制約(node_constraint)・
**MToon**(lit/shade・輪郭線・matcap・normalMap・UVアニメ) を実装。three-vrm / vrm-specification を
参照に実装・検証。**機能ごとの詳細な対応状況は [`docs/vrm_support_status.md`](docs/vrm_support_status.md)**。
未対応の主なもの: materialColorBind / VRMA 顔トラック / FirstPerson / scene ライティング(固定ライト)。

### リポジトリ
- 親 (`.`): VRM 実装 + ncbind バインド (`main.cpp`)。
- threepp fork (`threepp/`, branch `work_sdl3`): GLES 化 / GLTFLoader 露出・修正 / MToon 用の
  ShaderMaterial・MeshToonMaterial mixin 追加 / **morph テクスチャ化**(全モーフを RGBA32F
  テクスチャに詰め `gl_VertexID`+`texelFetch` で走査。旧 attribute 8 枠方式の「同時8モーフ」
  上限を撤廃) 等。**push は force 必要 (rebase 履歴)、未 push**。
  - ⚠ **シェーダ chunk (`src/shaders/**/*.glsl`) を編集したら再 configure 必須**:
    `cmake/embed.cmake` が configure 時に `.glsl` を READ して生成ヘッダ (`build/*/generated/`)
    へ埋め込む。`cmake --build` だけでは古い生成ヘッダが使われ、コンパイル失敗が**黙殺**され
    描画破綻になる (旧 `createShader` は `GL_COMPILE_STATUS` 未チェックだった→報告するよう修正済)。
- ncbind (`../ncbind`): 単一継承(is-a)拡張 + CreateAdaptor 祖先 attach。
- gles (`../gles`): git 管理外。`unpremultiply` 追加分はソース保存 + DLL を plugin64 に配置。

## Build Configuration

- C++17 standard, MSVC static runtime on Windows, `/bigobj` 有効
- Ninja Multi-Config generator
- vcpkg for dependency management (static linking)
- threepp options: SVG loader ON, Audio OFF (Kirikiri has its own), Generic frontend ON
