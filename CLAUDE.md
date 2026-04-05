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

The plugin has three layers:

1. **threepp** (git submodule, `threepp/`): Pure C++ 3D library (fork at `wtnbgo/threepp`, branch `work_sdl3`). Uses `THREEPP_WITH_GENERIC` mode for custom rendering integration.
2. **ncbind** (sibling directory `../ncbind`): C++ template library that bridges native C++ classes to Kirikiri's TJS2 scripting engine.
3. **main.cpp**: The binding layer that registers 70+ threepp classes with ncbind macros for TJS2 access.

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

- ビルド成功（Debug）
- GLES 接続コード（threepp の Generic frontend と吉里吉里の Layer を繋ぐ部分）は未実装 — 別途資料作成予定

## Build Configuration

- C++17 standard, MSVC static runtime on Windows, `/bigobj` 有効
- Ninja Multi-Config generator
- vcpkg for dependency management (static linking)
- threepp options: SVG loader ON, Audio OFF (Kirikiri has its own), Generic frontend ON
