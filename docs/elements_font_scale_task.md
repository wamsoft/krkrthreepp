# 作業指示: Elements のフォントスケールを layout から指定できるようにする

対象リポ: `krkrz_dev`（umbrella。engine ルート = `src/core/`。core は wamsoft/krkrz_develop）
ビルド対象 exe: WINVER `x64-windows-win`（threepp プラグインが使う版）。SDL3 版も同ソースなので両方通る想定。
※この文書は threepp 側サンプルの依存タスクの控え。実作業は krkrz_dev で行う。

## 目的
`Dialog.showDict/showJson` の layout で **フォントスケール**を指定し、`button` /
`toggle_button` / `radio_button` を含む UI 全体を大きく描けるようにする。
高解像度・大画面パネル（例: 1920×1080 のサイドパネル）でボタンが小さすぎる問題の解消。
**opt-in（既定 1.0 で従来と完全一致）**にして既存メニュー等へ影響を出さないこと。

## 現状（なぜ今できないか）
- `src/core/external/elements/external/elements_modal/src/json_layout.cpp`
  - `LayoutBuilder::build_button()`（~1125 行）は `ce::button(text)` を作るだけで
    **"size"/"size_scale" を一切読まない** → ボタン文字はテーマ固定。
  - `build_toggle_button` / `build_radio_button` も同様にサイズ未対応の見込み。
  - 一方 `build_label`（~978 行）や `invert_button`（~1594 行）/ picker は
    `resolve_font_px(o,"size","size_scale")` / `resolve_font_scale(...)`（~198〜225 行）で
    サイズを反映している。ベースは `cycfi::elements::get_theme().label_font._size`（≒14px）。
- テーマ基準サイズ `label_font._size` は `common/visual/elements/StoragesResourceLoader.cpp:477`
  で設定されるが、**TJS には公開されていない**（family のみ `Dialog.defaultFontFamily`）。

## 実装方針（推奨: top-level `font_scale`）
layout のトップレベル辞書に `"font_scale"`（float, 既定 1.0）を追加し、その dialog の
ウィジェット構築中だけテーマの基準フォントを `base * font_scale` に一時変更する。
cycfi の button/label は**構築時にスタイラへフォントを取り込む**ため、構築期間だけ
テーマを差し替えれば、その dialog の全ウィジェットが拡大サイズで作られる（構築後に復元）。

### 変更点
1. **トップレベル option のパース**（`json_layout.cpp` の overlay/session ビルド入口、
   `size`/`align`/`background`/`input`/`vars` 等を読んでいる箇所）で
   `float font_scale = 1.0` を読む（`resolve_*` と同じ `get_field`/is<double> で）。
   - 併せて `Dialog` 側から渡せるよう `common/visual/elements/DialogIntf.cpp` の
     showDict/showJson 経路に font_scale を素通しする口があると尚良い（無ければ JSON だけでOK）。
2. **ビルド期間のテーマ一時差し替え**（最小実装・推奨）:
   ```cpp
   auto& thm = const_cast<cycfi::elements::theme&>(cycfi::elements::get_theme());
   float saved = thm.label_font._size;
   if (font_scale != 1.0f) thm.label_font._size = saved * font_scale;   // ボタン系のベース
   // （button_font 等 別フィールドがあるものはそれも同様にスケール）
   ... ここで content(ツリー全体) を build ...
   thm.label_font._size = saved;   // 復元(例外時も戻すよう RAII 推奨)
   ```
   - `theme` に button 用の別フォントフィールドがあれば（`button_font` 等）それも
     同率でスケールすること。cycfi のバージョンに合わせて確認。
   - **要検証**: cycfi の `button`/`toggle_button`/`radio_button` が font を
     **構築時に取り込む**か（この方式が効く前提）。もし draw 時参照なら次の 3 を採用。
3. **代替（確実だが手数多）**: `build_button`/`build_toggle_button`/`build_radio_button` を
   `build_label` と同様に `resolve_font_px(o,"size","size_scale")` を読んで、
   そのボタンのラベル要素へ明示的にフォントサイズを設定する実装に変更。
   併せて top-level `font_scale` を各 build に既定として渡す（明示 size があればそちら優先）。

### 後方互換
- `font_scale` 省略時＝1.0＝テーマ差し替えなし＝**従来と完全一致**。他の全メニュー/ダイアログは無変更で不影響。

## 検証
- threepp の ADV サンプル `data/advsample.tjs` の `buildLayout()` の返す辞書に
  `"font_scale" => 1.6` を足して起動 → 左列パネルのボタンが 1.6 倍で描かれること。
- 起動: `krkrz64.exe D:/work/kirikiri/wamsoft_work/threepp/data -readencoding=UTF-8 -sample -replfile=<dir>`
  → `Agent.captureScreen(png)` で目視（このリポの検証フローと同じ）。
- 既存メニュー（krkrz_dev の data/ui/menu 等）を font_scale 無しで開き、見た目が変わらないこと。

## 補足（別件・engine 起因かも）
- 現状、`label` を **vtile 直下**（align_left で包んでも）に置くと描画されない事象がある
  （htile/wrow 内では出る）。font_scale とは別問題。余力があれば vtile 直下の label/align_*
  がゼロ高さで潰れていないか確認（優先度低）。

## 完了状況（2026-08-05）
- **engine 側: 完了**。`json_layout.cpp` に top-level `"font_scale"`（既定 1.0）を実装。
  推奨案（テーマ一時差替）ではなく **`effective_font_scale(o)` 方式**で実装され、
  button/toggle_button/radio_button/check_box は styler の size 引数へ、label は
  明示 size 無しなら `base*font_scale` を font_size(px) へ流す。明示 `size`/`size_scale`
  があればそちら優先。省略時（=1.0）は従来完全一致。group の `label_size`（既定 1.0）も
  タイトル倍率として使える。build 済 exe = `x64-windows-win/core/Release/krkrz64.exe`。
- **threepp 側: 完了**。`data/advsample.tjs` で `font_scale=1.3` を本採用（全11〜12
  セクションを縦 1080 に収めるため 1.4〜1.6 ではなく 1.3）。個別 `size_scale` は撤去し
  top-level 一本化。パネルを編集 IDE 風 4 ペイン（左=コマンド/中央=ビュー/右=
  インスペクター/下=ログ）へ再構成。
- **見出しは label ではなく group で実現**。下記の未解決事項のとおり単体 label が
  vtile/htile 内で潰れて描画されないため、見出しは **group（固定高 box を子にした
  タイトルバー）+ その下に兄弟としてボタン群**で表示している。

## 未解決（engine 側・優先度低）
- **単体 `label` が vtile/htile/band の中で 0 高さに潰れて描画されない**（group の子
  として直接置いた場合や button は描画される）。`build_label` 側の垂直 limits か、
  親レイアウトでの高さ配分の問題と思われる。現状は group タイトルバーで回避しているが、
  素の見出し label が使えると UI 記述が簡潔になる。余力があれば調査。
