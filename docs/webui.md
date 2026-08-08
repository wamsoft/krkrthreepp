# ブラウザ編集 UI (webui) — アーキテクチャと拡張ガイド

エンジンの `-replweb` HTTP+SSE サーバに乗る、VRM 立ち絵/箱庭編集のブラウザ UI。
Elements 版の編集パネルは撤去済みで(2026-08)、**本体ウィンドウは 3D 表示エリアと
ゲーム内 UI(視線ターゲット等)のみ**、編集・インスペクター・操作ログ・REPL は
すべてブラウザ側に統合されている。

## 起動

```
krkrz64.exe <ABS data> -readencoding=UTF-8 -sample -replweb=8899 -webui
```

| フラグ | 説明 |
|---|---|
| `-replweb[=port]` | HTTP+SSE サーバ起動(エンジン機能。既定 127.0.0.1:8899) |
| `-sample` | advsample(立ち絵2体のADVサンプル)を起動。無指定は単体ビューア |
| `-webui` | 編集 UI をブラウザの**アプリモード**(タブ/アドレスバー無し)で自動オープン |

手動で開く: ブラウザで `http://127.0.0.1:8899/ui/`、またはゲーム画面で **Tab**、
または REPL から `webui.openBrowser()`(Edge → Chrome `--app=` → 既定ブラウザの順)。

## 構成(4層)

| 層 | ファイル | 役割 |
|---|---|---|
| エンジン | `krkrz core/common/utils/ReplWebServer.cpp` + `ReplWebIntf.cpp` | HTTP+SSE サーバと **TJS クラス `WebServer`**(register/serveStatic/broadcast)。ハンドラは常にメインスレッド実行。詳細は engine の `doc/REPL.md` |
| プラグイン | `main.cpp` の `ThreeWebApi` ほか | `/api/three/` をネイティブハンドラで公開(リンク時に自動配線)。UI 向け一括 JSON ダンプ `Object3D.treeJson()` / `VRM.expressionListJson()` / `VRM.morphListJson()` |
| アプリブリッジ | `data/webui.tjs` (`WebUIBridge`) | advsample のモデル層を `/app/…` ルートへ橋渡し + `/ui/` 静的配信 + SSE `state` チャネル配信 |
| ブラウザ UI | `data/ui/index.html` | 単一ページ(ビルドレス vanilla JS)。作業モード連動ペイン + サイドバー + REPL コンソール |

## 通信

- **client → server**: `fetch POST`(body=JSON)。スライダは 50ms throttle(キー単位で最新値のみ)。
- **server → client**: SSE `/sub/state` に状態 JSON を push。
  - 各操作ルートの直後(`push()`)
  - 本体フレームループから `webui.tick(fps)` が **~4Hz で差分検知 push**
    (REPL 等 UI 外からの変更もブラウザに自動反映される)
- **REPL コンソール**: エンジン組み込みの `/events`(ログ SSE)+ `/cmd`(TJS 評価)を
  そのまま利用。エンジン変更なしで UI に統合されている。

## ルート一覧

### プラグイン(ネイティブ、main.cpp)
| ルート | 説明 |
|---|---|
| `GET /api/three/ping` | 疎通確認 `{"ok":true,"plugin":"krkr_threepp"}` |

### アプリ(webui.tjs)
| ルート | body | 説明 |
|---|---|---|
| `GET /app/state` | — | 全編集状態 JSON(下記スキーマ) |
| `POST /app/workmode` | `{mode}` | 作業モード切替 (op/face/pose/seq/char) |
| `POST /app/op/panel` | `{id, val?}` | **旧 Elements パネルと同一 id の汎用パススルー**(`app.onPanel` へ委譲)。表情/モーション/シーン/カメラ/フレーム/トグル等すべて |
| `GET /app/face/morphs` | — | 対象 VRM の全モーフ名(詳細モード用) |
| `POST /app/face/morph` | `{name, w}` | 生モーフのライブ反映(push なし) |
| `POST /app/face/clear · save · load` | `{id}` | クリア / faces.json 保存 / 読込 |
| `POST /app/pose/base` | `{kind:"capture"/"rest"}` | 基準姿勢(採取 / 素立ち) |
| `POST /app/pose/bone · rot · hips` | `{name}` / `{axis,deg}` / `{axis,m}` | 関節選択 / 回転 / 腰移動(rot·hips は push なし) |
| `POST /app/pose/save · load` | `{id}` | poses.json 保存 / 読込 |
| `POST /app/seq/add · select · move · del · clear` | `{id}` / `{i}` / `{dir}` | 手順操作 |
| `POST /app/seq/hold · trans · expr` | `{sec}` / `{sec}` / `{name}` | 選択手順の保持/遷移/表情 |
| `POST /app/seq/preview · stop · export` | `{file?}` | プレビュー / 停止 / VRMA 書出 |
| `POST /app/char/select · assign · register` | `{id}` / `{slot,id?}` / `{id,name,vrm,frontZ}` | キャラ選択 / スロット割当 / 新規登録 |

## state スキーマ(概要)

```jsonc
{
  "ready": true,            // false = advsample 未起動 (ビューアモード)
  "workMode": "op",
  // 表情:  editName / morphs{名:w} / faces[] / groups[{title,items[{name,label}]}]
  // ポーズ: pose{bones[],editBone,euler[3],hips[3],poses[],poseName}
  // 連番:  seq{steps[{pose,hold,trans,expr}],sel,emotes[]}
  // キャラ: chars{list[],slots[],sel,slotNames[]}
  // 操作:  op{target,mode,exprs[],motions[],scenes[],blink,look,idle,gaze,camMode,bgFollow,fov}
  // 観測:  insp{fps,chars[{name,selected,expr,motion,look,blink,idle,frame,x,z}]}
  //        log[]  (操作ログリング)
}
```

UI 側は `insp`/`log` を除いた部分が変わったときだけエディタ DOM を再構築する
(4Hz の観測更新で入力・スライダを壊さないため)。

## 拡張の手引き

**アプリ機能を足す**: webui.tjs の `handler` にルートを足し(操作後は `push()`)、
`buildState` に必要な状態を足し、index.html にペイン/部品を足す。op 系の単発
ボタンなら `app.onPanel` に id を足して `/app/op/panel` パススルーに乗せるのが最短。

**プラグインのネイティブ API を足す**: `ThreeWebApi_handle`(main.cpp)の path
分岐に追加(nlohmann::json で生成し `ThreeUtf8ToTtstr` で返す)。重い一括データは
専用メソッド(treeJson 方式)にして TJS ハンドラから呼ぶ形でもよい。

**別アプリで使う**: `WebServer.serveStatic` + `register` を呼ぶ TJS を書けばよい。
webui.tjs は advsample 専用ブリッジだが、`/ui/` 配信と WebServer API 自体は汎用。

## ハマりどころ(実装時に踏んだもの)

- **TJS2 にローカル関数のレキシカルクロージャは無い** — IIFE 内の関数同士の相互
  参照は実行時「メンバが見つかりません」。クラス(WebUIBridge)のメンバにして
  objthis 経由で解決する。
- **TJS2 のブロックコメントはネスト可能** — コメント内の `/ui/` + `*` の並びが
  `/*` と誤認され「コメントが終わらないまま終端」。webui.tjs は行コメントのみ。
- **startup スクリプト中の例外は致命** — REPL 保護(TVPReplActive)は
  TVPCreateREPL 後に立つため、webui.tjs のロード失敗はプロセスごと落ちる。
- **json.dll** — /app/ ブリッジは `Scripts.toJSONString/evalJSON` が必要。
  webui.tjs が未リンクなら自力で `Plugins.link("json.dll")` を試み、不可なら
  `/ui/` 配信のみ登録する。
